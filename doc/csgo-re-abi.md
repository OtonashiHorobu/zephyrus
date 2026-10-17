# zephyrus-csgo Linux ABI / vtable Reverse-Engineering Verification

> Goal: record the calling conventions, vtable slots and struct offsets that have been
> verified with Ghidra + core dumps on **legacy CS:GO 1.38.8.1 linux64** (`client_client.so` /
> `engine_client.so` / `vphysics_client.so`). Each entry gives the module offset, the evidence
> (function/instruction), the confidence and the corresponding zephyrus code path.
>
> Related: [csgo-port-plan.md](csgo-port-plan.md) · [csgo-re-f3.md](csgo-re-f3.md) (request
> list for the animation/prediction/network TODO items) · [todo-csgo.md](todo-csgo.md)
>
> Address convention: Ghidra address = ELF vaddr + 0x100000; unless stated otherwise, this
> document uses **vaddr (module offset)**. Ghidra project `csgo`
> (`/datassd/Clone/acr-bin/ghidra/csgo`) programs:
> `/client/client_client.so`, `/engine/engine_client.so`, `/vphysics/vphysics_client.so`.
> Runtime evidence: `coredumpctl debug <pid>` / `gdb` reading core memory directly.

---

## 1. Calling Convention Rules (Lessons Learned the Hard Way)

Source engine "reference parameters" are **pointers** under the SysV ABI. If the C++ side
passes a large struct (`Ray_t`, `trace_t`, `Vector` and other MEMORY-class aggregates) by
**value** to `virtual_call`, the compiler places them on the stack, leaves garbage in the
registers, and the engine side then dereferences them as pointers → SIGSEGV.

zephyrus's fix (`include/zephyrus/csgo/entity.hpp`): dedicated typed wrappers
`engine_trace_ray()` / `engine_point_contents()` were written for `IEngineTrace`, with the
signature spelling out pointers explicitly:

```cpp
using fn_type = void (*)(void *, const ray &, unsigned int, trace_filter *, trace *);
```

**All** cross-module virtual calls must be written this way; do not pass aggregate types
directly to `virtual_call` again.

---

## 2. `IEngineTrace` (`EngineTraceClient004`, engine_client.so)

### 2.1 vtable and Slots

vtable measured on a runtime core (inside engine_client.so):

| Slot | vaddr | Signature (measured) | Notes |
|---|---|---|---|
| 0 | `0x5462e0` | `int GetPointContents(long* this, const Vector* pos, uint mask, void** ppEntity)` | all parameters are pointers |
| 5 | `0x547290` | `void TraceRay(long* this, const Ray_t* ray, uint mask, ITraceFilter* filter, trace_t* trace)` | all parameters are pointers |

Key instruction evidence (`0x547290`):

```asm
; TraceRay entry: rdi=this, rsi=ray*, edx=mask, rcx=filter*, r8=trace*
mov  rdi, r8            ; -> FUN_004fcdc0(trace)
call 0x3fcdc0           ; 0x3fcdc0: MOV ECX,0xd; XOR EAX,EAX; REP STOSQ -> clears 13*8=0x68 bytes
...
mov  (%r12),%rax
call *0x8(%rax)         ; filter->vtable[1]() result compared against 1/2 -> slot1 = GetTraceType
...
call *(%rax)            ; filter->vtable[0](entity, mask) -> slot0 = ShouldHitEntity
```

Conclusions:

* `trace_t` is **0x68 (104 bytes)** in size; the engine zeroes the whole block before every
  TraceRay. Extra corroboration: `+0x36` = `all_solid`, `+0x37` = `start_solid` (consistent
  with the `Trace` struct).
* `Ray_t`: `m_IsRay` is at **+0x48** (the engine reads `*(char *)(ray + 0x48)`);
  `m_Start` actually occupies 16 bytes (`float pad`), and `m_Delta` starts at +0x10.
* `ITraceFilter`: slot 0 = `ShouldHitEntity`, slot 1 = `GetTraceType`,
  **no virtual destructor**. zephyrus once added `virtual ~trace_filter()`, which inserted
  two destructor slots at slot 0/1, so the engine's `GetTraceType` call actually landed on
  the destructor (`delete` on a stack object) — removed.

### 2.2 Crash Evidence (Before the Fix)

