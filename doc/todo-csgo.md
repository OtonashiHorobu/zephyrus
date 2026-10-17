# zephyrus TODO — csgo (new game)

> Maintenance rules: see [todo-shared.md](todo-shared.md); overall plan: [csgo-port-plan.md](csgo-port-plan.md).
> Feature reference: OsirisAndExtra (rewritten); SDK/patterns reference: Osiris csgo branch (MIT, portable).

## F0. Safety and injection

- [x] `-insecure` hard validation: parse `/proc/self/cmdline`; if missing → refuse initialization
      (stdout + log), never install any hook. (Verified by loading via `ctypes.CDLL` without
      `-insecure`: the library loads normally, prints the refusal message, and does not initialize.)
- [x] Version validation: read PatchVersion/ClientVersion from `csgo/steam.inf`; warn on mismatch.
- [x] `scripts/csgo-start.sh`: `LD_PRELOAD` + `-insecure` assertion + Steam scout runtime wrapper
      (`libcurl-gnutls.so.4`) + force X11 + `WINDOWED=1` (workaround for the XWayland fullscreen
      black screen). The Steam launch-option form is documented in the script header. Direct
      launch has been tested and reaches the game.
- [ ] In-game injection test (the script is ready; do it after the overlay exists).

## F1. SDK basics (Phase 2)

- [x] Module loading: `hook::find_module` (basename match, dl_iterate_phdr) + probe logging.
- [x] Interface acquisition: `CreateInterface` (RTLD_NOLOAD + dlsym; `s_pInterfaceRegs` fallback
      to be done later).
- [x] `patterns`: the probe has runtime-validated 4 Osiris Linux patterns (localPlayer etc.).
- [x] `netvars`: recv-table traversal (`src/zephyrus/csgo/netvars.cpp`).
- [x] Full `patterns` port (Linux client table + engine table, `patterns.cpp`).
- [x] `memory`/`game`: localPlayer/globalVars/clientMode/input/engine/engineTrace/
      cvar/glowObjectManager (pattern)/weaponSystem/playerResource/plantedC4s/
      gameRules/lineGoesThroughSmoke/isOtherEnemy/setAbsOrigin/….
- [x] `entity`/`usercmd`/`view_setup`/trace/bone (`virtual_call` + netvar accessors).
- [x] Startup dump: module base addresses, version, interfaces, pattern hit list
      (`zephyrus_csgo_probe`).

## F2. Overlay (Phase 2)

- [x] `SDL_PollEvent` pointer-slot hook (events go to RmlUi first; events are swallowed while the
      menu is visible; inline fallback in place).
- [x] `SDL_GL_SwapWindow` pointer-slot hook (draw the frame + original swap; inline fallback in
      place).
- [x] Cursor/relative mouse: while the menu is visible, `SDL_SetRelativeMouseMode(SDL_FALSE)` +
      show cursor; restore on close (reset every frame so the game cannot take it back).
- [x] Delayed installation: SDL2 is loaded by the engine after the constructor; a background
      thread polls and swaps the pointer slots.
- [x] Menu skeleton `asset/ui/csgo/menu.rml` (smoke-test document: render/input/cursor chain).
- [x] In-game test: restart the game → INSERT opens/closes → log `csgo overlay hooks installed` /
      `csgo overlay ready`; menu rendering and cursor restore verified.
- [x] In-game mouse fix: while the menu is visible, intercept `SDL_SetRelativeMouseMode(TRUE)` /
      `SDL_WarpMouseInWindow` / `SDL_ShowCursor(0)`, and synthesize down/up by polling
      `SDL_GetMouseState` every frame (the game consumes key events through its own event path;
      the event queue is unreliable).
- [x] `-insecure` smoke test: GL_VERSION / GL_CONTEXT_PROFILE_MASK logging added (on the test
      machine: `GL_VERSION=4.6.0 NVIDIA` / `profile_mask=0x0` → compatibility context,
      fixed-function pipeline available; the GL3 backend stays in use, GL2 kept as a fallback).
