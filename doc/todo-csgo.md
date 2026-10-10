# zephyrus TODO — csgo（新游戏）

> 维护规则见 [todo-shared.md](todo-shared.md)，总计划见 [csgo-port-plan.md](csgo-port-plan.md)。
> 功能参考：OsirisAndExtra（重写）；SDK/patterns 参考 Osiris csgo 分支（MIT，可移植）。

## F0. 安全与注入

- [x] `-insecure` 强校验：解析 `/proc/self/cmdline`；缺失 → 拒绝初始化（stdout + log），
      绝不安装任何 hook。（已用 `ctypes.CDLL` 无 `-insecure` 加载验证：库正常载入、
      打印拒绝信息、不初始化）
- [x] 版本校验：读 `csgo/steam.inf` 的 PatchVersion/ClientVersion，不匹配时告警。
- [x] `scripts/csgo-start.sh`：`LD_PRELOAD` + `-insecure` 断言 + Steam scout runtime 包装
      （`libcurl-gnutls.so.4`）+ 强制 X11 + `WINDOWED=1`（XWayland 全屏黑屏的规避）。
      Steam 启动项写法见脚本头。直接启动已实测可进游戏。
- [ ] 游戏内注入实测（脚本已可用；待叠加层实现后做）。

## F1. SDK 基础（Phase 2）

- [x] 模块加载：`hook::find_module`（basename 匹配，dl_iterate_phdr）+ 探针日志。
- [x] 接口获取：`CreateInterface`（RTLD_NOLOAD + dlsym；`s_pInterfaceRegs` 回退后续做）。
- [x] `patterns`：探针已对 4 个 Osiris Linux pattern 做运行期校验（localPlayer 等）。
- [x] `netvars`：recv table 遍历（`src/zephyrus/csgo/netvars.cpp`）。
- [x] `patterns` 全量移植（Linux client 表 + engine 表，`patterns.cpp`）。
- [x] `memory`/`game`：localPlayer/globalVars/clientMode/input/engine/engineTrace/
      cvar/glowObjectManager（pattern）/weaponSystem/playerResource/plantedC4s/
      gameRules/lineGoesThroughSmoke/isOtherEnemy/setAbsOrigin/…。
- [x] `entity`/`usercmd`/`view_setup`/trace/bone（`virtual_call` + netvar 访问器）。
- [x] 启动 dump：模块基址、版本、接口、pattern 命中清单（`zephyrus_csgo_probe`）。

## F2. 叠加层（Phase 2）

- [x] `SDL_PollEvent` 指针槽 hook（事件先给 RmlUi；菜单可见时吞事件；inline 回退已就位）。
- [x] `SDL_GL_SwapWindow` 指针槽 hook（画帧 + 原 swap；inline 回退已就位）。
- [x] 光标/相对鼠标：菜单可见时 `SDL_SetRelativeMouseMode(SDL_FALSE)` +
      show cursor；关闭时还原（每帧重设以防游戏抢回）。
- [x] 延迟安装：SDL2 由引擎在 constructor 之后加载，后台线程轮询 + 指针槽交换。
- [x] 菜单骨架 `asset/ui/csgo/menu.rml`（冒烟测试文档：渲染/输入/光标链路）。
- [x] 游戏内实测：重启游戏 → INSERT 开合 → 日志 `csgo overlay hooks installed` /
      `csgo overlay ready`；菜单渲染与光标恢复已验证。
- [x] 游戏内鼠标修复：菜单可见时拦截 `SDL_SetRelativeMouseMode(TRUE)` /
      `SDL_WarpMouseInWindow` / `SDL_ShowCursor(0)`，并按帧轮询 `SDL_GetMouseState`
      合成 down/up（游戏用自身事件路径消费按键事件，事件队列不可靠）。
- [ ] `-insecure` 冒烟：日志 GL_VERSION / GL_CONTEXT_PROFILE_MASK；
      固定管线可用性验证（不可用 → 触发 GL3 移植，见 shared TODO）。
- [x] 命令通道 + 测试脚本（`ui open|close|toggle|tab|panel`/`dump`/`players`/
      `rage`/`aa`/`fl`/`bt`/`esp`/`glow`/`autowall`/`fov`/`bhop`/`trace`/
      `config save|load|delete|list|reset|export|import|folder [name]`；
      `scripts/csgo-cmd.sh`）。已导出 `zephyrus_csgo_toggle_overlay` /
      `zephyrus_csgo_probe` 供 gdb 自动化。

## F3. HvH v1 功能（Phase 3）

- [x] Hooks：ClientMode `createMove`(25, 支持 sendPacket 覆写) / `overrideView`(19) /
      `getViewModelFov`(36)；Client `frameStageNotify`(37，backtrack 记录)。
