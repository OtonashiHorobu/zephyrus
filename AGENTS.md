# AGENTS.md

## Build

Dependencies are managed exclusively by **Conan 2** (`conanfile.py`). The three
recipes that are not on ConanCenter live in `conan/recipes/` and must be
exported into the local cache once per machine:

```bash
./scripts/conan-export.sh                     # export local recipes (once)
conan install . --build=missing -pr gcc-debug # Debug, GCC, C++26
cmake --preset conan-debug
cmake --build --preset conan-debug
```

For clang use the `debug` profile; for release use `gcc-release`. Conan
generates `CMakeUserPresets.json`, so always configure through a
`conan-*` preset.

## Test

```bash
ctest --preset conan-debug                 # all tests
ctest --preset conan-debug -R <test-name>  # single test (use hyphens)
```

Test executables are auto-generated from `test/**/*.cpp`. Filename underscores become hyphens in the test name (e.g. `test_cmrc_file_interface.cpp` → `test-cmrc-file-interface`).

## Format & Lint

- `clang-format -i` — uses `.clang-format` (LLVM base, 4-space indent)
- `-Wall -Wextra -Werror` on library targets — treat warnings as errors

## Code Style

- **C++26** standard required. clangd config in `.clangd` strips module flags.
- `snake_case` for namespaces, classes, functions, variables
- `PascalCase` only for RmlUi virtual method overrides (`Open`, `Close`, `Read`, `Seek`, `Tell`)
- **Brace initialization** preferred: `auto x{value}`, not `auto x = value`
- **SPDX license header** required on every source file (MIT)
- `#pragma once` for header guards
- `noexcept` on most method implementations
- Macros prefixed with `ZEPHYRUS_`

## Architecture

- **CRTP managers**: `abstract::manager<Derived>` base class — new subsystems inherit it and implement `init_impl()` returning `std::expected<void, std::string>`
- **Error handling**: `std::expected` (C++23), not exceptions
- **Dependency injection**: Boost.DI (`boost/di.hpp`), wired in `global_zephyrus()`
- **Asset embedding**: CMRC — add resources in `asset/CMakeLists.txt`, declare with `CMRC_DECLARE` in headers
- **Logging**: spdlog with custom `ZEPHYRUS_*` macros (`ZEPHYRUS_TRACE`, `ZEPHYRUS_INFO`, etc.)

## Build Targets

| Target | Type | Description |
|---|---|---|
| `zephyrus-core` | Static lib | Core library (`src/zephyrus/`) |
| `zephyrus-standalone` | Executable | Standalone entry point |
| `zephyrus-acr` | Shared lib | ACR entry point (Linux `__attribute__((constructor))`) |

## Gotchas

- CMake 4.2+ required
- Linux: `zephyrus-standalone` static-links libstdc++/libgcc; `zephyrus-acr` never does (it is `dlopen`ed into the game, which already has libstdc++), and always builds with `-fvisibility=hidden`
- `NDEBUG` controls spdlog level: `trace` in debug, `info` in release
- Dependencies come from **Conan 2**, never CPM and never vendored. Non-ConanCenter recipes: `conan/recipes/` (`boost-ext-di`, `cmrc`, `zydis`)
- Injected builds do not link SDL: the SDL 1.2 function table is resolved at runtime from the game's own loaded instance (`src/zephyrus/sdl/`)
- The hook engine uses Zydis for prologue decoding; hooks are installed from the first `SDL_GL_SwapBuffers` callback to guarantee a safe point with a current GL context

## Feature Modules

| Module | Files | Notes |
|---|---|---|
| Game bindings | `include/zephyrus/acr/`, `src/zephyrus/acr/` | Offsets generated with `offsetof` against the game headers (`offsets.hpp`); symbols resolved through `dlsym(RTLD_DEFAULT, ...)` (`symbols.hpp`). `weapon::mag/ammo/gunwait` are **references**, dereference the stored pointer |
| ESP | `features/esp.*` | GL geometry (2D/2D corners/3D/3D corners, glow as extruded quads, health bars, snaplines) + RmlUi text labels (`asset/ui/esp/esp.rml`) |
| Skeleton | `features/skeleton.*` | Hooks `renderclient(playerent*)` + `md2::render`, reads the dynamic vertex cache of the player body mesh, transforms with `vertmodel::matrixstack[0]`, clusters joints by height. Player models are MD2 (vertex keyframes, no bones) |
| Rage | `features/rage.*` | Per-weapon profiles (Osiris style) with global fallback, silent aim, autofire, triggerbot, head magnet (ray-closest point, ≤1.95 cube pull), `skip_protected`, auto scope (`setscope`), no recoil (zero `pitchvel/yawvel`), hitbox select + lethal, hitchance estimation (aware of the forced packet state) and hitchance-gated firing / auto stop, shot log with miss reasons |
| Movement | `features/movement.*` | Hooks the **inner** `moveplayer(physent*, int, bool, int)` (200 Hz substep): maxspeed speedhack, auto bhop, and instant air control (WASD changes the horizontal velocity direction without turning the view). Collision stays with the game; speeds above ~3x may tunnel |
| Exploits | `features/exploits.*` | `sendposition` hook for forced scope/crouch/zero-velocity/on-floor bits; `addshotline` hook for spawn-protection mirroring + shot tracers; knife/grenade protocol actions (`SV_WEAPCHANGE` + `SV_SHOOTC` + `SV_EXPLODE`, weapon restore is **delayed** or the queued event is dropped by `weap != cs.gunselect`); blink/teleport validated through `entinmap` |
| Config | `config/config.*` | TOML (toml++). Defaults at startup; `ZEPHYRUS_ACR_CONFIG=<file>` loads a profile; every change snapshots to `~/.config/zephyrus/latest.toml` (recovery only, never auto-loaded); named profiles in `profiles/`; clipboard import/export |
| Third person | `features/thirdperson.*` | Animates the exported `thirdperson` distance variable so `camera3` pulls back/zooms in smoothly |
| Menu | `overlay/menu.*`, `asset/ui/menu/menu.rml` | Analyst-style visual language, Osiris layout (top tabs + sidebar + sections), inline keybinds (left click rebind, right click Off/Always/Hold/Toggle/Unset), profile editor, OsirisAndExtra-style Config card (profile list + name field + Create/Save/Load/Delete/Open folder/Reset/Clipboard), RmlUi animations (panel/section stagger, pop-in, number easing, slider/icon transitions) |
| Backend | `rmlui/backend/` | GL2 render interface with FBO layers (`box-shadow`) and filters (blur/color matrices). Layer compositing must save GL state **before** the filter chain runs — capturing afterwards restores the post-process FBO/viewport/blend and turns text into black blocks for the rest of the frame. SDL_image texture loading, SDL2/3 clipboard, SDL cursors; system interface feeds keys/mouse to RmlUi |

