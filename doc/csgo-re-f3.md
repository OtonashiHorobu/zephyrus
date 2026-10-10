# zephyrus-csgo F3 reverse engineering (Ghidra)

> ABI / vtable / 结构体偏移的实测汇总见 [csgo-re-abi.md](csgo-re-abi.md)（Ghidra + core dump
> 验证，含本次修复的错误索引）。
>
> 目标：为 `zephyrus-csgo` 的 F3 功能补齐 **legacy CS:GO 1.38.8.1 linux64**
> 的偏移/函数/索引。所有结果写入本文件底部的 `FINDINGS` 代码块，并同步更新
> `include/zephyrus/csgo/offsets.hpp` 与 `src/zephyrus/csgo/patterns.cpp`
> 中对应占位（0/NULL）。
>
> 程序（Ghidra 项目 `csgo`）：
> - `/client/client_client.so`（完整分析已完成，~82k 函数）
> - `/engine/engine_client.so`
>
> 地址约定：Ghidra 地址 = ELF vaddr + 0x100000（每个 program 先用
> `get_metadata` 确认）。报告 **模块内偏移**（Ghidra 地址 - base）。
>
> 交付格式：每项给出 模块偏移、运行期可用的 **字节 pattern**（IDA 风格，
> 用 `search_byte_patterns` 验证唯一性并报告命中数）、证据（函数地址 +
> 反编译/交叉引用片段）、置信度（high/medium/low）。vtable 索引另附 vtable
> 槽地址与函数地址。

## 请求清单（按优先级）

### A. AnimState / 动画（anti-aim 真实 desync、resolver、前向 fake matrix）

| # | 项目 | 用途 | 备注 |
|---|---|---|---|
| A1 | `C_CSPlayer::UpdateClientSideAnimation` 函数偏移 + **实体 vtable 索引** + pattern | hooks（F3 未完成项） | 它调用 `CCSPlayerAnimState::Update`；Osiris Windows 索引 224 |
| A2 | `C_CSPlayer::m_PlayerAnimState` 指针偏移（Linux） | 读写 AnimState | Windows O&E: 0x3914 |
| A3 | `CCSPlayerAnimState` 字段偏移 | resolver 检测、desync、动画框架 | Windows 布局见 O&E AnimState.h |
| A4 | `CCSPlayerAnimState::Update` + `C_CSPlayer::CreateAnimState` + pattern | 本地 fake matrix 复刻 | |
| A5 | `C_BaseAnimating::SetAbsAngle` 函数偏移 + pattern | fake matrix 旋转 | O&E Windows pattern |
| A6 | `C_BaseEntity::m_angAbsRotation` 偏移 | fake matrix / resolver | |
| A7 | `C_BaseAnimating::m_flPoseParameter` 偏移 | 动画框架 | |
| A8 | `C_BaseAnimating::m_AnimOverlay` 偏移 + `AnimationLayer` 布局 | resolver desync 检测 | |

### B. Engine prediction

| # | 项目 | 用途 |
|---|---|---|
| B1 | `restoreEntityToPredictedFrame(int, int)` | 手动 prediction 后还原实体 |
| B2 | `m_nPredictionPlayer` 全局 | prediction 前置 |
| B3 | `m_nPredictionRandomSeed` 全局 | prediction / no-spread |
| B4 | `CGlobalVars` Linux 偏移复核 | prediction |
| B5 | `m_nButtons` / `m_afButtonForced` | prediction 按钮状态 |
| B6 | `VClientPrediction001` / `GameMovement001` 可取性 | prediction 接口 |

### C. ClientState / 网络

| # | 项目 |
|---|---|
| C1 | `CClientState` 全局（engine） |
| C2 | `deltaTick` / `lastCommandAck` / `lastOutgoingCommand` / `chokedCommands` / `m_NetChannel` / `paused` |
| C3 | `CNetChan::SendDatagram` |
| C4 | `CNetChan::chokedPackets` |
| C5 | `CGlobalVars::serverTime()` curtime 路径 |

### D. 其他

