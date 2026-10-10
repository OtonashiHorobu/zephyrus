# zephyrus TODO

> 最后更新：2026-10-09（HvH 功能轮）
> 状态标记：`[x]` 完成 · `[~]` 进行中/待验证 · `[ ]` 未开始
> 当前所有改动均未提交（zephyrus 与 acr-git 工作区 dirty）
>
> 多游戏分块计划与分模块 TODO：[csgo-port-plan.md](csgo-port-plan.md) ·
> [todo-shared.md](todo-shared.md) · [todo-acr.md](todo-acr.md) · [todo-csgo.md](todo-csgo.md)

## A. HvH 功能轮（2026-10-09）

### A.0 黑框根因（本次攻克）

- [x] **拖动/弹窗时的黑框、文字黑块、灰色高亮变黑**
  - 根因：`CompositeLayers()` 在 `apply_filters()` **之后**才 `glGet*` 保存渲染状态，
    滤镜管线把 FBO 切到 post target、viewport 改小、blend 关闭之后，"恢复"把这些
    中间状态当原状态写回；本帧之后所有 UI（文字/高亮/阴影）在错误状态下绘制。
  - 修复：状态保存移到滤镜链之前（`render_interface_gl2.cpp`）。
  - 附带修复：`PushLayer` 全纹理清理（避免复用 layer 的陈旧像素被 blur 降采样
    链抹进阴影）。
  - 状态：用户实测 "works perfectly"。

### A.1 新增模块

- [x] `features/movement`：speedhack（maxspeed 缩放）、auto bhop、空中即时换向
  （游戏 200Hz 内层 `moveplayer` hook，逐 substep 生效；视角不动）。
- [x] `features/exploits`：
  - 飞刀协议杀（`SV_WEAPCHANGE → SV_SHOOTC → SV_EXPLODE(flags=victim)`，延迟还原武器
    避免排队事件被 `gunselect` 检查丢弃；可选回收落刀循环）；
  - 手雷远程引爆（客户端坐标直接 `SV_EXPLODE`）；
  - 伪造发包位（force_scope / force_crouch / zero_velocity / force_onfloor，
    hook `sendposition` 临时改写后还原）；
  - teleport/blink（前向/准星/目标背后；`entinmap` 校验 + 落地 + 可选黑屏淡出）。
- [x] `addshotline` hook：远程出生保护同步 + 弹道线缓冲（ESP tracer）。
- [x] rage：爆头磁铁数学修正（弹线上距服务端脖颈最近点、≤1.95 夹取）、
  `skip_protected`、`head_magnet_all/strength`、hitchance 计入伪造包状态。
- [x] spawn protection 精确镜像（`gametype[].implied` + `lastspawn`），ESP 护盾 +
  倒计时文本。
- [x] **射击崩溃修复**：游戏 `vec` 是 float/int union（SysV INTEGER 分类），
  `addshotline(playerent*, vec, ...)` 按值参数走 rsi/rdx/rcx；`acr::vec3` 改为同
  union 布局后 ABI 对齐（原来纯 float → SSE，栈上 `to` 指针是垃圾 → SIGSEGV）。
- [x] **配置系统（TOML / toml++）**：启动默认空配置；`ZEPHYRUS_ACR_CONFIG=<file>`
  显式加载；所有改动自动快照到 `~/.config/zephyrus/latest.toml`（只写不自动读）；
  命名 profile 存取于 `profiles/`；剪贴板导入导出；OAE 风格两栏 Config 卡片
  （列表 + 名称 + Create/Save/Load/Delete/Open folder/Reset/Export/Import）。
- [x] 测试通道：Unix 文件命令 `/run/user/$UID/zephyrus-cmd`（`knife [cn]`、`nade [cn]`、
  `blink`、`goto <cn>`、`lookat <cn>`、`players`、`dump`、`key <input> <0|1>`、
  `movement/air/bhop/speed`、`config ...`、`ui open/close/tab/panel/reset`）。
- [x] 启动器 `/datassd/Clone/acr-bin/client-zephyrus.sh`（自动找库；强制
  `SDL_VIDEODRIVER=x11`，覆盖环境里的 wayland）。

### A.2 待验证 / 待办

- [~] **飞刀/手雷对敌伤害端到端验证**（代码路径与 "sent" 日志已验证，延迟还原修复后
  还需要一次带 bot 的定向测试：`players` 挑敌人 → `knife <cn>` → 看 hp 变化）。
- [ ] 服务端视野内的 fake-bit 效果实测（服务端认为开镜/下蹲/静止的 spread 统计）。
- [ ] `doc/AGENTS.md` 功能模块表补充 movement/exploits/config 与测试通道说明。
- [ ] 提交两个仓库的工作区改动。

