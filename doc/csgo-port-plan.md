# zephyrus-csgo 移植计划（CS:GO HvH）

> 创建：2026-10-09 · 状态：Phase 1 启动
> 关联 TODO：[todo-shared.md](todo-shared.md) · [todo-csgo.md](todo-csgo.md)
> 约束：仅 `-insecure` 环境使用；不做反作弊绕过。
> 历史文档：ACR 已拆到 `acr` 分支并冻结（见 [BRANCHES.md](../BRANCHES.md)）；
> 文中涉及 ACR 的目标、目录与验收项均为拆分前的历史状态。

## 1. 目标

- 新增 `zephyrus-csgo` 共享库：注入 CS:GO（legacy 1.38.8.1, linux64）客户端，
  提供 HvH 完整功能（主要参考 OsirisAndExtra 的功能集）。
- 现有基础设施（hook 引擎、RmlUi 菜单、TOML 配置、日志、命令通道）最大化复用。
- 仓库按“游戏 / shared”分块：目录、CMake 目标、TODO 三者都分开维护。

## 2. 已验证事实（2026-10-09）

| 项 | 结论 |
|---|---|
| 游戏 | `/datassd/SteamLibrary/.../Counter-Strike Global Offensive`，legacy CS:GO `1.38.8.1` / ClientVersion `1575` / VersionDate `2023-10-12`，x86-64（`csgo/steam.inf`） |
| 模块 | `csgo/bin/linux64/client_client.so`（35 MB）、`bin/linux64/engine_client.so`（12.6 MB）；均 not stripped，但 `.dynsym` 仅 138 个符号（`CreateInterface`、`s_pInterfaceRegs`）；`.gnu_debuglink` 指向未随游戏分发的 `client_client.so.dbg` |
| Osiris patterns | csgo 分支（tip `8677ddf1`, 2025-07）的 **Linux** pattern：client 命中 60/61、engine 7/7；唯一缺失是 `uiComponentInventory`（库存功能，不需要） |
| SDL2 | `bin/linux64/libSDL2-2.0.so.0`；所有导出函数为 `FF 25 rel32` thunk → 可像 Osiris 一样替换指针槽，零代码补丁 hook `SDL_PollEvent` / `SDL_GL_SwapWindow` |
| GL | `shaderapidx9_client.so` 含 “SDL failed to create GL compatibility profile” 字符串 → 游戏主动申请 **compat profile**；Osiris Linux 用 GL3 ImGui 在同版本游戏渲染成功 |
| Ghidra | MCP 实例在线；项目 `csgo`（`/datassd/Clone/acr-bin/ghidra/csgo`）已建但 **尚无 program** |
| 许可 | Osiris = MIT（可移植，保留版权声明）；OsirisAndExtra = CC BY-NC-SA 4.0（**仅参考逻辑，代码重写**） |
| 注入前提 | 用户已在 Steam 设置 `-insecure`；库与启动脚本都要强校验，缺失即拒绝初始化 |

## 3. 分块架构

### 3.1 目录

```
include/zephyrus/
  abstract/ hook/ log/ platform/ util/ asset/   # shared
  config/ input/ rmlui/ sdl/ sdl2/              # shared（sdl/sdl2 各自独立）
  acr/          # ACR 游戏模块（现有，不删）
  csgo/         # ★ 新：SDK / interfaces / netvars / patterns / memory / entity / anim …
  features/acr/ # 现有功能移入
  features/csgo/# ★ 新：rage / antiaim / fakelag / resolver / backtrack / esp / chams / glow / misc …
  overlay/acr/  # 现有 overlay + menu 移入
  overlay/csgo/ # ★ 新 overlay + menu
  overlay/menu_base.hpp        # ★ 从现有 menu 抽出的通用控件机制
src/ 同构；src/zephyrus_acr.cpp、src/zephyrus_csgo.cpp 为入口
asset/ui/csgo/  # ★ CS:GO 菜单 RML
```