| # | 项目 |
|---|---|
| D1 | `GetMaxDesyncDelta` / `aimYawMin` / `aimYawMax` |
| D2 | `C_CSPlayer::GetModelPtr`（`m_pStudioHdr`） |
| D3 | `CStudioHdr` / hitbox 布局 |
| D4 | `CGameMovement::ProcessMovement` |
| D5 | `VPhysicsSurfaceProps001` |
| D6 | `IsBreakable` / 玻璃穿透 |
| D7 | `RandomSeed` / `RandomFloat` dlsym |

## 分析方法摘要

- 用 Python + pyelftools 直接解析 `.rela.dyn`（`R_X86_64_RELATIVE`）与
  `.data.rel.ro`，由 RTTI 名称字符串（`_ZTS...`）反查 `_ZTI`，再由
  `_ZTI` 反查 vtable，从而定位 `C_CSPlayer` / `CBasePlayerAnimState` /
  `CCSPlayerAnimState` 的 vtable。
- 用 Ghidra `decompile_function` / `disassemble_bytes` / `get_xrefs_to` /
  `search_byte_patterns` 交叉验证。
- 字符串 xref（`cl_showanimstate` 的 ConVar 对象 `0x24734e0`）定位
  `CBasePlayerAnimState::Update`。

### 已确认的关键 vtable

| 类 | vtable 模块偏移 | 说明 |
|---|---|---|
| `C_CSPlayer` primary | `0x21C7B00` | slot 208=`isAlive`、210=`isPlayer`（`return 1`）、218=`isWeapon`（`return 0`），与 Osiris Linux 索引一致 |
| `CBasePlayerAnimState` primary | `0x20FE218` | 34 槽；slot 1 = `Update`(0x768370) |
| `CCSPlayerAnimState` primary | `0x21D60C0` | 覆盖 slot 1 = `0xF3E360` |

`C_CSPlayer` 索引自检（Osiris Linux 值）：`getAbsOrigin` 12 → slot12 写 `[this+0x2c4]`（origin 分量）；
`isPlayer` 210 → `return 1`；`isAlive` 208 → `return *(char*)(this+0x297)==0`；`isWeapon` 218 → `return 0`。

---

## 结果表

| # | 项目 | 模块偏移 | vtable 索引 | pattern（唯一命中数） | 置信度 |
|---|---|---|---|---|---|
| A1 | UpdateClientSideAnimation 最近实现 | `0xECFC90`（FUN_00fcec90，读 `[this+0xA248]` 并 `call [anim+8]`） | UNKNOWN（Win 224；Linux 未定） | 无（见正文） | low |
| A2 | `m_PlayerAnimState` | `0xA248` | — | 无（结构偏移） | high |
| A3 | `eyeYaw` | `0x28` | — | — | high |
| A3 | `eyePitch` | `0x2C` | — | — | high |
| A3 | `moveYaw` | `0x6C` | — | — | medium |
| A3 | `footYaw` | `0x84` | — | — | medium |
| A3 | `footYawLast` | `0x34` | — | — | medium |
| A3 | `lastUpdateTime` | `0x74` | — | — | low |
| A4 | `CBasePlayerAnimState::Update` | `0x768370` | 1 | `55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB 48 83 EC 18 4C 8B 25 ?? ?? ?? ?? F3 0F 11 45 C8 F3 0F 11 4D C4` (1) | high |
| A4 | `CCSPlayerAnimState::Update`（derived override） | `0xF3E360` | 1（anim vtable） | — | medium |
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
| A8 | `prev_cycle`/`playback_rate` | UNKNOWN（疑似 `0x20`/`0x2C`） | — | — | low |
| B1 | restoreEntityToPredictedFrame | UNKNOWN | — | — | — |
| B2 | `m_nPredictionPlayer` | `0x237CC80` | — | `41 8D 56 FF 31 C9` +`(-14).abs()` (1) | high |
| B3 | `m_nPredictionRandomSeed` | UNKNOWN（见正文，Osiris 的 Linux 锚点实为 B2） | — | — | low |
| B4 | CGlobalVars | 沿用现有 | — | — | low |
| B5 | `m_nButtons`/`m_afButtonForced` | UNKNOWN | — | — | — |
| B6 | `VClientPrediction001`/`GameMovement001` | 可创建（字符串已注册） | — | — | medium-high |
| C1 | `CClientState` getter | `0x2B48D0`（getter）；全局槽 `0xE27A08` | — | `48 8B 05 ?? ?? ?? ?? 55 48 89 E5 48 83 C0 08 83 FF FF` (1) | medium |
| C2 | `signonState` | `0x1A0` | — | — | high |
| C2 | `m_NetChannel` | `0x128` | — | — | medium |
| C2 | `deltaTick`/`lastCommandAck`/`lastOutgoingCommand`/`chokedCommands`/`paused` | UNKNOWN | — | — | — |
| C3 | `CNetChan::SendDatagram` | `0x4AA000` | — | `55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 53 48 89 F3 48 81 EC A8 01 10 00 64 48 8B 04 25 28 00 00 00 48 89 45 C8` (1) | high |
| C4 | CNetChan chokedPackets | UNKNOWN | — | — | — |
| C5 | curtime 路径 | `PTR_DAT_023f7d28`(+0x10) = gpGlobals->curtime | — | — | medium |
| D1 | GetMaxDesyncDelta / aimYawMin/Max | UNKNOWN | — | — | — |
| D2 | `m_pStudioHdr` | `0x2FD8` | — | — | medium-high |
| D3 | CStudioHdr/hitbox 布局 | UNKNOWN | — | — | — |
| D4 | ProcessMovement | 索引 1（doc 已确认）；地址 UNKNOWN | 1 | — | low |
| D5 | `VPhysicsSurfaceProps001` | 字符串 `0x195706C`，被 `FUN_00ab2d50` 引用 | — | — | medium |
| D6 | IsBreakable | UNKNOWN | — | — | — |
| D7 | `RandomSeed`/`RandomFloat` | libvstdlib_client.so dynsym 导出（见正文） | — | — | high |

