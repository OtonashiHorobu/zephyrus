# zephyrus-csgo Linux ABI / vtable 逆向验证

> 目标：记录 **legacy CS:GO 1.38.8.1 linux64**（`client_client.so` / `engine_client.so` /
> `vphysics_client.so`）上已用 Ghidra + core dump 实测验证过的调用约定、vtable 槽位与
> 结构体偏移。每条给出模块偏移、证据（函数/指令）、置信度与对应的 zephyrus 代码路径。
>
> 关联：[csgo-port-plan.md](csgo-port-plan.md) · [csgo-re-f3.md](csgo-re-f3.md)（动画/预测/
> 网络等待办项的请求清单）· [todo-csgo.md](todo-csgo.md)
>
> 地址约定：Ghidra 地址 = ELF vaddr + 0x100000；本文除特别说明外使用 **vaddr（模块偏移）**。
> Ghidra 项目 `csgo`（`/datassd/Clone/acr-bin/ghidra/csgo`）programs：
> `/client/client_client.so`、`/engine/engine_client.so`、`/vphysics/vphysics_client.so`。
> 运行期证据：`coredumpctl debug <pid>` / `gdb` 直接读 core 内存。

---

## 1. 调用约定总则（血的教训）

Source 引擎的“引用参数”在 SysV ABI 下是**指针**。如果 C++ 侧把大结构体（`Ray_t`、
`trace_t`、`Vector` 等 MEMORY class 聚合）按 **值** 传给 `virtual_call`，编译器会把它们
放到栈上，寄存器里留下垃圾，引擎侧再按指针解引用 → SIGSEGV。

zephyrus 的修复（`include/zephyrus/csgo/entity.hpp`）：为 `IEngineTrace` 写了专用
typed wrapper `engine_trace_ray()` / `engine_point_contents()`，签名显式写指针：

```cpp
using fn_type = void (*)(void *, const ray &, unsigned int, trace_filter *, trace *);
```

**所有**跨模块虚调用都必须这样写；不要再给 `virtual_call` 直接传聚合类型。

---

## 2. `IEngineTrace`（`EngineTraceClient004`，engine_client.so）

### 2.1 vtable 与槽位

runtime core 实测 vtable（engine_client.so 内）：

| 槽位 | vaddr | 签名（实测） | 说明 |
|---|---|---|---|
| 0 | `0x5462e0` | `int GetPointContents(long* this, const Vector* pos, uint mask, void** ppEntity)` | 参数全指针 |
| 5 | `0x547290` | `void TraceRay(long* this, const Ray_t* ray, uint mask, ITraceFilter* filter, trace_t* trace)` | 参数全指针 |

关键指令证据（`0x547290`）：

```asm
; TraceRay 入口：rdi=this, rsi=ray*, edx=mask, rcx=filter*, r8=trace*
mov  rdi, r8            ; -> FUN_004fcdc0(trace)
call 0x3fcdc0           ; 0x3fcdc0: MOV ECX,0xd; XOR EAX,EAX; REP STOSQ -> 清零 13*8=0x68 字节
...
mov  (%r12),%rax
call *0x8(%rax)         ; filter->vtable[1]() 结果与 1/2 比较 -> slot1 = GetTraceType
...
call *(%rax)            ; filter->vtable[0](entity, mask) -> slot0 = ShouldHitEntity
```

结论：

* `trace_t` 大小 **0x68 (104 字节)**；引擎在每次 TraceRay 前整块清零。
  额外佐证：`+0x36` = `all_solid`、`+0x37` = `start_solid`（与 `Trace` 结构一致）。
* `Ray_t`：`m_IsRay` 在 **+0x48**（引擎读取 `*(char *)(ray + 0x48)`）；
  `m_Start` 实际占 16 字节（`float pad`），`m_Delta` 从 +0x10 开始。
* `ITraceFilter`：slot 0 = `ShouldHitEntity`，slot 1 = `GetTraceType`，
  **没有虚析构函数**。zephyrus 曾经加了 `virtual ~trace_filter()`，会把两个析构槽插到
  槽 0/1，引擎调用 `GetTraceType` 实际调到析构函数（对栈对象 `delete`）——已移除。

### 2.2 崩溃证据（修复前）

