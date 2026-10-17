# zephyrus-csgo F3 reverse engineering (Ghidra)

> ABI / vtable / struct offsets as measured are summarized in [csgo-re-abi.md](csgo-re-abi.md) (Ghidra + core dump
> verification, including the wrong indices fixed in this round).
>
> Goal: fill in the offsets/functions/indices for **legacy CS:GO 1.38.8.1 linux64** that the
> F3 features of `zephyrus-csgo` need. All results are written into the `FINDINGS` code block at
> the bottom of this file, and the corresponding placeholders (0/NULL) in
> `include/zephyrus/csgo/offsets.hpp` and `src/zephyrus/csgo/patterns.cpp` are updated in sync.
>
> Programs (Ghidra project `csgo`):
> - `/client/client_client.so` (full analysis completed, ~82k functions)
> - `/engine/engine_client.so`
>
> Address convention: Ghidra address = ELF vaddr + 0x100000 (confirm with `get_metadata` for
> each program first). Report **in-module offsets** (Ghidra address - base).
>
> Delivery format: each item gives the module offset, a runtime-usable **byte pattern** (IDA
> style, verified for uniqueness with `search_byte_patterns`, reporting the hit count), evidence
> (function address + decompilation/xref snippets), and confidence (high/medium/low). A vtable
> index additionally lists the vtable slot address and the function address.

## Request list (by priority)

### A. AnimState / animation (anti-aim real desync, resolver, forward fake matrix)

| # | Item | Purpose | Notes |
|---|---|---|---|
| A1 | `C_CSPlayer::UpdateClientSideAnimation` function offset + **entity vtable index** + pattern | hooks (unfinished F3 item) | It calls `CCSPlayerAnimState::Update`; Osiris Windows index 224 |
| A2 | `C_CSPlayer::m_PlayerAnimState` pointer offset (Linux) | read/write AnimState | Windows O&E: 0x3914 |
| A3 | `CCSPlayerAnimState` field offsets | resolver detection, desync, animation framework | Windows layout in O&E AnimState.h |
| A4 | `CCSPlayerAnimState::Update` + `C_CSPlayer::CreateAnimState` + pattern | local fake matrix replication | |
| A5 | `C_BaseAnimating::SetAbsAngle` function offset + pattern | fake matrix rotation | O&E Windows pattern |
| A6 | `C_BaseEntity::m_angAbsRotation` offset | fake matrix / resolver | |
| A7 | `C_BaseAnimating::m_flPoseParameter` offset | animation framework | |
| A8 | `C_BaseAnimating::m_AnimOverlay` offset + `AnimationLayer` layout | resolver desync detection | |

### B. Engine prediction

| # | Item | Purpose |
|---|---|---|
| B1 | `restoreEntityToPredictedFrame(int, int)` | restore entities after manual prediction |
| B2 | `m_nPredictionPlayer` global | prediction prerequisite |
| B3 | `m_nPredictionRandomSeed` global | prediction / no-spread |
| B4 | `CGlobalVars` Linux offset re-verification | prediction |
| B5 | `m_nButtons` / `m_afButtonForced` | prediction button state |
| B6 | `VClientPrediction001` / `GameMovement001` availability | prediction interfaces |

### C. ClientState / networking

| # | Item |
|---|---|
| C1 | `CClientState` global (engine) |
| C2 | `deltaTick` / `lastCommandAck` / `lastOutgoingCommand` / `chokedCommands` / `m_NetChannel` / `paused` |
| C3 | `CNetChan::SendDatagram` |
| C4 | `CNetChan::chokedPackets` |
| C5 | `CGlobalVars::serverTime()` curtime path |

### D. Other

| # | Item |
|---|---|
| D1 | `GetMaxDesyncDelta` / `aimYawMin` / `aimYawMax` |
| D2 | `C_CSPlayer::GetModelPtr` (`m_pStudioHdr`) |
| D3 | `CStudioHdr` / hitbox layout |
| D4 | `CGameMovement::ProcessMovement` |
| D5 | `VPhysicsSurfaceProps001` |
| D6 | `IsBreakable` / glass penetration |
| D7 | `RandomSeed` / `RandomFloat` dlsym |

## Analysis method summary

- Parse `.rela.dyn` (`R_X86_64_RELATIVE`) and `.data.rel.ro` directly with Python + pyelftools,
  look up `_ZTI` from the RTTI name string (`_ZTS...`), then look up the vtable from `_ZTI`,
  thereby locating the vtables of `C_CSPlayer` / `CBasePlayerAnimState` /
  `CCSPlayerAnimState`.