---

## 证据明细

### A1 — UpdateClientSideAnimation（最近实现）

`C_CSPlayer` 主 vtable 位于模块偏移 `0x21C7B00`。由 Osiris Linux 已知索引校验：
slot 208（`isAlive`）= `FUN_0084fe20`：`return *(char*)(this+0x297)==0`；
slot 210（`isPlayer`）= `FUN_008fb5a0`：`return 1`；
slot 218（`isWeapon`）= `FUN_0084fcf0`：`return 0`。索引体系与 Osiris 一致。

唯一读取 `m_PlayerAnimState`（`[this+0xA248]`）的函数是 **`FUN_00fcec90`（模块 `0xECFC90`）**：

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

`param_1[0x1449]` = `+0xA248`，`+8` 即 animstate vtable 槽 1（`Update`）。因此
`FUN_00fcec90` 是驱动 animstate 更新的最近实现（疑似 `ClientThink`/`PostThink`
的一部分，其中 `*param_1 + 0x6f8` = vtable 索引 223）。

**未能确定 `C_CSPlayer::UpdateClientSideAnimation` 的 Linux vtable 索引**：Ghidra
中 `0x21C7B00` vtable 里没有指向 `0xECFC90` 的槽；Osiris Windows 索引 224 在 Linux
对应槽 224 = `0x780940`（`return this+0x39a4`，一个 getter，不是动画更新）。
尝试过：vtable 全表扫描（0..1000）、按 `[reg+0xA248]` 的各种寄存器/SIB 编码扫描、
按 `call [reg+8]` + `mov reg,[base+disp32]` 组合扫描（`80 BF ... 00 74 36` 等），
均未得到确定的 vtable 槽。**结论：A1 的 vtable 索引为 UNKNOWN，low 置信度，
未写入源码。**

### A2 — `m_PlayerAnimState` = `0xA248`（high）

1. 实体构造函数（Ghidra `0xFD4990` 附近）在创建动画状态后立即保存：

```
00fd49e2  CALL 0x0103a030          ; C_CSPlayer::CreateAnimState()
00fd49e7  MOV  qword ptr [RBX + 0xa248], RAX   ; m_PlayerAnimState = 返回值
00fd49f1  CALL 0x0103eef0
00fd49fd  MOV  qword ptr [RBX + 0xa250], RAX   ; 另一个指针（非 animstate）
```

2. `FUN_00fcec90` 从 `+0xA248` 取出指针并以 vtable 槽 1 调用（见 A1）。
   两处独立证据一致，故 `player_anim_state = 0xA248`。