```
2026-10-10 11:56 core:
#0 engine_client.so+0x3fcdcb   ; rep stos %rax,(%rdi), rdi=0x1
#1 engine_client.so+0x44730a
#2 zephyrus::csgo::virtual_call<5, void, ray, uint, trace_filter_skip, trace>
#3 zephyrus::csgo::entity::is_visible
#4 zephyrus::csgo::features::rage::run
```

`trace*` never reached R8 because it was passed by value, so R8=1 was taken as the trace
pointer → the zeroing wrote to 0x1. After rewriting it as a pointer, all `rage`/ESP/autowall
traces work normally (verified on a live game: the `trace` command returns sensible
`fraction/endpos/hitgroup` values).

---

## 3. `IPhysicsSurfaceProps` (`VPhysicsSurfaceProps001`, vphysics_client.so)

### 3.1 vtable (measured on a runtime core; on the object `this+0x170`=table pointer, `+0x180`=count)

| Slot | vaddr | Function |
|---|---|---|
| 0/1 | `0x3f530` / `0x3f640` | destructor pair |
| 2 | `0x3f9b0` | `ParseSurfaceData` |
| 3 | `0x3eec0` | `SurfacePropCount()` → `return *(int*)(this+0x180)` |
| 4 | `0x3f180` | `GetSurfaceIndex(const char*)` |
| 5 | `0x3eed0` | `GetPhysicsProperties(index, float* friction, float* elasticity, float* density, float* thickness)` |
| **6** | `0x3f000` | **`GetSurfaceData(index)`** |

Disassembly highlights of `GetSurfaceData` (`0x3f000`):

```asm
cmp esi, 0x7f / 0xf000  ; special index handling
mov eax, [rdi+0x180]    ; count
...
mov rdx, [rdi+0x170]    ; table
lea rax, (%rsi,%rsi,4)
lea rax, (%rsi,%rax,2)  ; rax = 11*index
shl rax, 4              ; * 16 -> stride 0xB0 (176)
add rax, rdx            ; entry
...
add rax, 0x14           ; return entry + 0x14
ret
```

> **Osiris's Linux index 5 is wrong** (it may be the same on Windows, but slot 5 in this
> build is `GetPhysicsProperties`, which takes 4 output pointer parameters). Using it as
> `GetSurfaceData` writes garbage registers as output pointers — the 2026-10-10 12:32 core is
> exactly this crash (`rdi=this`, `rdx=0x40`, writing `movss %xmm0,(%rdx)`).

### 3.2 `surfacedata_t` Layout (measured at runtime)

Decoding table entry 64 (`table=0x33fb2000`, entry 64 @ `0x33fb4c00`):

| Relative to the `GetSurfaceData()` return pointer | Value | Field |
|---|---|---|
| +0x00 | 0.8 | friction |
| +0x04 | 0.01 | elasticity |
| +0x08 | 1600.0 | density |
| +0x0C | 0.0 | thickness |
| +0x50 | 1.0 | `maxSpeedFactor` |
| +0x54 | 1.0 | `jumpFactor` |
| +0x58 | 0.6 | `penetrationModifier` |
| +0x5C | 0.6 | `damageModifier` |
| +0x60 (u16) | 2 | `material` |
| +0x62 (u8) | 0 | `climbable` |

The table entry also contains a pointer at `+0x00`, a count at `+0x10`, sound indices (a short
array) starting at `+0x4c`, and so on. zephyrus's `struct surface_data { std::byte pad0[80]; ... }`
matches this measured layout.

### 3.3 Corresponding Code

* `src/zephyrus/csgo/game.cpp`: `surface_props()` calls slot **6**.
* `src/zephyrus/features/csgo/autowall.cpp`: reads of `penetration_modifier` / `material`.

---

## 4. `MD5_PseudoRandom` and Weapon Spread (client_client.so)

Feature conclusion: **CS:GO cannot do no-spread** (the server owns the random numbers for
bullet spread), so zephyrus removed the feature; the notes below are RE reference material
only.

* `MD5_PseudoRandom` @ **vaddr 0x1346b30**:

```asm
1346b30: ...                       ; build the MD5 context
1346b5f: lea -0x84(%rbp),%rsi      ; input = &command_number
1346b8b: call MD5Update(ctx, &seed, 4)
1346b97: call MD5Final(digest @ rbp-0x80, ctx)
1346b9c: mov -0x7a(%rbp),%eax      ; returns *(uint32*)(digest + 6)  ← not digest+0
```