- [x] `doPostScreenEffects`(45)：`skip_post_screen_effects`（去瞄准镜遮罩）。
- [x] 其余 hooks：`shouldDrawFog`(18)/`dispatchUserMessage`(38) 已接；
      `UpdateClientSideAnimation`（Linux vtable 索引 **286**，Ghidra 验证
      主 vtable 0x22C7B00）hook 已启用，用于本地 real matrix 捕获。
- [~] `EnginePrediction`（velocity/flags，参考 O&E）：完整实现
      （`features/csgo/prediction.*`，IPrediction 20/21、GameMovement 1）；
      受 `restoreEntityToPredictedFrame` 未逆向限制自动禁用（re-f3 B1）。
- [x] `Animations` 框架：per-player 记录（matrix、pose、layers、simtime、
      velocity），48 条历史供 backtrack/resolver；`UpdateClientSideAnimation`
      hook + real matrix 管线已上线（fake matrix / 前向 fake pose 需 A5/A6）。
- [x] Ragebot 增强：Monte-Carlo hitbox 命中率（`hitchance.*`，基于被瞄准的
      记录骨骼；backtrack 记录同样适用）、autowall 伤害模型（`autowall.*`，
      引擎 trace 重写 TraceToExit；`IPhysicsSurfaceProps::GetSurfaceData`
      已用 core 验证为 **槽位 6**）、per-category 配置（7 类默认）。
      **no-spread 已整体移除**：CS:GO 的散布由服务端随机数决定，客户端无法
      消除（`MD5_PseudoRandom` 实测取 `digest+6`，仅存档于 re-abi 文档）。
- [~] AntiAim 增强：pitch（down/up/zero/fake）/yaw（backward/right/left/
      jitter/spin/static/at-targets）、choke 翻转 desync（static/jitter/
      sway/auto-direction）、LBY 破防、manual 按键（overlay 原始键事件）、
      auto-direction 扫描。真实 animstate 写入（fake pose）需 A5/A6。
- [x] Triggerbot：**已按用户要求丢弃**（HvH-only）。
- [x] Backtrack v1 → v2：记录迁移到 Animations（bones/simtime），
      select/rewind + per-record 解析与烟雾检查（不再每记录 trace）。
- [x] AntiAim v1：pitch（down/up/zero）、yaw（backward/jitter/spin）+ desync 抖动。
- [x] Fakelag v1：static/adaptive/random + on-shot；新增 `preview()` 供 AA
      判断本 tick 是否发送（deterministic per-tick limit）。
- [x] Resolver v2：移动速度 / AnimState footYaw（Linux 0x84，运行期
      合理性校验）/ `m_flLowerBodyYawTarget` 检测栈 + 按射击计数的
      brute-force；per-record 位置旋转。
- [x] ESP v1（RmlUi 叠加层，独立于菜单开合）：盒子/血量/名字/武器/距离/snapline +
      可见/敌我着色；骨架待做。
- [x] Glow（`GlowObjectManager`，敌人描边，血量/彩虹配色）。
- [x] Visuals：no-flash（`m_flFlashMaxAlpha` 置零）、remove scope（跳过
      `DoPostScreenEffects`）、no-fog（`ShouldDrawFog` hook）；no-smoke/
      十字线等待做。
- [x] Movement：bhop、auto-strafe、slow-walk（按键，overlay key capture）、
      movement fix（AA 改角后保持世界方向）。
- [x] 菜单/配置：Rage/AA/Backtrack/Fakelag/Movement/ESP/Glow/Resolver 全部
      绑定到菜单并持久化到 `csgo/latest.toml`（`[features]`）；新增
      Weapons 页（按武器类别）、manual 按键捕获、desync/LBY 控件。
      （`ZEPHYRUS_CSGO_*` 环境变量模式被 TOML 取代。）

### 第二轮修复（2026-10-10，review + core 分析）

- [x] **TraceRay 调用约定崩溃（SIGSEGV）**：`virtual_call` 按值传 `Ray_t`/
  `trace_t`/`Vector`，SysV 下 MEMORY 聚合落到栈上、寄存器留垃圾；引擎按
  指针解引用（core: `rep stos %rax,(%rdi)`, rdi=1）。修复：`entity.hpp` 的
  `engine_trace_ray()` / `engine_point_contents()` typed wrapper，全指针参数；
  `ITraceFilter` 移除误加的虚析构（槽 0=ShouldHitEntity, 1=GetTraceType）；
  `trace` 尺寸 static_assert 0x68（引擎清零 104 字节）。
