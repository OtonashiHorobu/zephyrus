// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/features/csgo/animations.hpp>

#include <array>

namespace zephyrus::csgo::features {
/// Result of one resolution step: the yaw the server body is suspected to
/// have, and how the decision was made.
struct resolver_result {
    float yaw{};
    /// 0 = none, 1 = moving, 2 = foot yaw (AnimState), 3 = lower body yaw,
    /// 4 = brute force.
    int mode{};
    bool resolved{};
};

/// Resolver v2: guesses the server-side body yaw of desynced players from the
/// animation state (foot yaw vs eye angles), the move weight and the
/// lower-body yaw target, with a brute-force fallback keyed on missed shots.
class resolver {
  public:
    struct resolver_options {
        bool enabled{false};
        /// 0 = auto (detectors), 1 = brute force.
        int mode{0};
        /// Brute force start side: 0 = both, 1 = right, 2 = left.
        int side{0};
        /// Prefer the AnimState foot yaw when available.
        bool use_foot_yaw{true};
    };

    struct player_state {
        float resolved_yaw{};
        int misses{};
        int shots{};
    };

    [[nodiscard]] resolver_options &options() noexcept { return options_; }
    [[nodiscard]] const resolver_options &options() const noexcept {
        return options_;
    }

    /// Resolve the body yaw for a target using its live animation data.
    [[nodiscard]] resolver_result
    resolve(const entity &target,
            const animation_player &animation) const noexcept;

    /// Rotate `bone_position` around `origin` so it matches the resolved yaw
    /// instead of the fake eye yaw.
    [[nodiscard]] Vector apply(const Vector &origin,
                               const Vector &bone_position, float eye_yaw,
                               float resolved_yaw) const noexcept;

    /// Note a fired shot at `target` (drives the brute force cycle).
    void on_shot(const entity &target) noexcept;
    /// Note a hit (resets the brute-force state for the target).
    void on_hit(const entity &target) noexcept;
    /// Note that the last shot at `target` did not deal damage.
    void on_miss(const entity &target) noexcept;
    void reset() noexcept;

  private:
    resolver_options options_{};
    mutable std::array<player_state, 65> states_{};
};
} // namespace zephyrus::csgo::features