命名空间约定：`zephyrus::csgo`（游戏/功能）、`zephyrus::csgo::overlay`；
ACR 暂保留 `zephyrus::acr` / `zephyrus::features` / `zephyrus::overlay`（减少 churn，
后续可在 todo-acr 里做命名空间整理）。

### 3.2 CMake 目标

```
zephyrus-core       STATIC  shared：hook / log / config / rmlui / cmrc / version / zephyrus
zephyrus-acr-core   STATIC  ACR 模块 + features/acr + overlay/acr
zephyrus-csgo-core  STATIC  CS:GO 模块 + features/csgo + overlay/csgo
zephyrus-acr        SHARED  入口（链接 acr-core + version-script）
zephyrus-csgo       SHARED  入口（链接 csgo-core + version-script）
zephyrus-standalone EXE     继续只做 ACR 菜单预览（链接 acr-core）
```

### 3.3 渲染后端（回答 SDL2/GL3 问题）

- **SDL2 必用**：CS:GO 层新写 `sdl2` 函数表和 `system_interface_sdl2`
  （事件/光标/剪贴板），从游戏已加载的 `libSDL2-2.0.so.0` 解析，不链接。
- **渲染先复用 GL2**（`render_interface_gl2`，已含 layer/filter 实现）：
  游戏申请 compat profile，固定管线可用；这是达到“可用版本”的最短路径。
- **保留 GL3 升级位**：Conan 缓存中 RmlUi 6.2 自带官方
  `RmlUi_Renderer_GL3`（实现完整 layer/filter API），如注入冒烟测试发现
  core-profile / 状态冲突问题，立即移植。冒烟测试项：记录
  `GL_VERSION` / `GL_CONTEXT_PROFILE_MASK`，验证固定管线可用。
- 无论哪种后端，注入路径都要补 `begin_frame/end_frame` 的完整 GL 状态保存/恢复
  （program / VAO / depth / cull / active texture / matrices）。
- `render_interface_gl2` 目前 `#include <SDL/SDL.h>` + SDL_image 纹理加载；
  抽出 `texture_loader` 接口：ACR 用 SDL1.2，CS:GO 用 SDL2（菜单无图片资源，
  可只留字体路径）。

## 4. 阶段计划

### Phase 0 — 准备（可与 Phase 1 并行）
- [ ] Ghidra 导入 `client_client.so` / `engine_client.so`（后台自动分析）。
- [ ] Osiris Linux pattern 扫描脚本入库（`scripts/` 或 `tools/`），对每次游戏更新校验。
- [ ] `scripts/csgo-start.sh`：LD_PRELOAD 注入 + `-insecure` 断言。
- [x] 版本/BuildID 探测与 pattern 可用性验证（见 §2）。

### Phase 1 — 分块与共享基础设施（进行中）
ACR 分块搬迁 + 构建拆分已完成（构建/测试绿色，历史记录见 `acr` 分支）；
`rmlui_manager` 已与 SDL1.2 解耦；SDL2 模块/menu base 抽取在 [todo-shared.md](todo-shared.md)。
验收：ACR 构建 + 测试 + standalone + 客户端实测不回归（实测项待做）。

### Phase 2 — CS:GO 基础（注入 → 画帧）（进行中）
已完成：`zephyrus-csgo` 目标 + 入口（`-insecure` 守卫/版本校验/模块/接口/pattern 探针）
+ `zephyrus_csgo_probe` + `scripts/csgo-start.sh`；库可在无 `-insecure` 时安全拒初始化。
见 [todo-csgo.md](todo-csgo.md)。剩余：SDL2 叠加层、菜单、实际画帧。