* Caller `CCSWeaponBase::UpdateSpread` @ **vaddr 0xfd5c10**:
  `RandomSeed(*(int*)(this+0x558))` (**no `&0xFF` mask**), followed by 4 `RandomFloat`
  calls: cached to `this+0x568 / +0x574 / +0x580 / +0x55C` for this frame's spread/recoil
  calculations.

---

## 5. `CHLClient` (`VClient018`, client_client.so)

* CreateInterface registration chain: `_INIT_119` → `InterfaceReg(&DAT_024b27b0, FUN_00985dd0, "VClient018")`
  → `FUN_00985dd0()` returns the instance `&DAT_024a9b60`; the constructor writes
  `*param_1 = &PTR_FUN_021c12c8` → **vtable base Ghidra 0x021c12c8**.
* Slots measured (Ghidra addresses):

| Slot | Address | Function | Notes |
|---|---|---|---|
| 37 | `0x0098bf80` | `FrameStageNotify(this, int stage)` | `switch(stage)` 0..8, writes `_DAT_023f7cf8` |
| 38 | `0x00986dc0` | `DispatchUserMessage` thunk | saves arguments → `call 0x00b25fd0` (fetch sub-object) → `jmp 0x00b25660` |

Conclusion: **Linux indices 37/38 match Osiris**. zephyrus's `client_mode_hooks` uses them
correctly (`frame_stage_notify`, `dispatch_user_message`).

Other ClientMode/Client slots (taken from Osiris Linux and already used by zephyrus):
`shouldDrawFog 18`, `overrideView 19`, `createMove 25`, `getViewModelFov 36`,
`doPostScreenEffects 45`.

---

## 6. `VEngineClient014` (engine_client.so)

* Registration chain: `_INIT_18` → `FUN_00751010(&DAT_00f11590, FUN_00378500, &DAT_00a3ad2f)`
  (string `VEngineClient014` @ file 0x93ad2f) → `FUN_00378500` returns the instance
  `&DAT_00f115a8`; `*DAT_00f115a8 = &PTR_FUN_00e64038` → vtable base Ghidra 0x00e64038.
* Slot 8 (`0x00e64078`) = `FUN_003796e0` = `GetPlayerInfo(int ent_num, void* out)`:
  * Copies **0x158 bytes** from the engine's player info source to `out`; zeroes 0x158 bytes
    on failure.
  * `player_info_t` layout: `version +0x00`, `xuid +0x08`, **`name +0x10`**,
    `userID +0x90`, `guid +0x94`, `friendsID +0xB8`, `friendsName +0xBC`,
    `fakeplayer +0x13C`, `hltv +0x13D`, `customFiles +0x140`, `filesDownloaded +0x150`,
    total size **0x158**.
* Other slots in use (Osiris Linux values that zephyrus relies on): `getViewAngles 18`,
  `setViewAngles 19`, `getMaxClients 20`, `worldToScreenMatrix 37`,
  `getLevelName 53`, `getNetworkChannel 78`; `NetworkChannel::getLatency = 9`.

---

## 7. `C_CSWeaponBase` (client_client.so)

Method: scan the `.data.rel.ro` vtable run of 556+ slots via RTTI, then disassemble candidate
slots to confirm their semantics.

| Slot | vaddr | Function | Evidence |
|---|---|---|---|
| 551 | `0x10182f0` | `GetInaccuracy()` | reads +0x3bb4 etc., returns a float in xmm0 |
| 552 | `0x101d770` | `GetSpread()` | reads `gpGlobals->curtime` (`PTR_DAT_023f7d28+0x10`) and weapon data, returns a float |

zephyrus `entity::inaccuracy()/spread()` uses 551/552, and `hitchance` models on top of both.

---

## 8. Struct Sizes and static_assert (Already in the Source)

`include/zephyrus/csgo/entity.hpp`:

```cpp
static_assert(offsetof(ray, is_ray) == 72, "Ray_t layout changed");
static_assert(sizeof(trace) == 0x68, "CGameTrace size changed");
static_assert(offsetof(trace, all_solid) == 54 &&
              offsetof(trace, start_solid) == 55, "CGameTrace flags moved");
```

`animation_layer`: stride 0x48 (72B), `order +0x18`, `weight +0x24`, `cycle +0x30`
(already recorded in `csgo-re-f3.md` A8).

`user_cmd`: matches the SDK, base part 0x50; the `viewangles_backup/buttons_backup` that
zephyrus appends only read engine data and never write back to it (`static_assert(sizeof==0x60)`).

---

## 9. Wrong Index Summary (Fixed This Round)