```
2026-10-10 11:56 core:
#0 engine_client.so+0x3fcdcb   ; rep stos %rax,(%rdi), rdi=0x1
#1 engine_client.so+0x44730a
#2 zephyrus::csgo::virtual_call<5, void, ray, uint, trace_filter_skip, trace>
#3 zephyrus::csgo::entity::is_visible
#4 zephyrus::csgo::features::rage::run
```

`trace*` 因按值传参没有进入 R8，R8=1 被当作 trace 指针 → 清零时写 0x1。
按指针重写后 `rage`/ESP/autowall 的所有 trace 均正常（实机验证 `trace` 命令返回
`fraction/endpos/hitgroup` 合理值）。

---

## 3. `IPhysicsSurfaceProps`（`VPhysicsSurfaceProps001`，vphysics_client.so）

### 3.1 vtable（runtime core 实测，对象 `this+0x170`=表指针、`+0x180`=数量）

| 槽位 | vaddr | 函数 |
|---|---|---|
| 0/1 | `0x3f530` / `0x3f640` | 析构对 |
| 2 | `0x3f9b0` | `ParseSurfaceData` |
| 3 | `0x3eec0` | `SurfacePropCount()` → `return *(int*)(this+0x180)` |
| 4 | `0x3f180` | `GetSurfaceIndex(const char*)` |
| 5 | `0x3eed0` | `GetPhysicsProperties(index, float* friction, float* elasticity, float* density, float* thickness)` |
| **6** | `0x3f000` | **`GetSurfaceData(index)`** |

`GetSurfaceData` 反汇编要点（`0x3f000`）：