### Phase 3 — HvH 可用版本（v1）
参考 OsirisAndExtra 功能集：Ragebot（hitchance/autofire/autoscope/autostop/
no-recoil/autowall/backtrack 集成）、Triggerbot、Backtrack、AntiAim、Fakelag、
Resolver v1、ESP（盒子/血量/名字/武器/骨架）、Bhop/AutoStrafe、菜单+配置。
验收：本地 `-insecure` 服务器 + bots 可打；双客户端/LAN 可验证 desync 行为；
社区 HvH 服由用户复测。

### Phase 4 — OsirisAndExtra 其余功能
Chams、Glow、Visuals（去烟/去闪/十字线系列/scope 覆盖/天空盒等）、Misc
（spectator list/bomb timer/hitmarker/watermark…）、EnginePrediction、Knifebot、
GrenadePrediction、Tickbase/DT（引擎级，最后做）。

### Phase 5 — Resolver / AA 迭代（持续）
按 §5 设计打磨，配合 hit log 与实测数据调参。

## 5. HvH 设计：AntiAim / Resolver 改进

参考 OsirisAndExtra，但按下列方向**重写增强**（不逐字复制其 CC 代码）。

### 5.1 AntiAim
1. **精确 desync**：同时用 `animstate` 字段与 `m_flPoseParameter[6]` 推导真实
   躯干 yaw；按 duck/air/move 修正 `58° * modifier`，而不是固定值。
2. **LBY 精确利用**：复刻服务端更新条件（移动 >0.1 或 z>100 后 0.22s，否则 1.1s），
   Sway/Spin 在窗口内保持脚部 yaw 连续变化；避免 O&E 的固定 flip。
3. **Jitter/Spin/Freestanding**：在 `-insecure` 下允许 roll；按敌人位置做
   freestanding 扫描（多 yaw 采样 + 遮挡评估），而不是单次 auto-direction。
4. **On-shot AA**：开火包与假包分离（配合 fakelag/tickbase 的 hide-shots），
   射击时保持假 yaw，避免被 shot-resolver 抓真实角度。
5. **反解析**：jitter 周期/幅度随机化（seeded PRNG），不依赖可预测的
   `sendPacket` 翻转；按武器/移动状态分档（O&E 已有分档，保留并扩展）。
6. **Manual**：左/右/后按键 + auto-direction，保留。

### 5.2 Resolver
1. **Per-record 解析**：每条 backtrack record 捕获 eye/foot yaw、pose、anim layers、
   simtime、velocity、matrix；解析在 record 副本上重建矩阵，**不改动活体 animstate**
   （O&E 改动 live animstate 会污染渲染/回溯）。
2. **检测器栈 + 置信度**：static desync（layer3/lean + pose 符号）、jitter（相邻
   choke 包的 footYaw 跳变方向/幅度）、LBY（目标更新时序）、spin（角速度估计 +
   按延迟预测）、freestanding（trace 侧向可达性）。
3. **Shot resolver**：对开火中的敌人用 fire yaw/impact 反推真实角度；
   `weapon_fire` / `bullet_impact` / `player_hurt` / `player_death` 事件做反馈。
4. **Brute-force 状态机**：角度桶（如 ±60/±40/±20/0）按玩家持久化命中统计，
   未命中的角度进黑名单，死/回合切换按策略重置；miss 分类
   （resolver miss vs spread/无伤害），只对 resolver miss 调整。
5. **与 aimbot 协同**：优先解析后的 hitbox；hitchance/autowall 基于解析矩阵；
   log 显示解析模式/侧向/命中率。
6. **可视化调试**：菜单显示每玩家 resolve 模式、命中率、最近 miss；命令通道可 dump。

### 5.3 需要自行逆向的 Linux 偏移（Phase 0/3 用 Ghidra）
- `CCSPlayerAnimState`：`eyeYaw`/`footYaw`/`lastUpdateTime`/`duckAdditional`/
  `onGround`/`walkToRunTransition`（Osiris 只给了 maxDesync 相关字段）。