### A3 — `CCSPlayerAnimState` 字段（相对 AnimState 对象）

`CBasePlayerAnimState::Update`（`FUN_00868370`，模块 `0x768370`）开头：

```c
uVar8 = FUN_014eed70(param_1); *(undefined4 *)(param_3 + 5) = uVar8;   // +0x28 = eyeYaw
uVar8 = FUN_014eed70(param_2); *(undefined4 *)((long)param_3 + 0x2c) = uVar8; // +0x2c = eyePitch
```

（`param_3` 是 AnimState；`param_3[4]` = `+0x20` = 实体指针。）

- `ComputePoseParam_BodyPitch`（`FUN_008650d0`）：`fVar2 = *(float*)(param_1+0x2c)` → 写
  `body_pitch` 姿态参数。**eyePitch = 0x2C**。
- `ComputePoseParam_MoveYaw`（`FUN_008661e0`）：`fVar8 = *(float*)(param_1+5)`（=`+0x28`，
  eyeYaw）与 `*(float*)((long)param_1+0x84)`（footYaw）作差，写 `+0x6C`（moveYaw），
  并写 `+0x58`/`+0x5C`（move_x/move_y）；`+0x34` 为 footYawLast。
- 调试函数 `FUN_008673e0` 打印 `ent yaw=+0x50, body_yaw=@0x2473104, move_yaw=@0x2473100,
  gait_yaw=+0x84, body_pitch=@0x2473108`；其中 `gait_yaw` 对应 `+0x84`。
- `FUN_00864080`（vtable 槽 3）：`*(float*)((long)param_1+0x74) = curtime` → 疑似 lastUpdateTime。
- `FUN_008648b0`（vtable 槽 30）把速度方向 `atan2(vel.y,vel.x)*57.29…` 写入 `+0x30`（moveYawIdeal）。

已确认：`eyeYaw=0x28`、`eyePitch=0x2C`（high）；`moveYaw=0x6C`、`footYaw=0x84`、
`footYawLast=0x34`（medium）；`lastUpdateTime=0x74`（low）。
`moveWeight / animDuckAmount / duckAdditional / onGround / walkToRunTransition /
velocityLengthXY / speedAsPortionOfRunTopSpeed` 本次未定位（UNKNOWN）。

### A4 — AnimState Update / CreateAnimState

- `CBasePlayerAnimState::Update` = `FUN_00868370`（模块 `0x768370`），
  vtable 槽 1（`0x20FE218+8`）。它读取 ConVar `cl_showanimstate`
  （`get_xrefs_to 0x24734e0` → `FUN_00868370`），设置 `eyeYaw/eyePitch`，
  依次调用 vtable 槽 `+0x58`（ClearAnimationLayers）、`+0x60`、`+0x68`、
  `+0xE8`（BodyPitch）、`+0xF0`、`+0xE0`（MoveYaw）。pattern 唯一命中 1。
- `CCSPlayerAnimState::Update`（derived 覆盖）= `FUN_00f3e360`（模块 `0xF3E360`），
  在 `0x103e427` 直接调用基类 `0x868370`。
- `C_CSPlayer::CreateAnimState` = `FUN_0103a030`（模块 `0xF3A030`）：

```
0103a048  MOV  EDI, 0x250          ; sizeof(CCSPlayerAnimState) = 0x250
0103a052  CALL operator new
0103a05d  CALL 0x01039d30          ; CCSPlayerAnimState ctor
0103a0f6  CALL 0x01039fa0          ; Init(state, entity, ...)
```
`FUN_00865560`（被 `FUN_01039fa0` 调用）确认 `state+0x20 = entity`：
`param_1[4] = param_2;`（`param_1`=state, `param_2`=实体）。

### A7 — `m_flPoseParameter` = `0x2D80`（high）

`C_BaseAnimating::GetPoseParameter`（Ghidra `0x8A1730`）：

```
MOV  RDI, [RDI + 0x2fd8]              ; CStudioHdr*
MOVSXD RAX, ESI
MOVSS XMM0, [RBX + RAX*4 + 0x2d80]    ; m_flPoseParameter[index]
```