```asm
cmp esi, 0x7f / 0xf000  ; 特殊索引处理
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

> **Osiris 的 Linux 索引 5 是错误的**（Windows 端可能相同，但本 build 槽 5 是
> `GetPhysicsProperties`，它有 4 个输出指针参数）。用它当 `GetSurfaceData` 会把
> garbage 寄存器当输出指针写 —— 2026-10-10 12:32 的 core 即此崩溃（`rdi=this`,
> `rdx=0x40`，写 `movss %xmm0,(%rdx)`）。

### 3.2 `surfacedata_t` 布局（运行时实测）

取索引 64 的表项（`table=0x33fb2000`, entry 64 @ `0x33fb4c00`）解码：

| 相对 `GetSurfaceData()` 返回指针 | 值 | 字段 |
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

表项内部另有 `+0x00` 指针、`+0x10` 计数、`+0x4c` 起的 sound 索引（short 数组）等。
zephyrus 的 `struct surface_data { std::byte pad0[80]; ... }` 与该实测布局一致。

### 3.3 相应代码

* `src/zephyrus/csgo/game.cpp`：`surface_props()` 调用槽位 **6**。
* `src/zephyrus/features/csgo/autowall.cpp`：`penetration_modifier` / `material` 读取。

---

## 4. `MD5_PseudoRandom` 与武器散布（client_client.so）

功能结论：**CS:GO 做不到 no-spread**（服务端拥有弹道散布的随机数），zephyrus 已移除该功能；
以下记录仅为逆向资料。

* `MD5_PseudoRandom` @ **vaddr 0x1346b30**：

```asm
1346b30: ...                       ; 构造 MD5 context
1346b5f: lea -0x84(%rbp),%rsi      ; 输入 = &command_number
1346b8b: call MD5Update(ctx, &seed, 4)
1346b97: call MD5Final(digest @ rbp-0x80, ctx)
1346b9c: mov -0x7a(%rbp),%eax      ; 返回 *(uint32*)(digest + 6)  ← 不是 digest+0
```

* 调用方 `CCSWeaponBase::UpdateSpread` @ **vaddr 0xfd5c10**：
  `RandomSeed(*(int*)(this+0x558))`（**无 `&0xFF` 掩码**），随后 4 次 `RandomFloat`：
  缓存到 `this+0x568 / +0x574 / +0x580 / +0x55C`，供本帧的散布/后坐力计算使用。

---

## 5. `CHLClient`（`VClient018`，client_client.so）

* CreateInterface 注册链：`_INIT_119` → `InterfaceReg(&DAT_024b27b0, FUN_00985dd0, "VClient018")`
  → `FUN_00985dd0()` 返回实例 `&DAT_024a9b60`；构造器写入
  `*param_1 = &PTR_FUN_021c12c8` → **vtable base Ghidra 0x021c12c8**。
* 槽位实测（Ghidra 地址）：

| 槽位 | 地址 | 函数 | 说明 |
|---|---|---|---|
| 37 | `0x0098bf80` | `FrameStageNotify(this, int stage)` | `switch(stage)` 0..8，写 `_DAT_023f7cf8` |
| 38 | `0x00986dc0` | `DispatchUserMessage` thunk | 保存参数 → `call 0x00b25fd0`（取子对象）→ `jmp 0x00b25660` |

结论：**Linux 索引 37/38 与 Osiris 一致**。zephyrus 的 `client_mode_hooks` 使用正确
（`frame_stage_notify`、`dispatch_user_message`）。

其它 ClientMode/Client 槽位（来自 Osiris Linux 并已被 zephyrus 使用）：
`shouldDrawFog 18`、`overrideView 19`、`createMove 25`、`getViewModelFov 36`、
`doPostScreenEffects 45`。

---

## 6. `VEngineClient014`（engine_client.so）

* 注册链：`_INIT_18` → `FUN_00751010(&DAT_00f11590, FUN_00378500, &DAT_00a3ad2f)`
  （字符串 `VEngineClient014` @ file 0x93ad2f）→ `FUN_00378500` 返回实例
  `&DAT_00f115a8`；`*DAT_00f115a8 = &PTR_FUN_00e64038` → vtable base Ghidra 0x00e64038。
* 槽位 8（`0x00e64078`）= `FUN_003796e0` = `GetPlayerInfo(int ent_num, void* out)`：
  * 从 engine 的玩家信息源拷贝 **0x158 字节** 到 `out`；失败时清零 0x158 字节。
  * `player_info_t` 布局：`version +0x00`、`xuid +0x08`、**`name +0x10`**、
    `userID +0x90`、`guid +0x94`、`friendsID +0xB8`、`friendsName +0xBC`、
    `fakeplayer +0x13C`、`hltv +0x13D`、`customFiles +0x140`、`filesDownloaded +0x150`，
    总大小 **0x158**。
* 其它已使用槽位（Osiris Linux 值，zephyrus 依赖）：`getViewAngles 18`、
  `setViewAngles 19`、`getMaxClients 20`、`worldToScreenMatrix 37`、
  `getLevelName 53`、`getNetworkChannel 78`；`NetworkChannel::getLatency = 9`。

---

## 7. `C_CSWeaponBase`（client_client.so）

方法：按 RTTI `.data.rel.ro` 扫描 556+ 槽位的 vtable run，再反汇编候选槽位核对语义。

| 槽位 | vaddr | 函数 | 证据 |
|---|---|---|---|
| 551 | `0x10182f0` | `GetInaccuracy()` | 读取 +0x3bb4 等，返回 xmm0 浮点 |
| 552 | `0x101d770` | `GetSpread()` | 读 `gpGlobals->curtime`（`PTR_DAT_023f7d28+0x10`）与武器数据，返回浮点 |

zephyrus `entity::inaccuracy()/spread()` 使用 551/552，`hitchance` 基于二者建模。

---

## 8. 结构体尺寸与 static_assert（已加入源码）

`include/zephyrus/csgo/entity.hpp`：

```cpp
static_assert(offsetof(ray, is_ray) == 72, "Ray_t layout changed");
static_assert(sizeof(trace) == 0x68, "CGameTrace size changed");
static_assert(offsetof(trace, all_solid) == 54 &&
              offsetof(trace, start_solid) == 55, "CGameTrace flags moved");
