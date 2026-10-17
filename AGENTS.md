# AGENTS.md

## Branch

This is the **`csgo`** branch, the main development branch. The ACR version is
frozen on the `acr` branch; the two trees do not contain each other's game
code. See `BRANCHES.md`. Everything below applies to the CS:GO build only.

## Documentation

All documentation in this repository is written in English: `AGENTS.md`,
`BRANCHES.md`, `doc/*.md` and `scripts/README.md`. Keep new and updated docs in
English; do not reintroduce Chinese prose.

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
- **Dependency injection**: Boost.DI (`boost/di.hpp`), wired in `zephyrus_csgo_init()`
- **Asset embedding**: CMRC — add resources in `asset/CMakeLists.txt`, declare with `CMRC_DECLARE` in headers
- **Logging**: spdlog with custom `ZEPHYRUS_*` macros (`ZEPHYRUS_TRACE`, `ZEPHYRUS_INFO`, etc.)

## Build Targets

| Target | Type | Description |
|---|---|---|
| `zephyrus-core` | Static lib | Shared core: hook engine, config, RmlUi runtime, logging, version |
| `zephyrus-csgo-core` | Static lib | Source SDK bindings, patterns/netvars, features, overlay/menu, SDL2 function table |
| `zephyrus-csgo` | Shared lib | Injected entry point (Linux `__attribute__((constructor))`) |

## Gotchas

- CMake 4.2+ required
- Linux: `zephyrus-csgo` is `dlopen`ed into the game process, which already has libstdc++; never link a second copy. It builds with `-fvisibility=hidden`, `-Wl,--exclude-libs,ALL` and the version script `cmake/zephyrus-csgo.map` (exports only `zephyrus_csgo_*`)
- `NDEBUG` controls spdlog level: `trace` in debug, `info` in release
- Dependencies come from **Conan 2**, never CPM and never vendored. Non-ConanCenter recipes: `conan/recipes/` (`boost-ext-di`, `cmrc`, `zydis`)
- Injected builds never link SDL2: the SDL2 function table is resolved at runtime from the game's own loaded instance (`src/zephyrus/sdl2/`). `SDL_PollEvent` / `SDL_GL_SwapWindow` are hooked through their `FF 25 rel32` pointer slots, with an inline-hook fallback
- The library refuses to initialize unless the game was launched with `-insecure`; it also validates `csgo/steam.inf` and warns on a version mismatch
- SDL2 is loaded by the engine *after* our ELF constructor, so the overlay installs from a background thread that polls for the module and then swaps the pointer slots
- The hook engine uses Zydis for prologue decoding; hooks are installed from the first present callback, when a GL context is current
- Only vtable slots / patterns / offsets verified in Ghidra may be used. Unverified hooks and patterns are deliberately removed (see the third audit round in `doc/todo-csgo.md`); do not re-add Osiris patterns without checking them against the local binaries
- The overlay uses the official RmlUi GL3 renderer and saves/restores the host GL state around every frame; the old GL2 render interface belonged to ACR and is gone

## Feature Modules