`C_BaseAnimating::SetPoseParameter`（`FUN_00898aa0`）写
`*(float*)(param_1 + 0x2d80 + index*4)`。故 `m_flPoseParameter = 0x2D80`。
同时得到 `m_pStudioHdr = 0x2FD8`（D2）。

### A8 — `m_AnimOverlay` 与 `AnimationLayer`（high）

`C_BaseAnimating::GetNumAnimOverlays` = `FUN_008b14c0`：

```
return *(int *)(param_1 + 0x3030);       // count
```

`C_BaseAnimating::GetAnimOverlay` = `FUN_008b1450`：

```c
iVar2 = *(int *)(param_1 + 0x3030);          // count
lVar3 = *(long *)(param_1 + 0x3020);         // data pointer
...
while (lVar5 = lVar4 + 0x48, ...)            // stride = 0x48 (72)
    piVar1 = (int *)(lVar4 + 0x60);          // = next layer + 0x18 -> order
```

`AnimationLayer` 字段（由 setter 确认）：
- `FUN_008b0e10`：`*(int*)(layer+0x18) = v`（且判 `==0xf`）→ **order = 0x18**
- `FUN_008b0ce0`：`*(int*)(layer+0x1c) = v` → **sequence = 0x1C**
- `FUN_008b0e80`：`*(float*)(layer+0x24) = v` → **weight = 0x24**
- `FUN_008b0d70`：`*(float*)(layer+0x30) = v` → **cycle = 0x30**
- `FUN_008b0e10/0ce0/0d70/0e80` 均使用 `*(long*)(layer+0x38)` 作 owner 通知 → **owner = 0x38**

故 `anim_overlay_vector = 0x3020`、`anim_overlay_count = 0x3030`、stride `0x48`。
`prev_cycle`/`playback_rate` 未直接证实（疑似 `0x20`/`0x2C`，low）。

### B2 — `m_nPredictionPlayer` = `0x237CC80`（high）

pattern `41 8D 56 FF 31 C9` 在 client 中唯一命中 Ghidra `0x861961`。
Osiris Linux 链 `(-14).abs()` 解析到 Ghidra `0x247CC80`（模块 `0x237CC80`）：

```
00861950  LEA  RAX, [0x247cc80]        ; 全局
00861957  CMP  qword ptr [RAX], 0x0
00861961  LEA  EDX, [R14 + -0x1]       ; pattern 起点
00861965  XOR  ECX, ECX
```

该全局被下列函数写为**实体指针**（证明是 `m_nPredictionPlayer`，不是随机种子）：

```c
// FUN_00ad17b0  (module 0x9D17B0)
*(long *)(param_2 + 0x3b28) = param_3;
...
DAT_0247cc80 = param_2;                 // m_nPredictionPlayer = entity

// FUN_00ad1a50  (module 0x9D1A50)
DAT_0247cc80 = 0;                       // m_nPredictionPlayer = nullptr
```

`prediction_player` 用与 `prediction_random_seed` 相同的 pattern 链即可得到
`0x237CC80`。**注意：Osiris 的 Linux `predictionRandomSeed()` 实际上解析到本全局，
即它在此 build 上返回的是 player 指针而非 int 随机种子（见 B3）。**

### B3 — `m_nPredictionRandomSeed`

未定位到独立的 int 随机种子全局。`41 8D 56 FF 31 C9` 锚点（现 `patterns.cpp`
的 `prediction_random_seed`）解析到 `0x237CC80`，但该地址的读写语义是
`C_BasePlayer*`（B2 证据）。因此 **B3 = UNKNOWN（low）**，现有源码链保留不动。

### B6 — 接口注册

- `VClientPrediction001` 字符串模块偏移 `0x19D0D70`（含前导 `?` 字节），
  被 `_INIT_446`（Ghidra `0x7DD073`）引用 → 由 `CreateInterface` 注册。
- `GameMovement001` 字符串模块偏移 `0x19C39E6`，被 `FUN_007D6BE0`（`0x7D75C2`）引用。
- `VClient018` `0x1945F14`、`VEngineClient014` `0x1945D24`、`VClientEntityList003`
  `0x19479B6`、`VPhysicsSurfaceProps001` `0x195706C` 同样存在。
  结论：client 的 `CreateInterface` 能取到 `VClientPrediction001` 与
  `GameMovement001`（medium-high）。IPrediction 索引沿用文档已知值
  `setupMove 20 / finishMove 21`；`CGameMovement::processMovement 1`。

