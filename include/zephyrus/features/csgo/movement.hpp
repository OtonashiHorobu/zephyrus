// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>

namespace zephyrus::csgo::features {
/// Movement helpers driven from the `ClientMode::CreateMove` hook.
class movement {
  public:
    struct movement_options {
        /// Keep jump pressed exactly on the landing tick (bunny hop).
        bool bhop{false};
        /// Steer the strafe towards the mouse direction while airborne.
        bool auto_strafe{false};
        /// Cap the movement speed while the slow-walk binding is active.
        bool slow_walk{false};
    };

    [[nodiscard]] movement_options &options() noexcept { return options_; }
    [[nodiscard]] const movement_options &options() const noexcept {
        return options_;
    }

    /// Update the slow-walk binding state (keybind modes resolved by the
    /// caller).
    void set_slow_walk_active(bool active) noexcept {
        slow_walk_active_ = active;
    }

    void create_move(const game &game, user_cmd &cmd) noexcept;

    /// Rotate the movement keys so the world-space wish direction is
    /// unchanged after the view angles were rewritten by anti-aim.
    static void fix_movement(user_cmd &cmd, float original_yaw) noexcept;

  private:
    movement_options options_{};
    bool slow_walk_active_{};
};
} // namespace zephyrus::csgo::features