```

`animation_layer`：stride 0x48（72B），`order +0x18`、`weight +0x24`、`cycle +0x30`
（`csgo-re-f3.md` A8 已记录）。

`user_cmd`：与 SDK 一致，基础部分 0x50；zephyrus 追加的
`viewangles_backup/buttons_backup` 只读不写引擎数据（`static_assert(sizeof==0x60)`）。

---

## 9. 错误索引汇总（本次修复）

| 项 | 错误 | 修复 |
|---|---|---|
| `IPhysicsSurfaceProps::GetSurfaceData` | 槽 5（实际是 `GetPhysicsProperties`）→ 崩溃 | 改为槽 6（core + 反汇编验证） |
| `IEngineTrace::TraceRay/GetPointContents` | 按值传聚合参数 → 寄存器垃圾 → 崩溃 | typed wrapper，显式指针（`entity.hpp`） |
| `ITraceFilter` | 多加 `virtual ~trace_filter()` 使槽位错位 | 移除虚析构 |
| `MD5_PseudoRandom` | 取 `digest+0`（实际 `digest+6`） | 功能整体移除（CS:GO 无法 no-spread） |
| `CHLClient` 37/38 | 曾被怀疑；实测正确 | 保持 |
| `ClientMode` 18（no-fog） | 桩函数 `return 1`，被当 `ShouldDrawFog` 用 | 钩子与功能删除（曾导致 viewmodel 消失） |
| `ClientMode` 45（no-scope） | 跳过 `DoPostScreenEffects` 不隐藏瞄准镜叠层 | 钩子与功能删除 |
| `SetAbsOrigin` pattern | 应用在 backtrack 里把敌人 origin 写到历史位置且不恢复 | 删除写回，只改 `cmd.tick_count`（修 ESP 幽灵框） |
| `line_goes_through_smoke` / `is_other_enemy` | Osiris pattern，未核对 | 删除；队内判断改用队伍号（vtable 128，已核对） |
| glow 模块（`glowObjectManager` pattern） | 未核对，开启即崩 | 模块、pattern、UI 全删 |
| prediction 模块 | 依赖的 `client_state` / `restoreEntityToPredictedFrame` 从未解析（恒 null） | 模块删除（含 m_nPrediction* pattern） |
| entity 348（`eye_position`） | 未核对；实为带参函数（RSI/RDX），无参调用读垃圾 | 改为 netvar 计算 `origin + m_vecViewOffset[0]` |
| entity 409 / 218 | 未核对 / 基类 `return 0` | 删除（无使用） |
| `UpdateClientSideAnimation` 钩子 | 只服务于未实现的 fake matrix | 删除 |

**教训**：Osiris Linux 的 vtable 索引并非全部可信（至少 `IPhysicsSurfaceProps` 是错的）。
凡是带复杂签名/输出指针的虚调用，上线前必须用 core 或 Ghidra 复核一次。

---

## 11. RE 审计：逐槽位核对记录（2026-10-10）

### 11.1 `ClientModeCSNormal`

* 静态实例 `0x06e04200`（`_INIT_438` @ `0x007d62b2` 调用构造器）；
  全局槽 `DAT_06e078e8` 由 `FUN_00ff6cc0` 写入 `&DAT_06e04200`，
  `CHLClient::vtable[10]`（`0x009862b0`）读取该全局。
* 构造器 `0x00ff8b20` 写入 vtable **`0x022d14b0`**（另有子对象表
  `[+8]=0x22d16b0`、`[+0x50]=0x22d16e0`、`[+0x58]=0x21cfa10`）。
* 反汇编核对（`0x022d14b0`）：

| 槽 | vaddr | 签名（实测） | 结论 |
|---|---|---|---|
| 14/17/18/29 | `0x009ab530` | `PUSH RBP; MOV EAX,1; RET` | 默认谓词 `return true`，**不是** `ShouldDrawFog` |
| 19 | `0x009ad1b0` | `(this[ignored], void* setup)` 取单例后转发 | `OverrideView(CViewSetup*)`，保留 |
| 25 | `0x009aba30` | 保存 `xmm0`(float) 与 `rsi`(ptr) 后转发 | `CreateMove(float, CUserCmd*)`，保留 |
| 36 | `0x009ae060` | 无额外参数转发 | `GetViewModelFov()`；功能未证实，钩子删除 |
| 45 | `0x00ff6f40` | `(this, param)` 调 `FUN_009aefb0`→`FUN_00a17fd0`→`FUN_00a1a990` | `DoPostScreenEffects`（glow 渲染）；钩子删除（no-scope 无效） |

### 11.2 `CHLClient`（`VClient018`）

vtable `0x021c12c8`（构造器 `_INIT_119`→`0x00985dd0`）：

| 槽 | vaddr | 函数 |
|---|---|---|
| 37 | `0x0098bf80` | `FrameStageNotify(this, int)`（`switch(stage)` 0..8） |
| 38 | `0x00986dc0` | `DispatchUserMessage` thunk（保存参数→`0x00b25fd0`→`0x00b25660`） |

### 11.3 `C_CSPlayer` 对象布局（构造器 `0x00fd3740`）

```
[obj+0x00] = 0x22c7b00  主 vtable (C_CSPlayer)
[obj+0x08] = 0x22c89b0  IClientRenderable 子对象 vtable
[obj+0x10] = 0x22c8b30  IClientNetworkable 子对象 vtable
```

| 表 | 槽 | vaddr | 语义（反编译证据） |
|---|---|---|---|
| 主 | 12 | `0x008c1ef0` | `GetAbsOrigin` → `this+0xD8`（先调网络更新） |
| 主 | 128 | `0x008bbe80` | 队伍号 → `*(int*)(this+0x12C)` |
| 主 | 167 | `0x00a1ade0` | 生命值 → `*(int*)(this+0x138)` |
| 主 | 208 | `0x0084fe20` | `IsAlive` → `*(char*)(this+0x297) == 0` |
| 主 | 210 | `0x008fb5a0` | `IsPlayer` → `return 1` |
| renderable | 13 | `0x00fe2620`→`0x00fe25d0` | `SetupBones(matrix*, max, mask, time)`（转发 `0x008a7ba0`） |
| networkable | 9 | `0x008bbf30`→`0x008bbf00` | `IsDormant` → index!=-1 时读 `this+0x125` |
| networkable | 10 | `0x008bbae0`→`0x008bbad0` | `GetIndex` → `*(int*)(this+0x94)` |
| 武器表 | 551/552 | `0x10182f0`/`0x101d770` | `GetInaccuracy`/`GetSpread`（§7，武器名实测正常） |

### 11.4 `VEngineClient014`（vtable `0x00e64038`）

| 槽 | vaddr | 已核对语义 |
|---|---|---|
| 8 | `0x00e64078` | `GetPlayerInfo(int, void*)`，拷贝 0x158 字节 |
| 18 | `0x003798f0` | `GetViewAngles(Vector*)`（写 3 个 float） |
| 19 | `0x00379a00` | `SetViewAngles(const Vector*)`（含 NaN 检查） |
| 20 | `0x003794f0` | `GetMaxClients()` → `CClientState+0x420` |
| 37 | `0x00378670` | `WorldToScreenMatrix()` thunk（投影实测正常） |
| 78 | `0x003795b0` | `GetNetworkChannel()` → `CClientState+0x128` |

### 11.5 本地玩家 finder

`83 FF FF 48 8B 05` 在 `client_client.so` 中**唯一**命中 `0x008f4e11`：
`cmp edi,-1; mov rax,[rip+disp]` → 全局 `DAT_024847e8`；
`FUN_008f4e10(index)` 即 `(&DAT_024847e8)[index]`，且
`ClientModeCSNormal::CreateMove`（槽 25）以 `FUN_008f4e10(-1)` 取本地玩家。

### 11.6 保留但仅行为验证（未重新反编译）

* 武器 vtable 523/529（武器名、按类配置实测正常）。
* `VClientEntityList003` 槽 3（ESP/rage 实体解析实测正常）。
* `VEngineClient014` 槽 37（投影实测正常）。

### 11.7 本次删除清单

no-fog / no-scope / viewmodel FOV 钩子与 UI、glow 全模块、prediction 全模块、
`DispatchUserMessage` 钩子（仅 trace 日志）、`UpdateClientSideAnimation` 钩子、
`SetAbsOrigin`、`line_goes_through_smoke`、`is_other_enemy`、`view_render_beams`、
`weapon_system`、`player_resource_slot`、`planted_c4s`、`game_rules_slot`、
`view_render`、`send_clan_tag`、`send_datagram`、所有 prediction pattern、
entity 348/409/218。

---

## 10. 仍待验证（接 `csgo-re-f3.md`）

- `CCSPlayerAnimState`：`eyeYaw +0x28` 读数为 0（live bot），`footYaw +0x84` 同样为 0；
  需换场景/时间复测或重新定位（A3）。
- `restoreEntityToPredictedFrame`、`m_nPredictionRandomSeed`（B1/B3）。
- `CBaseAnimating::SetAbsAngle`、`m_angAbsRotation`（A5/A6，fake matrix 前置）。
- `CClientState`：`deltaTick/lastCommandAck/lastOutgoingCommand/chokedCommands/paused`（C2）。
- `sv_maxusrcmdprocessticks` / `SendDatagram` 参数（Tickbase/DT 前置）。
- `C_BasePlayer::m_nButtons/m_afButtonForced`（B5）。