### C1 — `CClientState` getter / 全局槽

`FUN_003b48d0`（engine 模块 `0x2B48D0`）：

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

pattern 唯一命中 1。全局槽地址 = engine 模块偏移 `0xE27A08`（Ghidra `0xF27A08`），
对象为 `[0xE27A08]`（getter 返回 `+8`）。`CBaseClientState::RunFrame`
（`FUN_0034be30`）与 `CClientState::SetSignonState`（`FUN_003c8820`）均以该对象为
`this`。置信度 medium（`+8` 的语义未完全厘清）。

### C2 — `CClientState` 字段

- `signonState`：`FUN_003c8820` 中 `switch(*(undefined4 *)(param_1 + 0x1a0))`；
  `FUN_0034be30` 中 `iVar8 = (int)param_1[0x34]`（= `+0x1A0`）。**= 0x1A0（high）**。
- `m_NetChannel`：两函数中 `*(long **)(param_1 + 0x128)`，并以
  `[chan_vtable + 0x148/0x100/0x200]` 调用 CNetChan 方法。**= 0x128（medium）**。
- `deltaTick` / `lastCommandAck` / `lastOutgoingCommand` / `chokedCommands` / `paused`
  未定位（UNKNOWN）。

### C3 — `CNetChan::SendDatagram` = `0x4AA000`（high）

`FUN_005aa000`（engine 模块 `0x4AA000`）内部包含 profiler 名称检查：

```
if ((char *)*_DAT_044477c0 != "CNetChan::SendDatagram")
    CVProfNode::GetSubNode(..., 0xa6c7cb, ...);   // 0xA6C7CB = 该字符串
```

字符串 `CNetChan::SendDatagram` 模块偏移 `0x96C7CB`，`get_xrefs_to` 指向
`FUN_005aa000`。签名 `int FUN_005aa000(long *chan, void *data)` 与
`int(void* this, void* data)` 一致。pattern 唯一命中 1。

### D5 — `VPhysicsSurfaceProps001`

字符串模块偏移 `0x195706C`，被 `FUN_00ab2d50`（Ghidra `0xAB2D74`）引用 → 接口可创建（medium）。

### D7 — dynsym 导出

`libvstdlib_client.so` 的 `.dynsym` 导出（可 `dlsym(RTLD_DEFAULT, ...)`）：
`_ZN20CUniformRandomStream9SetSeedEi` @ 0x1e3e0、`_ZN20CUniformRandomStream11RandomFloatEff` @ 0x1e750、
`_ZN20CUniformRandomStream9RandomIntEii` @ 0x1e840、`RandomSeed` @ 0x1e4a0、
`RandomFloat` @ 0x1e4c0、`RandomInt` @ 0x1e500。
client/engine 的 `.dynsym` 把 `CUniformRandomStream` 三个方法列为 **未定义导入**（`U`），
说明它们在运行时从 vstdlib 解析；`MD5_PseudoRandom` 未在 client/engine/vstdlib 的动态符号表中
找到（可能为本地符号）。`SendDatagram`、`EngineTraceClient` 未作为动态符号导出。

---

## 源码改动（仅 high 置信度）

`include/zephyrus/csgo/offsets.hpp`：
`player_anim_state=0xA248`、`anim_overlay_vector=0x3020`、`anim_overlay_count=0x3030`、
`anim_state.eye_yaw=0x28`、`anim_state.eye_pitch=0x2C`。
其余（`update_client_side_animation_vtable`、`client_state_*`、`player_buttons`、
`player_button_forced`）保持 0（UNKNOWN / medium）。

`src/zephyrus/csgo/patterns.cpp`：
新增 `update_anim_state`（0x768370）、`prediction_player`（0x237CC80）、
`send_datagram`（0x4AA000），pattern 均已在 Ghidra 验证唯一命中 1。
`update_client_side_animation`、`create_anim_state`、`restore_entity_to_predicted_frame`、
`trace_to_exit`、`client_state` 保持 null。

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