- [x] **`IPhysicsSurfaceProps::GetSurfaceData` 崩溃**：Osiris 的槽位 5 实际是
  `GetPhysicsProperties`（4 个输出指针）→ 修正为 **槽 6**（runtime vtable +
  反汇编验证）；`surfacedata_t` 字段偏移用 core 实测。
- [x] **RageBot / AntiAim / FakeLag 顺序**：rage 先跑（FOV 用真实视角；autofire
  位对 fakelag on_shot / AA disable_on_shot 可见），fakelag 决定 send，AA 最后；
  AA 在 ragebot 已瞄准的 tick 上不覆盖角度（O&E 语义，`original_angles`）。
- [x] **no-spread 移除**（服务端拥有随机数；UI/命令/模块全部删除），
  `MD5_PseudoRandom`/`UpdateSpread` 逆向结果存档于 `doc/csgo-re-abi.md`。
- [x] **ESP 修复**：玩家名/武器名 RML 转义（武器名剥掉 `#SFUI_WPNHUD_` 前缀）；
  关闭/死亡时清空图层；渲染行不再引用旧 document；改用 `GetAbsOrigin`；
  **相机背后目标投影守卫**（修复"抽播/飘天空/入地"的盒子）。
- [x] **菜单重做**：Config 页真实档案管理（save/load/delete/reset/export/
  import/folder，`config_*.` 命令同步）、滚动、拖拽视口夹取、右下角 resize、
  combo/键位弹窗与 Off/Always/Hold/Toggle/Unset 模式、键位持久化（SDL2
  scancode 原始存储）、Escape 取消捕获恢复原值、滑块 dragging 去过渡、
  `config_.tick()` 防抖（不再每次改动同步写盘）、FPS 实时显示。
- [x] **内建 UI 与实际能力的对齐**：删除 Chams/Tickbase/Indicators/Skeleton/
  烟雾/去闪（保留）/十字线/Max distance/Multipoint/No spread 等未实现控件。
- [x] **配置目录拆分**：`~/.config/zephyrus/acr/{latest.toml,profiles}` 与
  `~/.config/zephyrus/csgo/{latest.toml,profiles}`；ACR 旧顶层配置一次性迁移，
  CS:GO 不再列出 ACR 档案。版本串按产品区分（`Zephyrus CS:GO ...`）。
- [x] **Bug 修复**：Team check 反义、backtrack `time_limit` 生效、动画记录
  仅在 `setup_bones` 成功时入队、hook 安装失败可重试（不再误置已安装）、
  重新注入复位 `overlay_stop`、`server_time` 同 tick 复用缓存、AA static
  yaw_add 双计、LBY 单 tick flick、resolver `on_hit/on_miss`（rage 用血量差
  确认命中）+ auto 模式 miss 后 brute-force。
- [x] **Ghidra 命名/注释落项目**：TraceRay/GetPointContents/GetPlayerInfo/
  CHLClient 37/38/MD5/GetSurfaceData 等函数与 vtable 标签已写入 Ghidra 项目。
- [ ] 游戏自身 `DoPostScreenEffects` 路径仍偶发 SIGSEGV（core：参数为
  截断指针 `0xef11fdc8`，位于游戏 glow 渲染循环，非 zephyrus 代码）；
  需用 **无注入 vanilla 对照**（Steam 运行时未启动时无法测试）或多轮观察确认。

### F3 实机验证（2026-10-10）

- [x] **修复游戏初始化从未成功**：`game::create()` 使用了错误的接口名
      `VEngineTraceClient004`（正确为 `EngineTraceClient004`），此前所有
      feature hook 实际从未安装。实机日志确认 `game resolved` +
      `patterns: resolved (0 missing)` + `netvars: 1797 properties` +
      client mode hooks（含 shouldDrawFog/dispatchUserMessage）+
      `UpdateClientSideAnimation hook installed (index 286)`。
- [x] 主菜单 → 地图加载：修复 overlay 安装线程超时后无人再解析 game 的问题
      （`on_ready` 每 15 帧重试 `initialize_game`），以及菜单在功能对象
      出现前初始化导致控件未绑定的问题（`refresh_menu_bindings`）。
- [x] 菜单 sidebar 背景断裂修复（显式 height/align-self）。
- [x] 命令通道实测（`dump`/`players`/`rage on`/…），`players` 输出
      eye_yaw/layers/animstate 指针诊断。
- [x] Glow 改为"实体列表取敌 → 仅匹配 manager 条目指针"的写入方式，
      避免对 manager 内的可疑指针做虚调用。