- [x] Command channel + test script (`ui open|close|toggle|tab|panel`/`dump`/`players`/
      `rage`/`aa`/`fl`/`bt`/`esp`/`glow`/`autowall`/`fov`/`bhop`/`trace`/
      `config save|load|delete|list|reset|export|import|folder [name]`;
      `scripts/csgo-cmd.sh`). Exported `zephyrus_csgo_toggle_overlay` /
      `zephyrus_csgo_probe` for gdb automation.

## F3. HvH v1 features (Phase 3)

- [x] Hooks: ClientMode `createMove`(25, supports overriding sendPacket) / `overrideView`(19);
      Client `frameStageNotify`(37, animation recording).
      **(Round 3 removed unverified items: getViewModelFov 36 / doPostScreenEffects 45 /
      shouldDrawFog 18 / dispatchUserMessage 38 / UpdateClientSideAnimation 286.)**
- [~] `EnginePrediction` (velocity/flags, O&E reference): full implementation
      (`features/csgo/prediction.*`, IPrediction 20/21, GameMovement 1); automatically disabled
      because `restoreEntityToPredictedFrame` is not reverse-engineered (re-f3 B1).
- [x] `Animations` framework: per-player records (matrix, pose, layers, simtime,
      velocity), 48 history entries for backtrack/resolver; `UpdateClientSideAnimation`
      hook + real-matrix pipeline are live (fake matrix / forward fake pose need A5/A6).
- [x] Ragebot enhancements: Monte-Carlo hitbox hitchance (`hitchance.*`, based on the aimed
      record's bones; backtrack records also work), autowall damage model (`autowall.*`,
      engine trace rewrite of TraceToExit; `IPhysicsSurfaceProps::GetSurfaceData`
      verified on a core as **slot 6**), per-category config (7 categories by default).
      **no-spread removed entirely**: CS:GO spread is decided by server-side randomness and
      cannot be removed client-side (`MD5_PseudoRandom` measured as `digest+6`; archived in
      the re-abi doc only).
- [~] AntiAim enhancements: pitch (down/up/zero/fake)/yaw (backward/right/left/
      jitter/spin/static/at-targets), choke-flip desync (static/jitter/
      sway/auto-direction), LBY break, manual keys (raw overlay key events),
      auto-direction scan. Real animstate writes (fake pose) need A5/A6.
- [x] Triggerbot: **dropped at the user's request** (HvH-only).
- [x] Backtrack v1 → v2: records moved to Animations (bones/simtime),
      select/rewind + per-record resolve and smoke check (no more trace per record).
- [x] AntiAim v1: pitch (down/up/zero), yaw (backward/jitter/spin) + desync jitter.
- [x] Fakelag v1: static/adaptive/random + on-shot; added `preview()` so AA can tell whether
      this tick sends (deterministic per-tick limit).
- [x] Resolver v2: movement speed / AnimState footYaw (Linux 0x84, runtime sanity check) /
      `m_flLowerBodyYawTarget` detection stack + shot-count brute-force; per-record position
      rotation.
- [x] ESP v1 (RmlUi overlay, independent of the menu): box/health/name/weapon/distance/snapline +
      visible/team colouring; skeleton pending.
- [x] ~~Glow~~ → removed entirely in round 3 (`glowObjectManager` pattern unverified; enabling it
      crashed).
- [x] Visuals: no-flash (`m_flFlashMaxAlpha` zeroed); remove scope / no-fog removed in round 3
      (slots unverified and behaviour wrong: 18 is a `return true` stub, 45 skips
      `DoPostScreenEffects` without hiding the scope overlay).
- [x] Movement: bhop, auto-strafe, slow-walk (key, overlay key capture),
      movement fix (keep the world direction after AA changes angles).
- [x] Menu/config: Rage/AA/Backtrack/Fakelag/Movement/ESP/Resolver all bound to the menu and
      persisted to `csgo/latest.toml` (`[features]`); added a Weapons page (by weapon category),
      manual key capture, desync/LBY controls. (`ZEPHYRUS_CSGO_*` environment-variable mode was
      replaced by TOML. Glow page removed in round 3.)

### Second fix round (2026-10-10, review + core analysis)