| Item | Mistake | Fix |
|---|---|---|
| `IPhysicsSurfaceProps::GetSurfaceData` | Slot 5 (actually `GetPhysicsProperties`) → crash | Changed to slot 6 (verified with core + disassembly) |
| `IEngineTrace::TraceRay/GetPointContents` | Aggregates passed by value → garbage registers → crash | typed wrapper with explicit pointers (`entity.hpp`) |
| `ITraceFilter` | An extra `virtual ~trace_filter()` shifted the slots | Removed the virtual destructor |
| `MD5_PseudoRandom` | Read `digest+0` (actually `digest+6`) | Removed the feature entirely (CS:GO cannot no-spread) |
| `CHLClient` 37/38 | Was suspected at one point; measured correct | Kept |
| `ClientMode` 18 (no-fog) | Stub `return 1`, used as `ShouldDrawFog` | Hook and feature removed (it had made the viewmodel disappear) |
| `ClientMode` 45 (no-scope) | Skipping `DoPostScreenEffects` does not hide the scope overlay | Hook and feature removed |
| `SetAbsOrigin` pattern | Applying it in backtrack wrote enemy origins to historical positions and never restored them | Removed the write-back; only `cmd.tick_count` is changed (fixed the ESP ghost boxes) |
| `line_goes_through_smoke` / `is_other_enemy` | Osiris patterns, not verified | Removed; team checks now use the team number (vtable 128, verified) |
| glow module (`glowObjectManager` pattern) | Not verified, crashed as soon as it was enabled | Module, pattern and UI all removed |
| prediction module | Its `client_state` / `restoreEntityToPredictedFrame` dependencies were never resolved (always null) | Module removed (including the m_nPrediction* pattern) |
| entity 348 (`eye_position`) | Not verified; it actually takes arguments (RSI/RDX), calling it with none reads garbage | Changed to the netvar computation `origin + m_vecViewOffset[0]` |
| entity 409 / 218 | Not verified / base class `return 0` | Removed (unused) |
| `UpdateClientSideAnimation` hook | Only served the unimplemented fake matrix | Removed |

**Lesson**: not all Osiris Linux vtable indices can be trusted (at least `IPhysicsSurfaceProps`
is wrong). Any virtual call with a complex signature/output pointer must be re-checked against
a core or Ghidra once before shipping.

---

## 11. RE Audit: Slot-by-Slot Verification Log (2026-10-10)

### 11.1 `ClientModeCSNormal`

* Static instance `0x06e04200` (`_INIT_438` @ `0x007d62b2` calls the constructor);
  the global slot `DAT_06e078e8` is written with `&DAT_06e04200` by `FUN_00ff6cc0`, and
  `CHLClient::vtable[10]` (`0x009862b0`) reads that global.
* Constructor `0x00ff8b20` writes vtable **`0x022d14b0`** (plus the sub-object tables
  `[+8]=0x22d16b0`, `[+0x50]=0x22d16e0`, `[+0x58]=0x21cfa10`).
* Disassembly check (`0x022d14b0`):

| Slot | vaddr | Signature (measured) | Conclusion |
|---|---|---|---|
| 14/17/18/29 | `0x009ab530` | `PUSH RBP; MOV EAX,1; RET` | default predicate `return true`, **not** `ShouldDrawFog` |
| 19 | `0x009ad1b0` | `(this[ignored], void* setup)` fetches the singleton, then forwards | `OverrideView(CViewSetup*)`, kept |
| 25 | `0x009aba30` | saves `xmm0`(float) and `rsi`(ptr), then forwards | `CreateMove(float, CUserCmd*)`, kept |
| 36 | `0x009ae060` | forwards with no extra arguments | `GetViewModelFov()`; feature unproven, hook removed |
| 45 | `0x00ff6f40` | `(this, param)` calls `FUN_009aefb0`→`FUN_00a17fd0`→`FUN_00a1a990` | `DoPostScreenEffects` (glow rendering); hook removed (no-scope ineffective) |

### 11.2 `CHLClient` (`VClient018`)

vtable `0x021c12c8` (constructor `_INIT_119`→`0x00985dd0`):

| Slot | vaddr | Function |
|---|---|---|
| 37 | `0x0098bf80` | `FrameStageNotify(this, int)` (`switch(stage)` 0..8) |
| 38 | `0x00986dc0` | `DispatchUserMessage` thunk (saves arguments→`0x00b25fd0`→`0x00b25660`) |