## 0. 上轮修复（已完成）

- [x] **菜单缩放后布局塌陷**（子元素整体偏移、header 消失、侧栏被裁）
  - 根因：`#menu` 用 `left:50% + margin-left:-450dp` 居中，RmlUi 在元素尺寸变化后
    把百分比解析为相对元素自身尺寸，且 containing block 仍应用 stylesheet 负 margin，
    子元素被整体推移"尺寸差的一半"。
  - 修复：CSS 改为 `left:0; top:0`，位置完全由 `menu::materialize_position()` 设置；
    viewport 由 overlay 通过 `menu::set_viewport()` 直供。
- [x] **`std::clamp` 断言崩溃**（窗口/菜单太小时 `hi < lo` → `ACR error (6)` abort）
- [x] **窗口缩放后 overlay 外圈被裁切**：`menu::sync` 视口夹取 + 重新实体化
- [x] **resize 后内容不重排**（右侧控件被裁）：`refresh_content_width()`（按 Content 盒取宽、
  值变化才写属性）每帧收敛；slider 改为 `flex:1` 自适应
- [x] **整个 RmlUi 界面消失（打开一次菜单后连 ESP 标签也没了）**
  - 根因：游戏渲染到**自己的 FBO** 再自行呈现；我们 layer 弹栈后用 `bind_framebuffer(0)`
    恢复，导致之后所有 RmlUi 绘制进入默认帧缓冲（不可见）。ESP 线框在 BeginFrame 前绘制所以正常。
  - 修复：`begin_frame` 记录当时绑定的 framebuffer 作为主目标，`bind_layer_target(nullptr)`
    与 `ensure_layer_storage` 恢复都用它。
- [x] **box-shadow 没有模糊（硬边黑块/透明黑块溢出）**
  - 根因：未实现 `CompileFilter`/filter 合成 → RmlUi 跳过 blur pass。
  - 已实现完整 filter 管线（见 §1）。
- [x] **阴影不可见**（多轮排查后确认）
  - 关键结论：RmlUi 的 box-shadow 只在"元素外"绘制原始阴影环（clip mask），
    `offset=0 + spread=0` 时环为空 → 模糊无从展开 → 看不到阴影（这是 RmlUi 行为）。
  - 规避：CSS 使用非零 offset/spread（当前 `0dp 6dp 18dp 1dp` + `0dp 16dp 48dp 2dp`）。
- [x] **拖动时"时隐时现"高频闪烁**：`begin_frame` 不再逐帧绑 FBO；冗余 `SetProperty` 写入跳过
- [x] ESP 标签：
  - 居中偏移 bug（`(center_x-140)*inv_ratio` 把 140dp 当 px 扣，整体偏右 28px @1.2）
  - 名字按框高缩放（`font = clamp(box_h*0.20, 8, 22)px`，小数 dp = 无级缩放）
  - 武器/血量/距离移到 ESP 框**下方**（名字上方、meta 下方，两个 label root）
  - `text-shadow`（RmlUi 不支持）移除，改内联芯片样式
- [x] 复选框勾 / 下拉箭头视觉加大（12→14dp，▾→▼，13dp）
- [x] 列出 TODO 文档（本文件）
- [x] resize 后菜单不再被裁切（内容随面板宽度重排、控件完整可见）

## 1. RmlUi filter 管线（本轮新实现）

- [x] `CompileFilter`/`ReleaseFilter`：opacity、blur、drop-shadow、brightness、contrast、
  invert、grayscale、sepia、hue-rotate、saturate（后 8 个为色彩矩阵，shader 实现）
- [x] blur：降采样链（每级 1/2，最多 3 级）+ 可分离高斯（H/V 两 pass，权重归一化，
  `sigma' ≤ 3`、taps ≤ 9），post 目标按层级分配（尺寸精确，无角落偏移）
- [x] 越界采样丢弃（不 clamp 累加，避免区域边缘出现暗框）
- [x] filter pass 期间关闭 scissor/stencil（post 目标在自己空间渲染），结束恢复
- [x] 合成时按 scissor 区域裁剪（layer 区域内有效，防止复用层的陈旧像素外泄）
- [x] filter 应用失败时跳过该帧阴影（不回退画原始硬边阴影）

## 2. 未解决 / 高优先级