- [x] **TraceRay calling-convention crash (SIGSEGV)**: `virtual_call` passed `Ray_t`/
  `trace_t`/`Vector` by value; under SysV, MEMORY-class aggregates go on the stack and leave
  garbage in registers; the engine dereferences them as pointers (core: `rep stos %rax,(%rdi)`,
  rdi=1). Fix: typed wrappers `engine_trace_ray()` / `engine_point_contents()` in `entity.hpp`,
  all parameters as pointers; `ITraceFilter` lost its mistakenly added virtual destructor
  (slot 0=ShouldHitEntity, 1=GetTraceType); `trace` size static_assert 0x68 (the engine clears
  104 bytes).
- [x] **`IPhysicsSurfaceProps::GetSurfaceData` crash**: Osiris's slot 5 is actually
  `GetPhysicsProperties` (4 output pointers) → corrected to **slot 6** (runtime vtable +
  disassembly verification); `surfacedata_t` field offsets measured on a core.
- [x] **RageBot / AntiAim / FakeLag order**: rage runs first (FOV uses the real view; the autofire
  bit is visible to fakelag on_shot / AA disable_on_shot), fakelag decides send, AA last;
  AA does not overwrite angles on a tick the ragebot already aimed (O&E semantics, `original_angles`).
- [x] **no-spread removal** (the server owns the randomness; UI/commands/module all deleted),
  `MD5_PseudoRandom`/`UpdateSpread` RE results archived in `doc/csgo-re-abi.md`.
- [x] **ESP fixes**: RML escaping for player/weapon names (weapon name with the `#SFUI_WPNHUD_`
  prefix stripped); clear the layer on close/death; render rows no longer reference the old
  document; use `GetAbsOrigin`; **behind-camera target projection guard** (fixes boxes
  "stuttering/sky/underground").
- [x] **Menu rework**: real profile management on the Config page (save/load/delete/reset/export/
  import/folder, synchronized with `config_*.` commands), scrolling, drag viewport clamping,
  bottom-right resize, combo/keybind popups with Off/Always/Hold/Toggle/Unset modes, keybind
  persistence (raw SDL2 scancode storage), Escape cancels capture and restores the old value,
  slider dragging without transition, `config_.tick()` debounce (no more synchronous disk writes
  per change), live FPS display.
- [x] **Align built-in UI with actual capabilities**: removed unimplemented controls
  (Chams/Tickbase/Indicators/Skeleton/smoke/no-flash (kept)/crosshair/Max distance/Multipoint/
  No spread).
- [x] **Config directory split**: `~/.config/zephyrus/acr/{latest.toml,profiles}` and
  `~/.config/zephyrus/csgo/{latest.toml,profiles}`; one-time migration of the old ACR top-level
  config; CS:GO no longer lists ACR profiles. Version string is product-specific
  (`Zephyrus CS:GO ...`).
- [x] **Bug fixes**: team check inversion, backtrack `time_limit` now effective, animation records
  only enqueued when `setup_bones` succeeds, hook installation can retry after failure (no longer
  marks installed prematurely), re-injection resets `overlay_stop`, `server_time` reuses the cache
  within the same tick, AA static yaw_add double count, LBY single-tick flick, resolver
  `on_hit/on_miss` (rage confirms a hit via health delta) + brute-force after a miss in auto mode.
- [x] **Ghidra naming/comments committed to the project**: TraceRay/GetPointContents/GetPlayerInfo/
  CHLClient 37/38/MD5/GetSurfaceData and vtable labels written into the Ghidra project.
- [x] **Performance (menu frame drops)**: perf analysis pinned RmlUi full-document relayout at 60ms
  per pass (triggered by the FPS text refreshing every 500ms; shrink-to-fit recursion in deep flex
  nesting, measured cost halved per removed nesting level). Fix: `#content-scroll`/`.section`
  changed to block, `.ctrl` uses `flex: 1` (basis 0, parent no longer measures max-content),
  `.combo` changed to block + floating caret. Relayout spikes 60→8-12ms; `Context::Update`
  mean 1.3→0.5ms; in Debug builds the GL3 renderer's per-draw `glGetError` now only checks when
  `ZEPHYRUS_GL3_LOG` is set, `Context::Render` CPU 1.3→0.6ms. Overlay cost with the menu open is
  about 1.1ms/frame (at 150fps), no perceptible drops. Debug commands (command channel):
  `perf fps|anim on|off`, `style <selector> <prop> <value|->`, `ui hide|show <id>`.