- [x] ESP 崩溃定位（core dump 回溯：`esp.cpp` collideable `virtual_call<1>`
      在无 vtable 的对象上）：改用已验证的 netvar 包围盒
      （`CBaseEntity->m_vecMins` 0x388 / `m_vecMaxs` 0x394），collideable
      仅作带 vtable 校验的回退。
- [~] AnimState 字段仍需实机确认：`m_PlayerAnimState`（0xA248）对活体返回
      64 位指针，但 +0x28/+0x84 实测读到 0（刚 spawn 的 bot 可能尚未更新，
      也可能偏移不对）；`anim_state_view` 已加指针/合理性校验，resolver
      安全回退到 LBY/brute-force。
- [ ] ESP/Glow 完整实机回归（活体 + 稳定回合）：本机测试环境在回合切换时
      偶发 panorama_gl 崩溃（**修改前就存在**，core 回溯位于游戏自身
      panorama_gl_client.so 渲染线程），需要在用户环境复测区分。

> F3 剩余受限项：EnginePrediction（`restoreEntityToPredictedFrame`，re-f3 B1）、
> fake matrix / 前向 fake pose（`SetAbsAngle`/`m_angAbsRotation`，A5/A6）、
> 精确 hitbox 胶囊（D2/D3）、AnimState yaw 字段确认（A3 运行时复测）。
> Ghidra 第一/二轮结果见 `doc/csgo-re-f3.md`。

## F4. OsirisAndExtra 其余功能（Phase 4）

- [ ] Chams（材质系统 + KeyValues；Osiris Linux 路径可用）。
- [ ] Glow（`GlowObjectManager`，Linux 参数路径）。
- [ ] Visuals：no-scope zoom、remove scope overlay、no smoke/flash、天空盒、
      world color、bullet tracers、sniper/recoil/penetration crosshair。
- [ ] Misc：spectator list、bomb timer、hitmarker、watermark、nade trajectory、
      fake duck、slow walk、quick stop、auto peek。
- [ ] Knifebot、GrenadePrediction（O&E 参考）。
- [ ] Tickbase/DT/fake latency（最后；依赖 `SendDatagram`/`sv_maxusrcmdprocessticks` 逆向）。

## F5. Resolver / AA 迭代（Phase 5，持续）

- [ ] 命中反馈闭环：`weapon_fire`/`bullet_impact`/`player_hurt`/`player_death`。
- [ ] brute-force 桶持久化（按 userid）+ miss 分类（resolver/spread/无伤害）。
- [ ] resolver 调试 UI：模式/侧向/命中率/最近 miss；命令 `resolver dump`。
- [ ] AA 反解析随机化：seed、周期随机、按状态分档；实测双客户端校准。

## RE. 逆向清单（Phase 0/3/4）

> **ABI / vtable / 结构体偏移的实测汇总见 [csgo-re-abi.md](csgo-re-abi.md)**；
> Ghidra 项目内的函数命名与注释同步更新（TraceRay、GetPointContents、
> GetPlayerInfo、CHLClient 37/38、MD5_PseudoRandom、GetSurfaceData 等）。

- [x] Ghidra 导入 `client_client.so` + `engine_client.so`（项目 `csgo`）；
  `vphysics_client.so` 亦已导入（physics surface props 验证）。
- [x] Osiris Linux pattern 对本地二进制校验脚本化。
- [ ] `CCSPlayerAnimState` 字段偏移（eyeYaw/footYaw/lastUpdateTime/duckAdditional/
      onGround/walkToRunTransition）。
- [ ] `m_flPoseParameter` 读写 / `GetPoseParameter`。
- [ ] 动画层 `m_AnimOverlay`（weight/cycle/playbackrate）。
- [x] `UTIL_TraceToExit`（autowall）→ 引擎 trace 重写并使用
  `IPhysicsSurfaceProps`（GetSurfaceData=槽 6，surfacedata 偏移 core 实测）。
- [ ] `CNetChan`/`CClientState`（chokedPackets、SendDatagram）；
  `CClientState` 全局与 getter 已命名（`CClientState_GetGlobal/GetByIndex`）。
- [~] 随机种子/`RandomFloat`：MD5_PseudoRandom(digest+6) 与 UpdateSpread
  已定位并存档；功能因服务端权威已移除。
- [ ] `sv_maxusrcmdprocessticks` 值（Tickbase/DT 前置）。
- [x] 每个偏移记录：Ghidra 地址 + pattern + 校验日志（见 re-abi 文档）。

## T. 测试

- [ ] 本地 `-insecure` + `sv_lan 1` + bots：瞄准/回溯/移动。
- [ ] 双客户端/LAN：desync、resolver、fakelag 行为验证。
- [ ] 社区 HvH 服（用户复测）：实战 log + 调参。