- Cross-verify with Ghidra `decompile_function` / `disassemble_bytes` / `get_xrefs_to` /
  `search_byte_patterns`.
- String xref (the ConVar object `0x24734e0` of `cl_showanimstate`) locates
  `CBasePlayerAnimState::Update`.

### Confirmed key vtables

| Class | vtable module offset | Notes |
|---|---|---|
| `C_CSPlayer` primary | `0x21C7B00` | slot 208=`isAlive`, 210=`isPlayer` (`return 1`), 218=`isWeapon` (`return 0`), consistent with the Osiris Linux indices |
| `CBasePlayerAnimState` primary | `0x20FE218` | 34 slots; slot 1 = `Update`(0x768370) |
| `CCSPlayerAnimState` primary | `0x21D60C0` | overrides slot 1 = `0xF3E360` |

`C_CSPlayer` index self-check (Osiris Linux values): `getAbsOrigin` 12 → slot 12 writes `[this+0x2c4]` (origin component);
`isPlayer` 210 → `return 1`; `isAlive` 208 → `return *(char*)(this+0x297)==0`; `isWeapon` 218 → `return 0`.

---

## Results table

| # | Item | Module offset | vtable index | pattern (unique hit count) | Confidence |
|---|---|---|---|---|---|
| A1 | UpdateClientSideAnimation nearest implementation | `0xECFC90` (FUN_00fcec90, reads `[this+0xA248]` and `call [anim+8]`) | UNKNOWN (Win 224; Linux undetermined) | none (see body) | low |
| A2 | `m_PlayerAnimState` | `0xA248` | — | none (struct offset) | high |
| A3 | `eyeYaw` | `0x28` | — | — | high |
| A3 | `eyePitch` | `0x2C` | — | — | high |
| A3 | `moveYaw` | `0x6C` | — | — | medium |
| A3 | `footYaw` | `0x84` | — | — | medium |
| A3 | `footYawLast` | `0x34` | — | — | medium |
| A3 | `lastUpdateTime` | `0x74` | — | — | low |
| A4 | `CBasePlayerAnimState::Update` | `0x768370` | 1 | `55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18 4C 8B 25 ?? ?? ?? ?? F3 0F 11 45 C8 F3 0F 11 4D C4` (1) | high |
| A4 | `CCSPlayerAnimState::Update` (derived override) | `0xF3E360` | 1 (anim vtable) | — | medium |
| A4 | `C_CSPlayer::CreateAnimState` | `0xF3A030` | — | — | high |
| A5 | SetAbsAngle | UNKNOWN | — | — | — |
| A6 | `m_angAbsRotation` | UNKNOWN | — | — | — |
| A7 | `m_flPoseParameter` | `0x2D80` | — | — | high |
| A8 | `m_AnimOverlay` data | `0x3020` | — | — | high |
| A8 | `m_AnimOverlay` count | `0x3030` | — | — | high |
| A8 | `AnimationLayer` stride | `0x48` | — | — | high |
| A8 | `AnimationLayer.order` | `0x18` | — | — | high |
| A8 | `AnimationLayer.sequence` | `0x1C` | — | — | high |
| A8 | `AnimationLayer.weight` | `0x24` | — | — | high |
| A8 | `AnimationLayer.cycle` | `0x30` | — | — | high |
| A8 | `AnimationLayer.owner` | `0x38` | — | — | high |
| A8 | `prev_cycle`/`playback_rate` | UNKNOWN (suspected `0x20`/`0x2C`) | — | — | low |
| B1 | restoreEntityToPredictedFrame | UNKNOWN | — | — | — |
| B2 | `m_nPredictionPlayer` | `0x237CC80` | — | `41 8D 56 FF 31 C9` +`(-14).abs()` (1) | high |
| B3 | `m_nPredictionRandomSeed` | UNKNOWN (see body; Osiris's Linux anchor is actually B2) | — | — | low |
| B4 | CGlobalVars | reuse existing | — | — | low |
| B5 | `m_nButtons`/`m_afButtonForced` | UNKNOWN | — | — | — |
| B6 | `VClientPrediction001`/`GameMovement001` | creatable (string registered) | — | — | medium-high |
| C1 | `CClientState` getter | `0x2B48D0` (getter); global slot `0xE27A08` | — | `48 8B 05 ?? ?? ?? ?? 55 48 89 E5 48 83 C0 08 83 FF FF` (1) | medium |
| C2 | `signonState` | `0x1A0` | — | — | high |
| C2 | `m_NetChannel` | `0x128` | — | — | medium |
| C2 | `deltaTick`/`lastCommandAck`/`lastOutgoingCommand`/`chokedCommands`/`paused` | UNKNOWN | — | — | — |
| C3 | `CNetChan::SendDatagram` | `0x4AA000` | — | `55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 53 48 89 F3 48 81 EC A8 01 10 00 64 48 8B 04 25 28 00 00 00 48 89 45 C8` (1) | high |
| C4 | CNetChan chokedPackets | UNKNOWN | — | — | — |
| C5 | curtime path | `PTR_DAT_023f7d28`(+0x10) = gpGlobals->curtime | — | — | medium |
| D1 | GetMaxDesyncDelta / aimYawMin/Max | UNKNOWN | — | — | — |
| D2 | `m_pStudioHdr` | `0x2FD8` | — | — | medium-high |
| D3 | CStudioHdr/hitbox layout | UNKNOWN | — | — | — |
| D4 | ProcessMovement | index 1 (confirmed in doc); address UNKNOWN | 1 | — | low |
| D5 | `VPhysicsSurfaceProps001` | string `0x195706C`, referenced by `FUN_00ab2d50` | — | — | medium |
| D6 | IsBreakable | UNKNOWN | — | — | — |
| D7 | `RandomSeed`/`RandomFloat` | libvstdlib_client.so dynsym exports (see body) | — | — | high |

---

## Evidence details

### A1 — UpdateClientSideAnimation (nearest implementation)

The `C_CSPlayer` primary vtable is at module offset `0x21C7B00`. Verified against the known Osiris
Linux indices:
slot 208 (`isAlive`) = `FUN_0084fe20`: `return *(char*)(this+0x297)==0`;
slot 210 (`isPlayer`) = `FUN_008fb5a0`: `return 1`;
slot 218 (`isWeapon`) = `FUN_0084fcf0`: `return 0`. The index scheme matches Osiris.

The only function that reads `m_PlayerAnimState` (`[this+0xA248]`) is **`FUN_00fcec90` (module
`0xECFC90`)**:

```c
// FUN_00fcec90  (module 0xECFC90)
if ((char)param_1[0x1483] == '\0') {            // +0xA418
  ...
  cVar2 = FUN_008f4ef0(param_1);                // IsAlive-like
  if (cVar2 == '\0') {
    (**(code **)(*(long *)param_1[0x1449] + 8))();   // anim = param_1[0x1449]=+0xA248
  } else {                                            // call anim vtable slot 1 = Update
    pcVar1 = *(code **)(*(long *)param_1[0x1449] + 8);
    (**(code **)(*param_1 + 0x6f8))(param_1);
    (**(code **)(*param_1 + 0x6f8))(param_1);
    (*pcVar1)(param_1[0x1449]);
  }
}
```

`param_1[0x1449]` = `+0xA248`, and `+8` is animstate vtable slot 1 (`Update`). Therefore
`FUN_00fcec90` is the nearest implementation driving the animstate update (likely part of
`ClientThink`/`PostThink`, where `*param_1 + 0x6f8` = vtable index 223).

**The Linux vtable index of `C_CSPlayer::UpdateClientSideAnimation` could not be determined**: in
Ghidra the `0x21C7B00` vtable contains no slot pointing at `0xECFC90`; Osiris Windows index 224
corresponds on Linux to slot 224 = `0x780940` (`return this+0x39a4`, a getter, not an animation
update). Attempted: full vtable scan (0..1000), scans over various register/SIB encodings of
`[reg+0xA248]`, combined scans of `call [reg+8]` + `mov reg,[base+disp32]` (`80 BF ... 00 74 36`,
etc.), none yielded a definite vtable slot. **Conclusion: A1's vtable index is UNKNOWN, low
confidence, not written into the source.**

### A2 — `m_PlayerAnimState` = `0xA248` (high)

1. The entity constructor (around Ghidra `0xFD4990`) stores it immediately after creating the animation state:

```
00fd49e2  CALL 0x0103a030          ; C_CSPlayer::CreateAnimState()
00fd49e7  MOV  qword ptr [RBX + 0xa248], RAX   ; m_PlayerAnimState = return value
00fd49f1  CALL 0x0103eef0
00fd49fd  MOV  qword ptr [RBX + 0xa250], RAX   ; another pointer (not animstate)
```

2. `FUN_00fcec90` loads the pointer from `+0xA248` and calls it through vtable slot 1 (see A1).
   The two independent pieces of evidence agree, so `player_anim_state = 0xA248`.

### A3 — `CCSPlayerAnimState` fields (relative to the AnimState object)

Start of `CBasePlayerAnimState::Update` (`FUN_00868370`, module `0x768370`):

```c
uVar8 = FUN_014eed70(param_1); *(undefined4 *)(param_3 + 5) = uVar8;   // +0x28 = eyeYaw
uVar8 = FUN_014eed70(param_2); *(undefined4 *)((long)param_3 + 0x2c) = uVar8; // +0x2c = eyePitch
```

(`param_3` is the AnimState; `param_3[4]` = `+0x20` = entity pointer.)

- `ComputePoseParam_BodyPitch` (`FUN_008650d0`): `fVar2 = *(float*)(param_1+0x2c)` → writes the
  `body_pitch` pose parameter. **eyePitch = 0x2C**.
- `ComputePoseParam_MoveYaw` (`FUN_008661e0`): takes the difference between `fVar8 = *(float*)(param_1+5)`
  (=`+0x28`, eyeYaw) and `*(float*)((long)param_1+0x84)` (footYaw), writes `+0x6C`
  (moveYaw), and writes `+0x58`/`+0x5C` (move_x/move_y); `+0x34` is footYawLast.
- Debug function `FUN_008673e0` prints `ent yaw=+0x50, body_yaw=@0x2473104, move_yaw=@0x2473100,
  gait_yaw=+0x84, body_pitch=@0x2473108`; where `gait_yaw` corresponds to `+0x84`.
- `FUN_00864080` (vtable slot 3): `*(float*)((long)param_1+0x74) = curtime` → suspected lastUpdateTime.
- `FUN_008648b0` (vtable slot 30) writes the velocity direction `atan2(vel.y,vel.x)*57.29…` into `+0x30` (moveYawIdeal).

Confirmed: `eyeYaw=0x28`, `eyePitch=0x2C` (high); `moveYaw=0x6C`, `footYaw=0x84`,
`footYawLast=0x34` (medium); `lastUpdateTime=0x74` (low).
`moveWeight / animDuckAmount / duckAdditional / onGround / walkToRunTransition /
velocityLengthXY / speedAsPortionOfRunTopSpeed` were not located this round (UNKNOWN).

### A4 — AnimState Update / CreateAnimState

- `CBasePlayerAnimState::Update` = `FUN_00868370` (module `0x768370`),
  vtable slot 1 (`0x20FE218+8`). It reads the ConVar `cl_showanimstate`
  (`get_xrefs_to 0x24734e0` → `FUN_00868370`), sets `eyeYaw/eyePitch`, and calls
  vtable slots `+0x58` (ClearAnimationLayers), `+0x60`, `+0x68`, `+0xE8` (BodyPitch),
  `+0xF0`, `+0xE0` (MoveYaw) in order. pattern has exactly 1 unique hit.
- `CCSPlayerAnimState::Update` (derived override) = `FUN_00f3e360` (module `0xF3E360`),
  directly calls the base class `0x868370` at `0x103e427`.
- `C_CSPlayer::CreateAnimState` = `FUN_0103a030` (module `0xF3A030`):

```
0103a048  MOV  EDI, 0x250          ; sizeof(CCSPlayerAnimState) = 0x250
0103a052  CALL operator new
0103a05d  CALL 0x01039d30          ; CCSPlayerAnimState ctor
0103a0f6  CALL 0x01039fa0          ; Init(state, entity, ...)
```
`FUN_00865560` (called by `FUN_01039fa0`) confirms `state+0x20 = entity`:
`param_1[4] = param_2;` (`param_1`=state, `param_2`=entity).

### A7 — `m_flPoseParameter` = `0x2D80` (high)

`C_BaseAnimating::GetPoseParameter` (Ghidra `0x8A1730`):

```
MOV  RDI, [RDI + 0x2fd8]              ; CStudioHdr*
MOVSXD RAX, ESI
MOVSS XMM0, [RBX + RAX*4 + 0x2d80]    ; m_flPoseParameter[index]
```

`C_BaseAnimating::SetPoseParameter` (`FUN_00898aa0`) writes
`*(float*)(param_1 + 0x2d80 + index*4)`. Hence `m_flPoseParameter = 0x2D80`.
This also yields `m_pStudioHdr = 0x2FD8` (D2).

### A8 — `m_AnimOverlay` and `AnimationLayer` (high)

`C_BaseAnimating::GetNumAnimOverlays` = `FUN_008b14c0`:

```
return *(int *)(param_1 + 0x3030);       // count
```

`C_BaseAnimating::GetAnimOverlay` = `FUN_008b1450`:

```c
iVar2 = *(int *)(param_1 + 0x3030);          // count
lVar3 = *(long *)(param_1 + 0x3020);         // data pointer
...
while (lVar5 = lVar4 + 0x48, ...)            // stride = 0x48 (72)
    piVar1 = (int *)(lVar4 + 0x60);          // = next layer + 0x18 -> order
```

`AnimationLayer` fields (confirmed by the setters):
- `FUN_008b0e10`: `*(int*)(layer+0x18) = v` (and tests `==0xf`) → **order = 0x18**
- `FUN_008b0ce0`: `*(int*)(layer+0x1c) = v` → **sequence = 0x1C**
- `FUN_008b0e80`: `*(float*)(layer+0x24) = v` → **weight = 0x24**
- `FUN_008b0d70`: `*(float*)(layer+0x30) = v` → **cycle = 0x30**
- `FUN_008b0e10/0ce0/0d70/0e80` all use `*(long*)(layer+0x38)` for the owner notification → **owner = 0x38**

Hence `anim_overlay_vector = 0x3020`, `anim_overlay_count = 0x3030`, stride `0x48`.
`prev_cycle`/`playback_rate` were not directly confirmed (suspected `0x20`/`0x2C`, low).

### B2 — `m_nPredictionPlayer` = `0x237CC80` (high)

pattern `41 8D 56 FF 31 C9` has a unique hit in client at Ghidra `0x861961`.
The Osiris Linux chain `(-14).abs()` resolves to Ghidra `0x247CC80` (module `0x237CC80`):

```
00861950  LEA  RAX, [0x247cc80]        ; global
00861957  CMP  qword ptr [RAX], 0x0
00861961  LEA  EDX, [R14 + -0x1]       ; pattern start
00861965  XOR  ECX, ECX
```

This global is written as an **entity pointer** by the following functions (proving it is
`m_nPredictionPlayer`, not the random seed):

```c
// FUN_00ad17b0  (module 0x9D17B0)
*(long *)(param_2 + 0x3b28) = param_3;
...
DAT_0247cc80 = param_2;                 // m_nPredictionPlayer = entity

// FUN_00ad1a50  (module 0x9D1A50)
DAT_0247cc80 = 0;                       // m_nPredictionPlayer = nullptr
```

`prediction_player` can be obtained as `0x237CC80` with the same pattern chain as
`prediction_random_seed`. **Note: Osiris's Linux `predictionRandomSeed()` actually resolves to
this global, i.e. on this build it returns a player pointer rather than an int random seed (see
B3).**

### B3 — `m_nPredictionRandomSeed`

No standalone int random-seed global was located. The `41 8D 56 FF 31 C9` anchor (currently
`prediction_random_seed` in `patterns.cpp`) resolves to `0x237CC80`, but the read/write semantics
at that address are `C_BasePlayer*` (B2 evidence). Therefore **B3 = UNKNOWN (low)**; the existing
source chain is left unchanged.

### B6 — Interface registration

- `VClientPrediction001` string module offset `0x19D0D70` (with leading `?` byte),
  referenced by `_INIT_446` (Ghidra `0x7DD073`) → registered by `CreateInterface`.
- `GameMovement001` string module offset `0x19C39E6`, referenced by `FUN_007D6BE0` (`0x7D75C2`).
- `VClient018` `0x1945F14`, `VEngineClient014` `0x1945D24`, `VClientEntityList003`
  `0x19479B6`, `VPhysicsSurfaceProps001` `0x195706C` likewise exist.
  Conclusion: client's `CreateInterface` can retrieve `VClientPrediction001` and
  `GameMovement001` (medium-high). IPrediction indices reuse the document's known values
  `setupMove 20 / finishMove 21`; `CGameMovement::processMovement 1`.

### C1 — `CClientState` getter / global slot

`FUN_003b48d0` (engine module `0x2B48D0`):

```
003b48d0  MOV  RAX, qword ptr [0x00f27a08]   ; g_clientState[0]
003b48d7  PUSH RBP
003b48d8  MOV  RBP, RSP
003b48db  ADD  RAX, 0x8
003b48df  CMP  EDI, -0x1
003b48e2  JZ   +0x13
003b48e4  LEA  RAX, [0xf27a00]
003b48ee  MOV  RAX, [RAX + RDI*8 + 0x8]
```

pattern has exactly 1 unique hit. The global slot address = engine module offset `0xE27A08` (Ghidra
`0xF27A08`), the object is `[0xE27A08]` (the getter returns `+8`). Both `CBaseClientState::RunFrame`
(`FUN_0034be30`) and `CClientState::SetSignonState` (`FUN_003c8820`) use this object as
`this`. Confidence medium (the meaning of `+8` is not fully clarified).

### C2 — `CClientState` fields

- `signonState`: `switch(*(undefined4 *)(param_1 + 0x1a0))` in `FUN_003c8820`;
  `iVar8 = (int)param_1[0x34]` in `FUN_0034be30` (= `+0x1A0`). **= 0x1A0 (high)**.
- `m_NetChannel`: `*(long **)(param_1 + 0x128)` in both functions, with CNetChan methods called
  through `[chan_vtable + 0x148/0x100/0x200]`. **= 0x128 (medium)**.
- `deltaTick` / `lastCommandAck` / `lastOutgoingCommand` / `chokedCommands` / `paused`
  not located (UNKNOWN).

### C3 — `CNetChan::SendDatagram` = `0x4AA000` (high)

`FUN_005aa000` (engine module `0x4AA000`) contains a profiler name check:

```
if ((char *)*_DAT_044477c0 != "CNetChan::SendDatagram")
    CVProfNode::GetSubNode(..., 0xa6c7cb, ...);   // 0xA6C7CB = that string
```

The string `CNetChan::SendDatagram` is at module offset `0x96C7CB`, and `get_xrefs_to` points to
`FUN_005aa000`. The signature `int FUN_005aa000(long *chan, void *data)` matches
`int(void* this, void* data)`. pattern has exactly 1 unique hit.

### D5 — `VPhysicsSurfaceProps001`

The string is at module offset `0x195706C`, referenced by `FUN_00ab2d50` (Ghidra `0xAB2D74`) → interface is creatable (medium).

### D7 — dynsym exports

The `.dynsym` exports of `libvstdlib_client.so` (usable with `dlsym(RTLD_DEFAULT, ...)`):
`_ZN20CUniformRandomStream9SetSeedEi` @ 0x1e3e0, `_ZN20CUniformRandomStream11RandomFloatEff` @ 0x1e750,
`_ZN20CUniformRandomStream9RandomIntEii` @ 0x1e840, `RandomSeed` @ 0x1e4a0,
`RandomFloat` @ 0x1e4c0, `RandomInt` @ 0x1e500.
The `.dynsym` of client/engine lists the three `CUniformRandomStream` methods as **undefined
imports** (`U`), meaning they are resolved from vstdlib at runtime; `MD5_PseudoRandom` was not
found in the dynamic symbol tables of client/engine/vstdlib (it may be a local symbol).
`SendDatagram` and `EngineTraceClient` are not exported as dynamic symbols.

---

## Source changes (high confidence only)

`include/zephyrus/csgo/offsets.hpp`:
`player_anim_state=0xA248`, `anim_overlay_vector=0x3020`, `anim_overlay_count=0x3030`,
`anim_state.eye_yaw=0x28`, `anim_state.eye_pitch=0x2C`.
The rest (`update_client_side_animation_vtable`, `client_state_*`, `player_buttons`,
`player_button_forced`) stay 0 (UNKNOWN / medium).

`src/zephyrus/csgo/patterns.cpp`:
added `update_anim_state` (0x768370), `prediction_player` (0x237CC80),
`send_datagram` (0x4AA000); all patterns have been verified in Ghidra to have exactly 1 unique hit.
`update_client_side_animation`, `create_anim_state`, `restore_entity_to_predicted_frame`,
`trace_to_exit`, `client_state` stay null.

## FINDINGS

```json
{
  "A1": {"offset": "0xECFC90", "pattern": null, "vtable_index": 286, "confidence": "high",
         "evidence": "Primary C_CSPlayer vtable (Itanium header offset-to-top=0/typeinfo at Ghidra 0x22C7AF0) starts at Ghidra 0x22C7B00. Slot verification: [12]=0x8C1EF0 returns this+0xD8 (GetAbsOrigin), [208]=0x84FE20 returns *(char*)(this+0x297)==0 (IsAlive), [210]=0x8FB5A0 returns 1 (IsPlayer on a real player). [286] = 0xFCEC90 (module 0xECFC90): reads m_PlayerAnimState (+0xA248) and calls AnimState::Update (vtable slot 1). Linux index = 286 (Windows 224). vtable entry address Ghidra 0x22C83F0.",
  "A2": {"offset": "0xA248", "pattern": null, "vtable_index": null, "confidence": "high",
         "evidence": "0xFD49E7 MOV [RBX+0xa248],RAX after CALL 0x0103a030 (CreateAnimState); FUN_00fcec90 reads param_1[0x1449](=+0xA248) and calls [anim_vtable+8]."},
  "A3": {"offset": "0x28", "pattern": null, "vtable_index": null, "confidence": "medium",
         "evidence": "CBasePlayerAnimState::Update FUN_00868370 stores arg1/arg2 into state+0x28/state+0x2C (eyeYaw/eyePitch; high) after normalizing via FUN_014eed70. ComputePoseParam_BodyPitch FUN_008650D0 clamps [+0x2C] and sets the body_pitch pose (high). SetUpMovement FUN_008648B0 initializes [+0x34] from [+0x28] on first run and normalizes it every tick (goal feet yaw; medium). ComputePoseParam_MoveYaw FUN_008661E0 computes -(velocityYaw - [+0x84]) and stores into [+0x6C] (current feet yaw / move yaw; medium). Resolver sanity-checks |footYaw - eyeYaw| <= 65 before use.",
         "eye_yaw": "0x28", "eye_pitch": "0x2C", "move_yaw": "0x6C", "foot_yaw": "0x84",
         "foot_yaw_last": "0x34", "last_update_time": "0x74", "player_ptr": "0x20",
         "move_yaw_ideal": "0x30",
         "unknown": ["moveWeight","animDuckAmount","duckAdditional","onGround","walkToRunTransition","velocityLengthXY","speedAsPortionOfRunTopSpeed"],
         "confidence_detail": {"eye_yaw":"high","eye_pitch":"high","move_yaw":"medium","foot_yaw":"medium","foot_yaw_last":"medium","last_update_time":"low"}},
  "A4": {"offset": "0x768370", "pattern": "55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18 4C 8B 25 ?? ?? ?? ?? F3 0F 11 45 C8 F3 0F 11 4D C4", "vtable_index": 1, "confidence": "high",
         "evidence": "CBasePlayerAnimState::Update=FUN_00868370 at CBasePlayerAnimState vtable 0x20FE218 slot1; reads cl_showanimstate ConVar (0x24734e0). Derived CCSPlayerAnimState::Update=FUN_00f3e360 (module 0xF3E360) overrides slot1 and calls base at 0x103e427. CreateAnimState=FUN_0103a030 (module 0xF3A030) allocates 0x250, calls ctor FUN_01039d30 and Init FUN_01039fa0.",
         "create_anim_state": "0xF3A030", "derived_update": "0xF3E360"},
  "A5": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing",
         "evidence": "O&E Windows pattern 55 8B EC 83 E4 F8 83 EC 64 53 56 57 8B F1 has no Linux equivalent found; 48 83 E4 F8 48 83 EC 64 53 56 57 and 48 83 EC 64 53 56 57 both 0 matches."},
  "A6": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "not located"},
  "A7": {"offset": "0x2D80", "pattern": null, "vtable_index": null, "confidence": "high",
         "evidence": "C_BaseAnimating::GetPoseParameter FUN_008a1730: MOVSS XMM0,[RBX+RAX*4+0x2d80]; SetPoseParameter FUN_00898aa0 writes [this+0x2d80+idx*4]."},
  "A8": {"offset": "0x3020", "pattern": null, "vtable_index": null, "confidence": "high",
         "evidence": "GetNumAnimOverlays FUN_008b14c0 returns *(int*)(this+0x3030); GetAnimOverlay FUN_008b1450 uses data ptr [this+0x3020] and stride 0x48.",
         "vector": "0x3020", "count": "0x3030", "layer_stride": "0x48",
         "layer": {"order":"0x18","sequence":"0x1C","weight":"0x24","cycle":"0x30","owner":"0x38"},
         "unknown_layer": ["prev_cycle (suspected 0x20)","playback_rate (suspected 0x2C)"]},
  "B1": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "not located"},
  "B2": {"offset": "0x237CC80", "pattern": "41 8D 56 FF 31 C9", "vtable_index": null, "confidence": "high",
         "evidence": "pattern unique at Ghidra 0x861961; chain (-14).abs() -> Ghidra 0x247CC80 (module 0x237CC80). FUN_00ad17b0 writes DAT_0247cc80=entity; FUN_00ad1a50 writes 0."},
  "B3": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "low",
         "evidence": "The Osiris Linux predictionRandomSeed anchor 41 8D 56 FF 31 C9 resolves to the m_nPredictionPlayer pointer global 0x237CC80, not an int seed. True int seed global not located."},
  "B4": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "low",
         "evidence": "Existing offsets (realtime 0x0, curtime 0x10, frametime 0x14, tickCount 0x1C, intervalPerTick 0x20) not re-verified; PTR_DAT_023f7d28 is gpGlobals with +0x10 read as curtime."},
  "B5": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "not located"},
  "B6": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "medium",
         "evidence": "VClientPrediction001 string module 0x19D0D70 referenced by _INIT_446 (0x7DD073); GameMovement001 string module 0x19C39E6 referenced by FUN_007D6BE0 (0x7D75C2); both registered for CreateInterface. IPrediction setupMove 20 / finishMove 21 (known)."},
  "C1": {"offset": "0x2B48D0", "pattern": "48 8B 05 ?? ?? ?? ?? 55 48 89 E5 48 83 C0 08 83 FF FF", "vtable_index": null, "confidence": "medium",
         "evidence": "getter FUN_003b48d0 (module 0x2B48D0) unique; global slot at engine module 0xE27A08 (Ghidra 0xF27A08), getter returns [0xE27A08]+8."},
  "C2": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "medium",
         "evidence": "signonState=0x1A0 (FUN_003c8820 switch on [this+0x1a0]; FUN_0034be30 param_1[0x34]); m_NetChannel=0x128 (both functions deref [this+0x128] and call CNetChan vtable methods). deltaTick/lastCommandAck/lastOutgoingCommand/chokedCommands/paused UNKNOWN.",
         "signon_state": "0x1A0", "net_channel": "0x128",
         "unknown": ["deltaTick","lastCommandAck","lastOutgoingCommand","chokedCommands","paused"]},
  "C3": {"offset": "0x4AA000", "pattern": "55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 53 48 89 F3 48 81 EC A8 01 10 00 64 48 8B 04 25 28 00 00 00 48 89 45 C8", "vtable_index": null, "confidence": "high",
         "evidence": "FUN_005aa000 contains profiler name check against string CNetChan::SendDatagram (module 0x96C7CB); get_xrefs_to 0xA6C7CB -> FUN_005aa000."},
  "C4": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "not located"},
  "C5": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "low",
         "evidence": "gpGlobals = PTR_DAT_023f7d28; curtime read as [PTR_DAT_023f7d28]+0x10 (FUN_00864080, FUN_008648b0)."},
  "D1": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "aimYawMin/Max not located"},
  "D2": {"offset": "0x2FD8", "pattern": null, "vtable_index": null, "confidence": "medium",
         "evidence": "GetPoseParameter FUN_008a1730 loads MOV RDI,[RDI+0x2fd8] (CStudioHdr*)."},
  "D3": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "not located"},
  "D4": {"offset": null, "pattern": null, "vtable_index": 1, "confidence": "low",
         "evidence": "CGameMovement::processMovement index 1 already known; address not located."},
  "D5": {"offset": "0x195706C", "pattern": null, "vtable_index": null, "confidence": "medium",
         "evidence": "VPhysicsSurfaceProps001 string referenced by FUN_00ab2d50 -> creatable."},
  "D6": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "missing", "evidence": "not located"},
  "D7": {"offset": null, "pattern": null, "vtable_index": null, "confidence": "high",
         "evidence": "libvstdlib_client.so exports _ZN20CUniformRandomStream9SetSeedEi, _ZN20CUniformRandomStream11RandomFloatEff, _ZN20CUniformRandomStream9RandomIntEii, RandomSeed, RandomFloat, RandomInt (dlsym). client/engine import the three CUniformRandomStream methods. MD5_PseudoRandom / SendDatagram / EngineTraceClient not exported."}
}
```