- [ ] The game's own `DoPostScreenEffects` path still occasionally SIGSEGVs (core: argument is a
  truncated pointer `0xef11fdc8`, in the game's glow render loop, not zephyrus code);
  needs a **vanilla no-injection control** (cannot test while the Steam runtime is not running)
  or multiple observation rounds.

### Round 3: RE audit and deletions (2026-10-10, user reported suspected wrong hooks)

User report: no scope overlay ineffective, no fog hides the viewmodel, enabling glow crashes,
ESP still flickers at wrong positions, bot names disappear from the scoreboard/kill feed
(normal without injection). Requirement: delete every hook/pattern not personally verified in
Ghidra.

- [x] **ClientMode vtable measurements**: static instance `0x06e04200`, constructor `0x00ff8b20`
  installs table `0x022d14b0`; slots 17/18/29/14 = `return true` stubs (no-fog returning
  false through it also hides the viewmodel); slot 45 = glow rendering (skipping it does not
  hide the scope overlay). See re-abi §11.
- [x] **Removed**: no-fog (18), no-scope (45), viewmodel FOV (36),
  `DispatchUserMessage` (38, trace logging only), `UpdateClientSideAnimation`
  hook (fake matrix not implemented), the entire glow module + `glowObjectManager` pattern,
  the entire prediction module (depended on never-resolved `client_state`/`restoreEntity…`,
  effectively always disabled), `SetAbsOrigin`, `line_goes_through_smoke`, `is_other_enemy`,
  and all unused Osiris patterns (`view_render_beams`/`weapon_system`/
  `player_resource_slot`/`planted_c4s`/`game_rules_slot`/`view_render`/
  `send_clan_tag`/`send_datagram`), entity 348/409/218.
- [x] **ESP ghost boxes (wrong-position flicker) fix**: backtrack's `rewind` used to write the
  enemy's abs origin / simulation time to the historical position and never restore it (an
  unverified `SetAbsOrigin` pattern write) → after a shot the ESP drew boxes at old positions.
  It now only changes `cmd.tick_count` and never writes entities.
- [x] **Team checks use the team number** (entity vtable 128 verified as `this+0x12C`); no longer
  calls the unverified `isOtherEnemy`.
- [x] **eye_position uses a netvar**: `origin + m_vecViewOffset[0]`; the old vtable 348
  is actually a multi-argument function, and calling it with no arguments read garbage from
  registers.
- [x] References kept and verified in Ghidra: entity 12/128/167/208/210, renderable 13
  (SetupBones), networkable 9/10, engine 8/18/19/20/37/78, CHLClient 37,
  ClientMode 19/25, localPlayer pattern (single hit at `0x008f4e11`).
- [ ] Bot-name-disappearance regression: needs in-game confirmation (retest the scoreboard /
  kill feed after removing the `DispatchUserMessage` hook).

### F3 in-game verification (2026-10-10)

- [x] **Fix game initialization never succeeding**: `game::create()` used the wrong interface name
      `VEngineTraceClient004` (correct: `EngineTraceClient004`); all feature hooks had never
      actually been installed before. In-game logs confirm `game resolved` +
      `patterns: resolved (0 missing)` + `netvars: 1797 properties` + client mode hooks
      (including shouldDrawFog/dispatchUserMessage) +
      `UpdateClientSideAnimation hook installed (index 286)`.
- [x] Main menu → map load: fixed the overlay installer thread timing out and nobody resolving
      the game afterwards (`on_ready` retries `initialize_game` every 15 frames), and the menu
      initializing before feature objects exist causing unbound controls
      (`refresh_menu_bindings`).
- [x] Menu sidebar background break fix (explicit height/align-self).
- [x] Command channel tested (`dump`/`players`/`rage on`/…), `players` prints
      eye_yaw/layers/animstate pointer diagnostics.
- [x] Glow changed to "take enemies from the entity list → match manager entry pointers only"
      writes, avoiding virtual calls on suspicious pointers inside the manager.
- [x] ESP crash located (core dump backtrace: `esp.cpp` collideable `virtual_call<1>`
      on an object without a vtable): switched to the verified netvar bounding box
      (`CBaseEntity->m_vecMins` 0x388 / `m_vecMaxs` 0x394), with collideable
      only as a vtable-checked fallback.
