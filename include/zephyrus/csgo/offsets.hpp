// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Linux field offsets for legacy CS:GO 1.38.8.1 (linux64). Values are
// recovered from `client_client.so`/`engine_client.so` with Ghidra and
// validated at runtime by the feature that uses them: a zero value means
// "not located yet" and every consumer must degrade gracefully instead of
// dereferencing it. Windows counterparts from OsirisAndExtra are quoted next
// to each entry for orientation.

#pragma once

#include <cstddef>

namespace zephyrus::csgo::offsets {
// --- C_CSPlayer / C_BasePlayer -------------------------------------------
/// `C_CSPlayer::m_PlayerAnimState` (`CCSPlayerAnimState*`). Windows: 0x3914.
/// Linux: set right after `C_CSPlayer::CreateAnimState()` in the entity
/// constructor (`MOV [RBX+0xA248], RAX` after `CALL 0x0103A030`), and read by
/// the per-frame animstate driver `FUN_00FCEC90` (module 0xECFC90). High.
/// Runtime note (2026-10-10): live entities carry a 64-bit heap pointer here,
/// dead/transition entities hold a stale 32-bit-looking value;
/// `anim_state_view` rejects those before dereferencing.
inline constexpr std::ptrdiff_t player_anim_state{0xA248};
/// `C_BaseAnimating::m_AnimOverlay` (`CNetworkUtlVectorBase<AnimationLayer>`).
/// Windows: vector at 0x2990, count at 0x299C.
/// Linux: data pointer at 0x3020 (`GetAnimOverlay` `FUN_008B1450` reads
/// `[this+0x3020]`), layer stride 0x48. High.
inline constexpr std::ptrdiff_t anim_overlay_vector{0x3020};
/// Number of animation layers, read right after the vector. Windows: 0x299C.
/// Linux: `GetNumAnimOverlays` `FUN_008B14C0` returns `*(int*)(this+0x3030)`.
/// High.
inline constexpr std::ptrdiff_t anim_overlay_count{0x3030};
/// `C_BasePlayer::m_nTickBase` delta for the local player's own prediction?
/// (unused placeholder)

/// `C_BasePlayer`/`C_Animating` virtual index of
/// `UpdateClientSideAnimation` (Windows 224). Linux: 286 in the primary
/// `C_CSPlayer` vtable at Ghidra 0x22C7B00 (verified: slot 12 = GetAbsOrigin,
/// 208 = IsAlive, 210 = IsPlayer, 286 = module 0xECFC90, the function that
/// calls `AnimState::Update` at slot 1). High.
inline constexpr std::ptrdiff_t update_client_side_animation_vtable{286};

// --- CCSPlayerAnimState (relative to the AnimState object) ----------------
/// Windows layout (OsirisAndExtra SDK/AnimState.h): eyeYaw 0x78, eyePitch
/// 0x7C, footYaw 0x80, footYawLast 0x84, moveYaw 0x88, moveWeight 0x9C,
/// duckAdditional 0xA8, onGround 0x108, walkToRunTransition 0x11C,
/// lastUpdateTime 0x6C, velocityLengthXY 0xEC.
struct anim_state_layout {
    std::ptrdiff_t last_update_time{};
    /// Linux (derived from `CBasePlayerAnimState::Update` FUN_00868370 which
    /// stores its first/second angle args into `state+0x28`/`state+0x2C`,
    /// and `ComputePoseParam_BodyPitch` FUN_008650D0 which reads `+0x2C`).
    /// High.
    std::ptrdiff_t eye_yaw{0x28};
    std::ptrdiff_t eye_pitch{0x2C};
    /// `m_flFootYaw` (current feet yaw): read as the reference yaw by the
    /// move-yaw pose computation `FUN_008661E0` (`-(velocityYaw - footYaw)`).
    /// Runtime note (2026-10-10): a live bot still read 0.0 at +0x84, so this
    /// offset needs in-game confirmation; the resolver sanity-checks the value
    /// (bounded, <=65 deg from the eye yaw) and falls back otherwise.
    std::ptrdiff_t foot_yaw{0x84};
    /// `m_flGoalFeetYaw`-style field initialized from `eyeYaw` on first run by
    /// `SetUpMovement` (`FUN_008648B0`), normalized every tick. Medium.
    std::ptrdiff_t foot_yaw_last{0x34};
    /// Move yaw used by the strafe/sway pose computation. Medium.
    std::ptrdiff_t move_yaw{0x6C};
    std::ptrdiff_t move_weight{};
    std::ptrdiff_t duck_amount{};
    std::ptrdiff_t duck_additional{};
    std::ptrdiff_t on_ground{};
    std::ptrdiff_t walk_to_run_transition{};
    std::ptrdiff_t velocity_length_xy{};
};

inline constexpr anim_state_layout anim_state{};

// --- CClientState (engine module) -----------------------------------------
inline constexpr std::ptrdiff_t client_state_delta_tick{};
inline constexpr std::ptrdiff_t client_state_last_command_ack{};
inline constexpr std::ptrdiff_t client_state_last_outgoing_command{};
inline constexpr std::ptrdiff_t client_state_choked_commands{};
inline constexpr std::ptrdiff_t client_state_net_channel{};
inline constexpr std::ptrdiff_t client_state_paused{};

// --- CGlobalVars extras (Linux base offsets are in game.cpp) --------------
/// `frametime` sits right after `curtime` (Linux 0x14 / 20).
inline constexpr std::ptrdiff_t globals_frametime{20};
/// `curtime` (Linux 0x10).
inline constexpr std::ptrdiff_t globals_current_time{16};
inline constexpr std::ptrdiff_t globals_tick_count{28};
inline constexpr std::ptrdiff_t globals_interval_per_tick{32};
/// `CGlobalVars::realtime` (Linux 0x0).
inline constexpr std::ptrdiff_t globals_real_time{};

// --- CUserCmd button state kept on C_BasePlayer ---------------------------
/// Windows: `m_nButtons` 0x3344 / `m_afButtonForced` 0x3340 (O&E prediction).
inline constexpr std::ptrdiff_t player_buttons{};
inline constexpr std::ptrdiff_t player_button_forced{};
} // namespace zephyrus::csgo::offsets

namespace zephyrus::csgo {
/// True when an offset has been recovered and may be used.
[[nodiscard]] constexpr bool offset_valid(const std::ptrdiff_t value) noexcept {
    return value != 0;
}
} // namespace zephyrus::csgo
