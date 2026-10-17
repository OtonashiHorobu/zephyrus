# zephyrus TODO — shared (CS:GO shared layer)

> This branch (`csgo`) only maintains CS:GO and its shared layer; ACR has been
> split off into the `acr` branch and frozen, see [BRANCHES.md](../BRANCHES.md).
> CS:GO-specific items are in [todo-csgo.md](todo-csgo.md), the overall plan is
> in [csgo-port-plan.md](csgo-port-plan.md).
> Status: `[x]` done · `[~]` in progress · `[ ]` not started

## S1. Render backend

- [x] `rmlui_manager` is decoupled from the concrete SDL version through
      `Backend::Initialize` / `Backend::ContextAlive`; `init_impl` /
      `shutdown_impl` only call the Backend API.
- [x] Use the official RmlUi GL3 render interface; save/restore the host's GL
      state every frame.
- [~] Full GL state audit: program, VAO, depth test/mask, cull, active texture,
      matrices, blend/scissor. The common states are covered, but they still
      need to be checked item by item against the game's render path.
- [ ] `backend_mode` semantics cleanup: only `injected` is used right now, the
      `standalone` enum is still kept.

## S2. SDL2

- [x] `include/zephyrus/sdl2/sdl2.hpp` + `src/zephyrus/sdl2/sdl2.cpp`: function
      table (`dlopen("libSDL2-2.0.so.0", RTLD_NOLOAD | RTLD_LAZY)`).
- [x] `FF 25 rel32` pointer-slot replacement for `SDL_PollEvent` /
      `SDL_GL_SwapWindow`, with the inline-hook fallback kept.
- [x] `system_interface_sdl2`: cursor, clipboard, time, SDL2 key → Rml key
      mapping.
- [x] `backend_sdl2_gl3.cpp`: injected-mode initialization + `ProcessEvents`
      (SDL2 event routing).

## S3. Hook basics

- [x] `hook::pointer_slot`: read / replace / restore `FF 25 rel32` pointer
      slots; reports an error and falls back to an inline hook when the
      signature does not match.
- [x] Unit test: `test/zephyrus/hook/test_pointer_slot.cpp`.
- [ ] `hook::vmt` (optional): vtable copy + swap, used only when an inline hook
      is not suitable.

## S4. Generic menu mechanics

- [ ] Extract `overlay/menu_base` from `overlay/csgo/menu.cpp`: widget binding
      (checkbox/slider/combo/keybind), tab/sidebar navigation, drag/resize,
      scrolling, popup positioning, config list UI, `ProcessEvent`.
- [ ] Move the keybind name table out of `overlay/csgo/menu.cpp` into the shared
      layer (SDL2 scancode).

## S5. Generic config layer

- [x] `config::document` stays generic; CS:GO uses
      `~/.config/zephyrus/csgo/{latest.toml,profiles}`, autosaved after changes
      are debounced.
- [ ] Extract the command-channel poll logic into `overlay::command_channel`
      (path, one command per line), with CS:GO registering its own command
      handlers.

## S6. Build / CI

- [x] CMake target split: `zephyrus-core` / `zephyrus-csgo-core` / `zephyrus-csgo`.
- [x] Keep `-Wall -Wextra -Werror` and `-fvisibility=hidden` / version-script /
      `--exclude-libs,ALL`.
- [x] `cmake/zephyrus-csgo.map` (exports only `zephyrus_csgo_*`).
- [x] Test targets link only `Zephyrus::Core`.
- [ ] CI (build + ctest).