- [~] AnimState fields still need in-game confirmation: `m_PlayerAnimState` (0xA248) returns a
      64-bit pointer for live players, but +0x28/+0x84 measured as 0 (a freshly spawned bot may
      not have updated yet, or the offsets are wrong); `anim_state_view` has pointer/sanity
      checks, and the resolver safely falls back to LBY/brute-force.
- [ ] Full ESP/Glow in-game regression (live players + stable rounds): the local test environment
      occasionally crashes in panorama_gl on round changes (**pre-existing**, core backtrace in
      the game's own panorama_gl_client.so render thread); needs retesting in the user's
      environment to distinguish.

> Remaining F3 limitations: EnginePrediction (`restoreEntityToPredictedFrame`, re-f3 B1),
> fake matrix / forward fake pose (`SetAbsAngle`/`m_angAbsRotation`, A5/A6),
> precise hitbox capsules (D2/D3), AnimState yaw field confirmation (A3 runtime retest).
> See `doc/csgo-re-f3.md` for the first/second Ghidra rounds.

## F4. Remaining OsirisAndExtra features (Phase 4)

- [ ] Chams (material system + KeyValues; Osiris Linux path available).
- [ ] Glow (`GlowObjectManager`, Linux argument path).
- [ ] Visuals: no-scope zoom, remove scope overlay, no smoke/flash, skybox,
      world color, bullet tracers, sniper/recoil/penetration crosshair.
- [ ] Misc: spectator list, bomb timer, hitmarker, watermark, nade trajectory,
      fake duck, slow walk, quick stop, auto peek.
- [ ] Knifebot, GrenadePrediction (O&E reference).
- [ ] Tickbase/DT/fake latency (last; depends on RE of `SendDatagram`/`sv_maxusrcmdprocessticks`).

## F5. Resolver / AA iteration (Phase 5, ongoing)

- [ ] Hit-feedback loop: `weapon_fire`/`bullet_impact`/`player_hurt`/`player_death`.
- [ ] Brute-force bucket persistence (by userid) + miss classification (resolver/spread/no damage).
- [ ] Resolver debug UI: mode/side/hit rate/last miss; command `resolver dump`.
- [ ] AA anti-resolve randomization: seed, periodic randomness, per-state tiers; calibrate with
      two clients.

## RE. Reverse-engineering checklist (Phase 0/3/4)

> **Measured ABI / vtable / struct offsets are summarized in [csgo-re-abi.md](csgo-re-abi.md)**;
> function names and comments in the Ghidra project are kept in sync (TraceRay,
> GetPointContents, GetPlayerInfo, CHLClient 37/38, MD5_PseudoRandom, GetSurfaceData, etc.).

- [x] Ghidra imported `client_client.so` + `engine_client.so` (project `csgo`);
  `vphysics_client.so` also imported (physics surface props verification).
- [x] Osiris Linux pattern validation against the local binaries is scripted.
- [ ] `CCSPlayerAnimState` field offsets (eyeYaw/footYaw/lastUpdateTime/duckAdditional/
      onGround/walkToRunTransition).
- [ ] `m_flPoseParameter` read/write / `GetPoseParameter`.
- [ ] Animation layers `m_AnimOverlay` (weight/cycle/playbackrate).
- [x] `UTIL_TraceToExit` (autowall) → engine trace rewrite using
  `IPhysicsSurfaceProps` (GetSurfaceData=slot 6, surfacedata offsets measured on a core).
- [ ] `CNetChan`/`CClientState` (chokedPackets, SendDatagram);
  `CClientState` globals and getters named (`CClientState_GetGlobal/GetByIndex`).
- [~] Random seed/`RandomFloat`: MD5_PseudoRandom(digest+6) and UpdateSpread
  located and archived; the feature was removed because the server is authoritative.
- [ ] `sv_maxusrcmdprocessticks` value (Tickbase/DT prerequisite).
- [x] Every offset recorded: Ghidra address + pattern + verification log (see the re-abi doc).

## T. Testing

- [ ] Local `-insecure` + `sv_lan 1` + bots: aim/backtrack/movement.
- [ ] Two clients/LAN: desync, resolver, fakelag behaviour verification.
- [ ] Community HvH servers (user retest): live logs + tuning.
