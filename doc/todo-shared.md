# zephyrus TODO — shared（跨游戏公共层）

> 维护规则：一个条目只属于一个 TODO 文件；ACR 专属见 [todo-acr.md](todo-acr.md)，
> CS:GO 专属见 [todo-csgo.md](todo-csgo.md)，总计划见 [csgo-port-plan.md](csgo-port-plan.md)。
> 状态：`[x]` 完成 · `[~]` 进行中 · `[ ]` 未开始

## S1. 渲染后端解耦

- [x] `rmlui_manager` 不再依赖 `zephyrus::sdl`（SDL1.2）：
  - [x] 新增 `Backend::Initialize(backend_mode, name, w, h)` / `Backend::ContextAlive()`；
        `was_init(SDL_INIT_VIDEO)` 判断移入各后端实现。
  - [x] `init_impl` / `shutdown_impl` 只调用 Backend API。
- [ ] `render_interface_gl2` 的 SDL_image 文件纹理路径抽成 `texture_loader`：
  - [ ] ACR：SDL1.2 + SDL_image（保持现状）。
  - [ ] CS:GO：SDL2（菜单无图片资源，字体走 RmlUi freetype）。
- [ ] 注入路径 `begin_frame`/`end_frame` 全 GL 状态保存/恢复：
      program、VAO、depth test/mask、cull、active texture、矩阵、blend/scissor（已有部分）。
- [ ] `backend_mode` 扩展/语义整理，明确 standalone/injected + SDL 版本由链接的后端决定。

## S2. SDL2 支持（给 CS:GO）

- [ ] `include/zephyrus/sdl2/sdl2.hpp` + `src/.../sdl2.cpp`：
      函数表（PollEvent/PushEvent/PeepEvents/GetKeyboardState/GetModState/
      ShowCursor/CreateColorCursor/SetCursor/FreeCursor/WarpMouseInWindow/
      SetRelativeMouseMode/GetMouseState/GetTicks/Delay/GetWindowSize/
      GL_GetProcAddress/GL_GetCurrentContext/…）。
- [ ] 解析策略：`dlopen("libSDL2-2.0.so.0", RTLD_NOLOAD|RTLD_LAZY)`（游戏已加载），
      所有函数指针替换走指针槽（`FF 25 rel32`）时注意保存原指针。
- [ ] `system_interface_sdl2`（RmlUi SystemInterface）：光标、剪贴板、时间、
      SDL2 keycode → Rml key 映射（参考 RmlUi 官方 SDL 平台层 + Osiris InputUtil）。
- [ ] `backend_sdl2_gl2.cpp`：注入模式初始化 + `ProcessEvents`（SDL2 事件路由）。

## S3. Hook 基础增强

- [ ] `hook::pointer_slot`：给定 `FF 25 rel32` thunk 地址得到可写指针槽，
      支持读取原值 / 替换 / 还原；不满足特征时报错（回退 inline hook）。
- [ ] `hook::vmt`（可选）：VMT 拷贝 + 交换（参考 Osiris `VmtSwapper`），
      仅在 inline hook 不合适时使用。
- [ ] 单测：pointer_slot 对本地构造 thunk 的替换/还原。

## S4. 菜单通用机制抽取

- [ ] 从现有 `overlay/menu.cpp`（1518 行）抽出 `overlay/menu_base`：
      控件绑定（checkbox/slider/combo/keybind）、tab/sidebar 导航、拖动/缩放、
      滚动、popup 定位、config 列表 UI、`ProcessEvent`。
- [ ] ACR `menu` 改为继承/调用 menu_base，行为不变（用 standalone 验证）。
- [ ] CS:GO `menu` 复用 menu_base，只写功能绑定。
- [ ] keybind 键名映射按游戏提供（SDL1.2 vs SDL2 scancode）。

## S5. 配置通用层

- [ ] `config::document` 保持通用；profile 管理逻辑（list/save/load/delete/clipboard）
      抽为共用 helper，ACR/CS:GO 共享同一份 `~/.config/zephyrus/` 目录结构，
      文件名按游戏区分（`latest.toml` vs `csgo/latest.toml`）。
- [ ] 命令通道 poll 逻辑抽成 `overlay::command_channel`（路径、一行一命令），
      ACR/CS:GO 各自注册命令处理器。

## S6. 构建/CI

- [ ] CMake 拆分（见计划 §3.2）后保持 `-Wall -Wextra -Werror` 与
      `-fvisibility=hidden` / version-script / `--exclude-libs,ALL`。
- [ ] `cmake/zephyrus-csgo.map`（导出 `zephyrus_csgo_*`）。
- [ ] 测试目标保持只链接 `Zephyrus::Core`。
