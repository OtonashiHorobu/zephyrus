# zephyrus-csgo Port Plan (CS:GO HvH)

> Created: 2026-10-09 · Status: Phase 1 kick-off
> Related TODOs: [todo-shared.md](todo-shared.md) · [todo-csgo.md](todo-csgo.md)
> Constraint: for `-insecure` environments only; no anti-cheat bypass.
> Historical document: ACR has been split off to the `acr` branch and frozen (see [BRANCHES.md](../BRANCHES.md));
> the ACR targets, directories and acceptance items mentioned below are all pre-split historical state.

## 1. Goals

- Add a `zephyrus-csgo` shared library: it is injected into the CS:GO (legacy 1.38.8.1, linux64) client and
  provides the full HvH feature set (mainly following the OsirisAndExtra feature set).
- Maximize reuse of the existing infrastructure (hook engine, RmlUi menu, TOML config, logging, command channel).
- Split the repository by “game / shared”: directories, CMake targets and TODOs are all maintained separately.

## 2. Verified Facts (2026-10-09)

| Item | Conclusion |
|---|---|
| Game | `/datassd/SteamLibrary/.../Counter-Strike Global Offensive`, legacy CS:GO `1.38.8.1` / ClientVersion `1575` / VersionDate `2023-10-12`, x86-64 (`csgo/steam.inf`) |
| Modules | `csgo/bin/linux64/client_client.so` (35 MB), `bin/linux64/engine_client.so` (12.6 MB); both not stripped, but `.dynsym` has only 138 symbols (`CreateInterface`, `s_pInterfaceRegs`); `.gnu_debuglink` points to `client_client.so.dbg`, which is not shipped with the game |
| Osiris patterns | The **Linux** patterns of the csgo branch (tip `8677ddf1`, 2025-07): client hits 60/61, engine 7/7; the only miss is `uiComponentInventory` (inventory feature, not needed) |
| SDL2 | `bin/linux64/libSDL2-2.0.so.0`; all exported functions are `FF 25 rel32` thunks → the pointer slots can be replaced as Osiris does, hooking `SDL_PollEvent` / `SDL_GL_SwapWindow` with zero code patching |
| GL | `shaderapidx9_client.so` contains the string “SDL failed to create GL compatibility profile” → the game actively requests a **compat profile**; Osiris Linux renders successfully with GL3 ImGui on the same game version |
| Ghidra | MCP instance online; the `csgo` project (`/datassd/Clone/acr-bin/ghidra/csgo`) exists but **has no program yet** |
| License | Osiris = MIT (portable, keep the copyright notice); OsirisAndExtra = CC BY-NC-SA 4.0 (**logic reference only, code rewritten**) |
| Injection prerequisite | The user has already set `-insecure` in Steam; both the library and the launch script must hard-validate it and refuse initialization if it is missing |

## 3. Block Architecture

### 3.1 Directories

```
include/zephyrus/
  abstract/ hook/ log/ platform/ util/ asset/   # shared
  config/ input/ rmlui/ sdl/ sdl2/              # shared (sdl and sdl2 are separate)
  acr/          # ACR game module (existing, not removed)
  csgo/         # ★ new: SDK / interfaces / netvars / patterns / memory / entity / anim …
  features/acr/ # existing features moved here
  features/csgo/# ★ new: rage / antiaim / fakelag / resolver / backtrack / esp / chams / glow / misc …
  overlay/acr/  # existing overlay + menu moved here
  overlay/csgo/ # ★ new overlay + menu
  overlay/menu_base.hpp        # ★ shared widget mechanism extracted from the existing menu
src/ mirrors the layout; src/zephyrus_acr.cpp and src/zephyrus_csgo.cpp are the entry points
asset/ui/csgo/  # ★ CS:GO menu RML
```

Namespace convention: `zephyrus::csgo` (game/features), `zephyrus::csgo::overlay`;
ACR keeps `zephyrus::acr` / `zephyrus::features` / `zephyrus::overlay` for now (to reduce churn,
namespace cleanup can follow up in todo-acr).

### 3.2 CMake Targets

```
zephyrus-core       STATIC  shared: hook / log / config / rmlui / cmrc / version / zephyrus
zephyrus-acr-core   STATIC  ACR module + features/acr + overlay/acr
zephyrus-csgo-core  STATIC  CS:GO module + features/csgo + overlay/csgo
zephyrus-acr        SHARED  entry point (links acr-core + version-script)
zephyrus-csgo       SHARED  entry point (links csgo-core + version-script)
zephyrus-standalone EXE     still only ACR menu preview (links acr-core)
```

### 3.3 Render Backend (Answering the SDL2/GL3 Question)

- **SDL2 is mandatory**: the CS:GO layer gets a new `sdl2` function table and `system_interface_sdl2`
  (events/cursor/clipboard), resolved from the game's already loaded `libSDL2-2.0.so.0`, not linked.
- **Reuse GL2 for rendering first** (`render_interface_gl2`, which already includes the layer/filter
  implementation): the game requests a compat profile, so the fixed-function pipeline is available; this is the shortest path to a “usable version”.