### 11.3 `C_CSPlayer` Object Layout (constructor `0x00fd3740`)

```
[obj+0x00] = 0x22c7b00  main vtable (C_CSPlayer)
[obj+0x08] = 0x22c89b0  IClientRenderable sub-object vtable
[obj+0x10] = 0x22c8b30  IClientNetworkable sub-object vtable
```

| Table | Slot | vaddr | Semantics (decompiler evidence) |
|---|---|---|---|
| main | 12 | `0x008c1ef0` | `GetAbsOrigin` → `this+0xD8` (calls the network update first) |
| main | 128 | `0x008bbe80` | team number → `*(int*)(this+0x12C)` |
| main | 167 | `0x00a1ade0` | health → `*(int*)(this+0x138)` |
| main | 208 | `0x0084fe20` | `IsAlive` → `*(char*)(this+0x297) == 0` |
| main | 210 | `0x008fb5a0` | `IsPlayer` → `return 1` |
| renderable | 13 | `0x00fe2620`→`0x00fe25d0` | `SetupBones(matrix*, max, mask, time)` (forwards `0x008a7ba0`) |
| networkable | 9 | `0x008bbf30`→`0x008bbf00` | `IsDormant` → reads `this+0x125` when index!=-1 |
| networkable | 10 | `0x008bbae0`→`0x008bbad0` | `GetIndex` → `*(int*)(this+0x94)` |
| weapon table | 551/552 | `0x10182f0`/`0x101d770` | `GetInaccuracy`/`GetSpread` (§7, weapon names measured correct) |

### 11.4 `VEngineClient014` (vtable `0x00e64038`)

| Slot | vaddr | Verified semantics |
|---|---|---|
| 8 | `0x00e64078` | `GetPlayerInfo(int, void*)`, copies 0x158 bytes |
| 18 | `0x003798f0` | `GetViewAngles(Vector*)` (writes 3 floats) |
| 19 | `0x00379a00` | `SetViewAngles(const Vector*)` (includes a NaN check) |
| 20 | `0x003794f0` | `GetMaxClients()` → `CClientState+0x420` |
| 37 | `0x00378670` | `WorldToScreenMatrix()` thunk (projection measured correct) |
| 78 | `0x003795b0` | `GetNetworkChannel()` → `CClientState+0x128` |

### 11.5 Local Player Finder

`83 FF FF 48 8B 05` has a **unique** hit in `client_client.so` at `0x008f4e11`:
`cmp edi,-1; mov rax,[rip+disp]` → the global `DAT_024847e8`;
`FUN_008f4e10(index)` is just `(&DAT_024847e8)[index]`, and
`ClientModeCSNormal::CreateMove` (slot 25) obtains the local player with `FUN_008f4e10(-1)`.

### 11.6 Kept but Only Behaviour-Verified (not re-decompiled)

* weapon vtable 523/529 (weapon names and per-class config measured correct).
* `VClientEntityList003` slot 3 (ESP/rage entity resolution measured correct).
* `VEngineClient014` slot 37 (projection measured correct).

### 11.7 Removal List (This Round)

The no-fog / no-scope / viewmodel FOV hooks and UI, the whole glow module, the whole
prediction module, the `DispatchUserMessage` hook (trace logging only), the
`UpdateClientSideAnimation` hook, `SetAbsOrigin`, `line_goes_through_smoke`,
`is_other_enemy`, `view_render_beams`, `weapon_system`, `player_resource_slot`,
`planted_c4s`, `game_rules_slot`, `view_render`, `send_clan_tag`, `send_datagram`, all
prediction patterns, entity 348/409/218.

---

## 10. Still to Verify (continued from `csgo-re-f3.md`)

- `CCSPlayerAnimState`: `eyeYaw +0x28` reads as 0 (live bot), `footYaw +0x84` is likewise 0;
  needs re-testing in a different scenario/at a different time, or relocating (A3).
- `restoreEntityToPredictedFrame`, `m_nPredictionRandomSeed` (B1/B3).
- `CBaseAnimating::SetAbsAngle`, `m_angAbsRotation` (A5/A6, prerequisite for the fake matrix).
- `CClientState`: `deltaTick/lastCommandAck/lastOutgoingCommand/chokedCommands/paused` (C2).
- `sv_maxusrcmdprocessticks` / `SendDatagram` parameters (Tickbase/DT prerequisite).
- `C_BasePlayer::m_nButtons/m_afButtonForced` (B5).