- `C_BaseAnimating`：`m_flPoseParameter` 读写、`GetPoseParameter/SetPoseParameter`。
- 动画层结构（`m_AnimOverlay` / `CAnimationLayer`：weight/cycle/playbackrate/order）。
- `UTIL_TraceToExit`（Linux autowall；Osiris 的 Windows asm 在 Linux 返回 false）。
- `CNetChan`/`CClientState`：`chokedPackets`、`m_nChokedCommands`、`SendDatagram`。
- 随机种子：`m_nPredictionRandomSeed` / `RandomFloat/RandomInt`（no-spread）。
- `sv_maxusrcmdprocessticks` 值获取（Tickbase/DT 前置）。

## 6. 测试与安全

- **安全**：库初始化前解析 `/proc/self/cmdline`，无 `-insecure` → 拒绝初始化
  （记日志、不 hook）。启动脚本同样断言。测试中发现游戏未带 `-insecure`
  → 立即关闭游戏，禁止注入。
- 命令通道：`ZEPHYRUS_CSGO_CMD`（默认 `$XDG_RUNTIME_DIR/zephyrus-cmd`），
  沿用 ACR 的命令风格：`ui open|close|tab …`、`config …`、`dump`、`players`、
  `esp/rage/aa/fl …` 开关，便于无焦点自动化测试。
- 测试环境：本地 `-insecure` + `sv_lan 1` + bots（打枪/移动/回溯）；
  desync/resolver 需要双客户端或社区 HvH 服（用户复测）。
- 每阶段保持构建/测试绿色。

## 7. 风险

| 风险 | 缓解 |
|---|---|
| 游戏 core profile → 固定管线不可用 | 冒烟测试记录 profile；立刻移植官方 GL3 后端 |
| AnimState/动画层偏移未知 | Ghidra 逆向 + 运行时日志校验；resolver v1 可退化为 pose+eye 方案 |
| 注入 GL 状态被游戏污染（黑块/闪烁） | begin/end frame 全状态保存恢复（沿用 GL2 时期的黑框修复经验） |
| SDL2 相对鼠标/光标行为 | 参考 Osiris `ImGui_ImplSDL2` 路径 + `SDL_SetRelativeMouseMode` 手动控制 |
| legacy 客户端不再更新 | 以 BuildID/版本号强校验，pattern 失败时禁用对应功能而不是崩溃 |
| 卸载安全 | 沿用 ACR 生命周期（`active_detour_depth` 拒绝、析构兜底） |

## 8. 参考代码映射（Osiris csgo 分支 → zephyrus）

| Osiris | zephyrus-csgo |
|---|---|
| `Source/CSGO/*` | `zephyrus/csgo/sdk/*` |
| `Source/Interfaces/*` | `zephyrus/csgo/interfaces.*` |
| `Source/Netvars.cpp` | `zephyrus/csgo/netvars.*` |
| `Source/Memory.h/.cpp` | `zephyrus/csgo/memory.*` |
| `Source/BytePatterns/Linux*` | `zephyrus/csgo/patterns.*`（+ 自研 RE 偏移） |
| `Source/Hooks/{Client,ClientMode,ClientState,ModelRender,…}Hooks.h` | `zephyrus/csgo/hooks/*` |
| `Source/Hacks/{Aimbot,Triggerbot,Backtrack,Chams,Glow,Visuals,Misc,EnginePrediction}` | `zephyrus/features/csgo/*` |
| OsirisAndExtra `{AntiAim,Resolver,Fakelag,Tickbase,Ragebot,Animations,Knifebot,GrenadePrediction}` | 逻辑参考，按 §5 重写 |

## 9. 待确认

- 渲染：按 §3.3 先 GL2，失败即 GL3（若你坚持直接 GL3 请说）。
- 目录重构：文件移入 `features/acr`、`overlay/acr`，ACR 命名空间暂不动。
- v1 范围：Phase 2+3 一起作为“可用版本”，Phase 4 后续追加。