- **Keep the GL3 upgrade slot reserved**: RmlUi 6.2 in the Conan cache ships the official
  `RmlUi_Renderer_GL3` (a complete layer/filter API implementation); if the injection smoke test finds
  core-profile / state-conflict issues, port it immediately. Smoke test items: record
  `GL_VERSION` / `GL_CONTEXT_PROFILE_MASK`, and verify that the fixed-function pipeline works.
- Whichever backend is used, the injection path needs full GL state save/restore in `begin_frame/end_frame`
  (program / VAO / depth / cull / active texture / matrices).
- `render_interface_gl2` currently does `#include <SDL/SDL.h>` + SDL_image texture loading;
  extract a `texture_loader` interface: ACR uses SDL1.2, CS:GO uses SDL2 (the menu has no image
  assets, so only the font path can be kept).

## 4. Phase Plan

### Phase 0 — Preparation (can run in parallel with Phase 1)
- [ ] Import `client_client.so` / `engine_client.so` into Ghidra (background auto-analysis).
- [ ] Add the Osiris Linux pattern scanning script to the repo (`scripts/` or `tools/`) and validate it against every game update.
- [ ] `scripts/csgo-start.sh`: LD_PRELOAD injection + `-insecure` assertion.
- [x] Version/BuildID probing and pattern availability verification (see §2).

### Phase 1 — Block Split and Shared Infrastructure (in progress)
The ACR block relocation + build split are done (build/tests green, history in the `acr` branch);
`rmlui_manager` is decoupled from SDL1.2; the SDL2 module/menu base extraction is in [todo-shared.md](todo-shared.md).
Acceptance: ACR build + tests + standalone + in-client testing show no regressions (the in-client items are still pending).

### Phase 2 — CS:GO Foundation (injection → drawing frames) (in progress)
Done: the `zephyrus-csgo` target + entry point (`-insecure` guard / version check / module / interface / pattern probe)
+ `zephyrus_csgo_probe` + `scripts/csgo-start.sh`; the library can safely refuse initialization without `-insecure`.
See [todo-csgo.md](todo-csgo.md). Remaining: SDL2 overlay, menu, actually drawing frames.

### Phase 3 — Usable HvH Version (v1)
Following the OsirisAndExtra feature set: Ragebot (hitchance/autofire/autoscope/autostop/
no-recoil/autowall/backtrack integration), Triggerbot, Backtrack, Anti-Aim, Fake Lag,
Resolver v1, ESP (box/health/name/weapon/skeleton), Bhop/AutoStrafe, menu + config.
Acceptance: playable on a local `-insecure` server + bots; two clients/LAN can verify desync behavior;
community HvH servers are retested by the user.

### Phase 4 — Remaining OsirisAndExtra Features
Chams, Glow, Visuals (no-smoke/no-flash/crosshair series/scope override/skybox, etc.), Misc
(spectator list/bomb timer/hitmarker/watermark…), EnginePrediction, Knifebot,
GrenadePrediction, Tickbase/DT (engine-level, done last).

### Phase 5 — Resolver / AA Iteration (ongoing)
Refine per the §5 design, tuning with hit logs and measured data.

## 5. HvH Design: Anti-Aim / Resolver Improvements

Using OsirisAndExtra as a reference, **rewrite and enhance** it along the following lines (do not copy its CC code verbatim).

### 5.1 Anti-Aim
1. **Precise desync**: derive the real torso yaw from both the `animstate` fields and
   `m_flPoseParameter[6]`; adjust `58° * modifier` per duck/air/move rather than using a fixed value.
2. **Precise LBY exploitation**: replicate the server-side update conditions (0.22 s after moving >0.1 or z>100, otherwise 1.1 s), and
   have Sway/Spin keep the foot yaw changing continuously within the window; avoid O&E's fixed flip.
3. **Jitter/Spin/Freestanding**: allow roll under `-insecure`; scan for freestanding based on enemy
   positions (multiple yaw samples + occlusion evaluation) instead of a single auto-direction.
4. **On-shot AA**: separate the firing packet from the fake packet (combined with fakelag/tickbase hide-shots),
   keeping the fake yaw while shooting so a shot-resolver cannot catch the real angle.
5. **Anti-resolve**: randomize the jitter period/amplitude (seeded PRNG) instead of relying on the predictable
   `sendPacket` flip; tier by weapon/movement state (O&E already has tiers; keep and extend them).
6. **Manual**: left/right/back keys + auto-direction, retained.

### 5.2 Resolver
1. **Per-record resolve**: each backtrack record captures eye/foot yaw, pose, anim layers,
   simtime, velocity, matrix; resolution rebuilds the matrix on a copy of the record and **does not touch the live animstate**
   (O&E modifying the live animstate pollutes rendering/backtrack).
2. **Detector stack + confidence**: static desync (layer3/lean + pose sign), jitter (direction/
   magnitude of the footYaw jump between adjacent choke packets), LBY (target update timing), spin (angular velocity estimate +
   latency-based prediction), freestanding (lateral reachability via trace).