- [ ] **拖动 overlay（菜单）时出现"黑色一圈"**
  - 现状：仍可复现（用户报告），本轮未定位成功
  - 已知线索：
    - 打开 popup 时也会短暂出现，随后消失
    - 与"时隐时现闪烁"不同，后者已修复
    - `ElementBackgroundBorder::Render` 在存在 box-shadow 时只绘制阴影纹理
      （背景/边框都来自该纹理），纹理每次重新生成（拖动导致元素小数像素位置变化、
      `Math::ExpandToPixelGrid` 使 region 变化 → 新 cache 条目）
    - 怀疑方向：瞬态帧里 blur 未生效/纹理未就绪时把原始阴影环画出来
      （原始环=硬边黑圈）；或 region 边缘的陈旧像素参与采样
    - 待试：捕获单帧（需要高速截图手段，`import` 280ms/张太慢，x11grab 对 GL 窗口全黑）；
      可在 `CompositeLayers`/`apply_filters` 加计数器+条件截图，或把 region 尺寸/元素位置
      记录到日志逐帧对比
- [ ] 菜单深色背景下阴影可见度调优（当前双阴影 + spread，用户已确认可见）

## 3. 菜单 / UI

- [x] 间距/字号、Analyst 动效、DPI 响应式缩放、滚轮滚动、keybind、拖动、resize、clamp
- [ ] **配置保存/加载（Phase B）**：INI 风格、防抖自动保存
  - 路径：`~/.config/zephyrus/config.ini`
  - 内容：全部选项、菜单窗口位置/尺寸、HUD 窗口位置
  - 菜单内 Save / Reset 按钮
- [ ] 其他分辨率（1080p / 1440p / 21:9）下响应式布局复查

## 4. ESP

- [x] 骨骼捕获修复、名牌抑制、标签缩放/居中/位置、无级缩放
- [ ] **hitbox 实际位置可视化**：头=球 r=0.4（客户端上报 head）；躯干=上 35%、腿=下 65%
  （r=1.1，`TORSOPART=0.35`）；随射击者 pitch 倾斜；下蹲整体下移
  `PLAYERHEIGHT*(1-crouchfactor)`
- [ ] 布局/颜色继续打磨（低优先级）

## 5. HUD（Phase C）

- [ ] 多窗口 HUD：Watermark / Status / Rage / Target / Performance / Keybinds
  （独立可拖动、开关、位置持久化；参考 OAE watermark 按窗口名存位置）

## 6. 可视化（Phase C）

- [ ] **射击轨迹可视化**：hook `addshotline(playerent*, vec, vec, int)`（服务端广播真实弹道），
  持续一段时间后淡出
- [ ] **spread 可视化**：按 `θ = spread*perk*ads/2000` rad 绘制
- [ ] **no-scope overlay**：未开镜时镜罩叠加

## 7. RageBot（Phase D，重写引擎）

- [ ] 射线采样器：真实 hitbox / multipoint（OAE 点集：头 center+top/right/left；
  其余 center+right/left，偏移=半径×min(scale,95)%）/ 多目标 / 穿人（+10m 衰减、±5° 随机、
  排除已命中者）/ 跳弹（掠射角>15°、反射一次、±2° 服务端随机、+15m 衰减）。
  **ACR 无墙体穿透**，唯一过墙途径是跳弹。
- [ ] **TTK 期望决策器**（伤害/距离衰减/ADS 开镜时间 300ms(perk 200)/停步/ping/换弹/护甲），
  在"直接打 / 开镜 / 停步"中取最快可行者
- [ ] **ADS 状态机 + 迟滞**（进入交战才开镜；脱离延迟 400–600ms；最短保持时间）
- [ ] **no scope**（不满足阈值才开镜）、**智能 auto stop**（反向键+恰好够用速度）
- [ ] 目标排序（TTK+威胁权重+滞回）、Lethal 模式、动态阈值、射击日志/脱靶原因
- [ ] 默认**不开** visible only（用伤害路径判定替代）

## 8. 游戏侧（acr-git，genshadowvolume 缓存崩溃）

- [ ] **决定是否重启根因修复任务**（上次 subagent 被取消）。现状：
  - `modelcache.h`（`ACR_MCACHE_HUNT` 插桩+abort）、`vertmodel.h`、`clientgame.cpp` 277 行未提交
  - hunt 版二进制已部署（`.bak-hunt`）、本地修复版已部署（`.bak-20260523`）
- [ ] 更新 `docs/modelcache-shadow-cache-bug.md`（根因/修复状态），移除插桩后提交

## 9. 工程 / 杂项

- [ ] 提交当前工作区（两个仓库均 dirty）
- [ ] 清理仓库根目录 `zephyrus*.log`
- [ ] 回归测试脚本整理（`scripts/acr-start.sh` / `acr-do.sh` / `acr-menu-click.sh`；
  注意 `acr-menu-click.sh` 的菜单原点假设与实际不一致，本轮踩过坑）