| Module | Files | Notes |
|---|---|---|
| Game bindings | `include/zephyrus/csgo/`, `src/zephyrus/csgo/` | `CreateInterface` (`RTLD_NOLOAD` + `dlsym`), vtable wrappers, recv-table netvars (`netvars.cpp`), Linux patterns (`patterns.cpp`), entity/usercmd/view_setup/trace/bone helpers |
| Animations | `features/csgo/animations.*` | Per-player history: matrices, pose, animation layers, simulation time, velocity (48 records) for backtrack/resolver; fed from `frameStageNotify` (37) |
| Rage | `features/csgo/rage.*` + `hitchance.*` + `autowall.*` | Per-weapon-category profiles, silent aim, autofire, hitbox select + lethal, Monte-Carlo hitchance from the targeted record's bones, autowall damage model (engine trace + `TraceToExit`, `GetSurfaceData` slot 6) |
| AntiAim | `features/csgo/anti_aim.*` | Pitch (down/up/zero/fake), yaw (backward/right/left/jitter/spin/static/at-targets), choke-flip desync (static/jitter/sway/auto-direction), LBY break, manual keys, movement fix |
| Backtrack | `features/csgo/backtrack.*` | Selects/rewinds animation records; rewind only rewrites `cmd.tick_count` and never writes entity origins |
| Fake lag | `features/csgo/fake_lag.*` | Static/adaptive/random + on-shot, drives `sendPacket`; `preview()` lets AA know whether this tick sends |
| Resolver | `features/csgo/resolver.*` | Foot-yaw / LBY detection stack, brute-force by shot count, on-hit/on-miss feedback, per-record position rotation |
| Movement | `features/csgo/movement.*` | Bunnyhop, auto-strafe, slow-walk (overlay key capture), movement fix after AA changes angles |
| Visuals | `features/csgo/visuals.*` | No-flash (`m_flFlashMaxAlpha`) and FOV changer — only the verified paths remain |
| ESP | `features/csgo/esp.*` | RmlUi overlay layer independent of the menu: box/health/name/weapon/distance/snapline, visible/enemy colouring |
| Overlay / Menu | `overlay/csgo/*`, `asset/ui/csgo/menu.rml` | SDL2 pointer-slot hooks, RmlUi GL3, tabs RAGEBOT / VISUALS / MISC / CONFIGS, inline keybinds (raw SDL2 scancodes), config profile card, FPS/perf readout |
| Backend | `rmlui/backend/` | GL3 render interface (`render_interface_gl3.*`), SDL2 system interface, SDL2 function table and backend (`backend_sdl2_gl3.cpp`, `system_interface_sdl2.*`) |
| Config | `config/config.*` | TOML (toml++). Defaults at startup, persisted to `~/.config/zephyrus/csgo/latest.toml`, named profiles in `.../csgo/profiles/`, debounced autosave, clipboard import/export |

### Injected lifecycle & unload

- `zephyrus_csgo_init()` runs from the ELF constructor (`LD_PRELOAD` / gdb injection). It validates `-insecure`, creates the DI runtime, probes modules/interfaces/patterns and starts the overlay installer thread.
- `zephyrus_csgo_shutdown()` is exported for runtime unload and is also called by the library destructor. Both are idempotent; init can run again after a shutdown (no `dlclose` needed).
- Hooks are owned: overlay hooks in `overlay::shutdown()`, client-mode hooks in `client_mode_hooks`, feature hooks in each feature's shutdown. `inline_hook` restores the patched bytes in its destructor and unmaps its trampoline/island arenas once the last hook is removed.
- Do not call `zephyrus_csgo_shutdown()` from inside a detour (`hook::active_detour_depth() > 0`): that would free the trampoline and owning objects still on the stack.
- Keep the module `dlclose`-able: spdlog must not use the default `%+` pattern — it pulls in spdlog's MDC, whose non-trivial `thread_local` map registers a TLS destructor and makes glibc keep the library mapped forever.

### Testing (X11)

- `scripts/csgo-start.sh` launches the legacy linux64 client with `LD_PRELOAD`, asserts `-insecure`, forces X11 and windowed borderless (Steam launch options are documented in the script header)
- `scripts/csgo-cmd.sh <command...>` writes to the command channel; the library reads `$ZEPHYRUS_CSGO_CMD` (default `$XDG_RUNTIME_DIR/zephyrus-cmd`) on the next tick and deletes the file after processing
- Useful commands: `ui open|close|toggle|tab <name>|panel <name>`, `rage|aa|fl|bt|esp|autowall|fov|bhop on|off`, `desync ...`, `dump`, `players`, `netvar <name>`, `trace`, `perf fps|anim on|off`, `style <selector> <prop> <value|->`, `config save|load|delete|list|reset|export|import|folder [name]`
- gdb-exported entry points: `zephyrus_csgo_init`, `zephyrus_csgo_shutdown`, `zephyrus_csgo_probe`, `zephyrus_csgo_toggle_overlay`, `zephyrus_csgo_set_fov`, `zephyrus_csgo_set_bhop`
- Environment: `GAMEDIR`, `ZEPHYRUS_LIB` (defaults to the newest Debug/Release build), `LOG`, `WINDOWED`, `BORDERLESS`, `GPU`
- The X11 window size changes between runs — always compute coordinates from `xdotool getwindowgeometry`; `xdotool search --name` may need a moment after launch