3. **Shot resolver**: infer the real angle for a firing enemy from fire yaw/impact;
   use `weapon_fire` / `bullet_impact` / `player_hurt` / `player_death` events as feedback.
4. **Brute-force state machine**: angle buckets (e.g. ±60/±40/±20/0) with per-player persistent hit statistics,
   missed angles go on a blacklist, which is reset by policy on death/round change; classify misses
   (resolver miss vs spread/no damage) and only adjust for resolver misses.
5. **Aimbot coordination**: prefer the resolved hitbox; hitchance/autowall are based on the resolved matrix;
   logs show the resolve mode/side/hit rate.
6. **Visual debugging**: the menu shows each player's resolve mode, hit rate and latest miss; the command channel can dump them.

### 5.3 Linux Offsets That Must Be Reversed Manually (use Ghidra in Phase 0/3)
- `CCSPlayerAnimState`: `eyeYaw`/`footYaw`/`lastUpdateTime`/`duckAdditional`/
  `onGround`/`walkToRunTransition` (Osiris only provides the maxDesync-related fields).
- `C_BaseAnimating`: reading/writing `m_flPoseParameter`, `GetPoseParameter/SetPoseParameter`.
- Animation layer structures (`m_AnimOverlay` / `CAnimationLayer`: weight/cycle/playbackrate/order).
- `UTIL_TraceToExit` (Linux autowall; Osiris's Windows asm returns false on Linux).
- `CNetChan`/`CClientState`: `chokedPackets`, `m_nChokedCommands`, `SendDatagram`.
- Random seed: `m_nPredictionRandomSeed` / `RandomFloat/RandomInt` (no-spread).
- Obtaining the `sv_maxusrcmdprocessticks` value (prerequisite for Tickbase/DT).

## 6. Testing and Safety

- **Safety**: parse `/proc/self/cmdline` before library initialization; without `-insecure` → refuse initialization
  (log it, install no hooks). The launch script asserts the same. If testing finds the game was started without `-insecure`
  → close the game immediately; injection is forbidden.
- Command channel: `ZEPHYRUS_CSGO_CMD` (default `$XDG_RUNTIME_DIR/zephyrus-cmd`),
  following the ACR command style: `ui open|close|tab …`, `config …`, `dump`, `players`,
  `esp/rage/aa/fl …` toggles, which makes focus-free automated testing easy.
- Test environment: local `-insecure` + `sv_lan 1` + bots (shooting/movement/backtrack);
  desync/resolver require two clients or a community HvH server (retested by the user).
- Keep the build/tests green at every phase.

## 7. Risks

| Risk | Mitigation |
|---|---|
| Game uses a core profile → fixed-function pipeline unavailable | Smoke test records the profile; port the official GL3 backend immediately |
| AnimState/animation layer offsets unknown | Ghidra RE + runtime log verification; resolver v1 can fall back to the pose+eye approach |
| Injected GL state polluted by the game (black blocks/flicker) | Full state save/restore around begin/end frame (reusing the black-frame fix experience from the GL2 era) |
| SDL2 relative mouse/cursor behavior | Reference the Osiris `ImGui_ImplSDL2` path + manual control with `SDL_SetRelativeMouseMode` |
| The legacy client no longer updates | Hard validation via BuildID/version; when a pattern fails, disable the corresponding feature instead of crashing |
| Unload safety | Follow the ACR lifecycle (reject via `active_detour_depth`, destructor fallback) |

## 8. Reference Code Mapping (Osiris csgo branch → zephyrus)

| Osiris | zephyrus-csgo |
|---|---|
| `Source/CSGO/*` | `zephyrus/csgo/sdk/*` |
| `Source/Interfaces/*` | `zephyrus/csgo/interfaces.*` |
| `Source/Netvars.cpp` | `zephyrus/csgo/netvars.*` |
| `Source/Memory.h/.cpp` | `zephyrus/csgo/memory.*` |
| `Source/BytePatterns/Linux*` | `zephyrus/csgo/patterns.*` (+ our own RE offsets) |
| `Source/Hooks/{Client,ClientMode,ClientState,ModelRender,…}Hooks.h` | `zephyrus/csgo/hooks/*` |
| `Source/Hacks/{Aimbot,Triggerbot,Backtrack,Chams,Glow,Visuals,Misc,EnginePrediction}` | `zephyrus/features/csgo/*` |
| OsirisAndExtra `{AntiAim,Resolver,Fakelag,Tickbase,Ragebot,Animations,Knifebot,GrenadePrediction}` | logic reference, rewritten per §5 |

## 9. To Be Confirmed

- Rendering: GL2 first per §3.3, GL3 on failure (say so if you insist on going straight to GL3).
- Directory refactor: files move into `features/acr`, `overlay/acr`, ACR namespaces stay untouched for now.
- v1 scope: Phase 2+3 together form the “usable version”; Phase 4 is appended later.
