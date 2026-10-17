# zephyrus TODO — shared（CS:GO 公共层）

> 本分支（`csgo`）只维护 CS:GO 及其公共层；ACR 已拆到 `acr` 分支并冻结，
> 见 [BRANCHES.md](../BRANCHES.md)。CS:GO 专属条目见 [todo-csgo.md](todo-csgo.md)，
> 总体计划见 [csgo-port-plan.md](csgo-port-plan.md)。
> 状态：`[x]` 完成 · `[~]` 进行中 · `[ ]` 未开始

## S1. 渲染后端

- [x] `rmlui_manager` 通过 `Backend::Initialize` / `Backend::ContextAlive` 与具体
      SDL 版本解耦；`init_impl` / `shutdown_impl` 只调用 Backend API。
- [x] 使用官方 RmlUi GL3 渲染接口；每帧保存/恢复宿主的 GL 状态。
- [~] 全 GL 状态审计：program、VAO、depth test/mask、cull、active texture、
      矩阵、blend/scissor。常用状态已覆盖，仍需逐项对照游戏渲染路径。
- [ ] `backend_mode` 语义整理：当前只会用到 `injected`，`standalone` 枚举仍保留。

## S2. SDL2

- [x] `include/zephyrus/sdl2/sdl2.hpp` + `src/zephyrus/sdl2/sdl2.cpp`：函数表
      （`dlopen("libSDL2-2.0.so.0", RTLD_NOLOAD | RTLD_LAZY)`）。
- [x] `SDL_PollEvent` / `SDL_GL_SwapWindow` 的 `FF 25 rel32` 指针槽替换，
      并保留 inline hook 回退。
- [x] `system_interface_sdl2`：光标、剪贴板、时间、SDL2 key → Rml key 映射。
- [x] `backend_sdl2_gl3.cpp`：注入模式初始化 + `ProcessEvents`（SDL2 事件路由）。

## S3. Hook 基础

- [x] `hook::pointer_slot`：读取 / 替换 / 还原 `FF 25 rel32` 指针槽；不满足
      特征时报错并回退 inline hook。
- [x] 单测：`test/zephyrus/hook/test_pointer_slot.cpp`。
- [ ] `hook::vmt`（可选）：VMT 拷贝 + 交换，仅在 inline hook 不合适时使用。

## S4. 菜单通用机制

- [ ] 从 `overlay/csgo/menu.cpp` 抽出 `overlay/menu_base`：控件绑定
      （checkbox/slider/combo/keybind）、tab/sidebar 导航、拖动/缩放、滚动、
      popup 定位、config 列表 UI、`ProcessEvent`。
- [ ] keybind 名称表从 `overlay/csgo/menu.cpp` 移到公共层（SDL2 scancode）。

## S5. 配置通用层

- [x] `config::document` 保持通用；CS:GO 使用
      `~/.config/zephyrus/csgo/{latest.toml,profiles}`，改动防抖后自动保存。
- [ ] 命令通道 poll 逻辑抽成 `overlay::command_channel`（路径、一行一命令），
      由 CS:GO 注册自己的命令处理器。

## S6. 构建 / CI

- [x] CMake 目标拆分：`zephyrus-core` / `zephyrus-csgo-core` / `zephyrus-csgo`。
- [x] 保持 `-Wall -Wextra -Werror` 与 `-fvisibility=hidden` /
      version-script / `--exclude-libs,ALL`。
- [x] `cmake/zephyrus-csgo.map`（只导出 `zephyrus_csgo_*`）。
- [x] 测试目标只链接 `Zephyrus::Core`。
- [ ] CI（构建 + ctest）。