### RageBot spread model (server side, `serverevents.h`)

- `spread = guns[weap].spread * (vel.magnitude()/3 + 0.4) * 1.2 * crouchfactor`
- `applyspread` adds `(rnd(spread) - spread/2) * factor` per axis to `to`, with `factor = distance/1000 * perk * adsfactor`
- The final angular half angle per axis is therefore `spread * perk * ads / 2000` rad; client-side no-spread is **not** possible (the server owns the roll)
- Scoped snipers have `spreadrem ~ 97%`, hence auto scope makes them near perfect

### Lifecycle & unload (injected build)

- `zephyrus_acr_init()` runs from the ELF constructor (`dlopen`/GDB injection); `zephyrus_acr_shutdown()` is exported for runtime unload and is also called by the library destructor. Both are idempotent, and init can be called again after a shutdown (no `dlclose` needed).
- The game exits through `quit()` -> inlined `cleanup()` -> `SDL_Quit()`. `cleanup()` is **inlined into `quit()`**, so the exit hooks are `_Z4quitv` and `SDL_Quit` (tear the runtime down while SDL/GL are still alive, then call the original).
- `zephyrus_acr_shutdown()` refuses to run when called from inside a detour (`hook::active_detour_depth() > 0`): that would free the trampoline and owning objects that are still on the stack. Call it from game code (or retry).
- Every hook is owned: overlay hooks in `overlay::shutdown()`, skeleton hooks in `esp::shutdown()`, rage hooks in `rage::shutdown()`, exit hooks in `uninstall_exit_hooks()`. `inline_hook` restores the patched bytes in its destructor and unmaps its trampoline/island arenas once the last hook is removed, so unload/reload cycles do not leak RWX pages.
- Keep the module `dlclose`-able: spdlog must not use the default `%+` pattern — it pulls in spdlog's MDC, whose non-trivial `thread_local` map registers a TLS destructor and makes glibc keep the library mapped forever. `logging_manager` sets an explicit pattern (and colours `trace` gray via `ansicolor_sink::set_color`).

### Testing (X11)

- `scripts/acr-start.sh` (setsid + LD_PRELOAD), `scripts/acr-do.sh` (keys/mouse/screenshots), `scripts/acr-menu-click.sh` (menu-relative clicks)
- `/datassd/Clone/acr-bin/client-zephyrus.sh` launches the client with zephyrus directly (auto-detects the Debug/Release library, forces `SDL_VIDEODRIVER=x11` because the user environment exports `wayland` which SDL 1.2 cannot use)
- Test env: `ZEPHYRUS_ACR_BOTS=N`, `ZEPHYRUS_ACR_ESP=1`, `ZEPHYRUS_ACR_RAGE=1`, `ZEPHYRUS_ACR_RAGE_TEST=1`, `ZEPHYRUS_ACR_CONFIG=<profile.toml>`, `ZEPHYRUS_ACR_CMD=<path>`
- Command channel: write newline-separated commands to `$XDG_RUNTIME_DIR/zephyrus-cmd` (default path); useful for manual/scripted testing without focus games: `knife [cn]`, `nade [cn]`, `blink`, `blink_crosshair`, `goto <cn>`, `lookat <cn>`, `players`, `dump`, `key <forward|backward|left|right|jump|attack> <0|1>`, `movement 0|1`, `air 0|1`, `bhop 0|1`, `speed 0|1`, `config save|load|delete|list|folder|export|import|reset [name]`, `ui open|close|tab <name>|panel <name>|reset`
- Watch out: the game window size changes between runs — always compute click coordinates from `xdotool getwindowgeometry`; `xdotool search --name` may need a moment after launch, and `--sync` variants can block if the window is gone
- The current `linux_64_client` in `acr-bin` ships the **modelcache hunt instrumentation**: it intentionally aborts (`ACR error (6)`) when the known genshadowvolume cache corruption is detected. That crash is game-side (`docs/modelcache-shadow-cache-bug.md`), not zephyrus; use `linux_64_client.bak-20260523` (local fix build) for long runs
