// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>
#include <zephyrus/features/csgo/animations.hpp>

namespace zephyrus::csgo::features {
/// Anti-aim: silent pitch/yaw manipulation written after the engine has
/// computed movement, so the server sees desynced angles while the local
/// camera stays where the player looks. Includes LBY management, manual
/// directions, auto direction and choke-based desync.
class anti_aim {
  public:
    struct anti_aim_options {
        bool enabled{false};
        /// 0 = off, 1 = down, 2 = up, 3 = zero, 4 = fake down, 5 = fake up.
        int pitch_mode{0};
        /// 0 = off, 1 = backward, 2 = right, 3 = left, 4 = jitter,
        /// 5 = spin, 6 = static, 7 = at targets.
        int yaw_mode{0};
        float yaw_add{};
        float spin_speed{30.0F};
        float jitter_range{45.0F};
        /// Desync (choke-based body yaw split).
        bool desync{false};
        /// 0 = static, 1 = jitter, 2 = sway, 3 = auto direction.
        int desync_mode{0};
        float desync_angle{58.0F};
        /// 0 = auto, 1 = left, 2 = right.
        int desync_side{0};
        /// 0 = off, 1 = opposite flick, 2 = sway on LBY update.
        int lby_mode{0};
        /// Rotate to the least visible side automatically.
        bool auto_direction{false};
        /// Keep neutral angles while shooting.
        bool disable_on_shot{false};
        /// Jitter/side randomization seed (0 = no randomization).
        int random_seed{};
    };

    [[nodiscard]] anti_aim_options &options() noexcept { return options_; }
    [[nodiscard]] const anti_aim_options &options() const noexcept {
        return options_;
    }

    void set_animations(animations *value) noexcept { animations_ = value; }

    /// `send` is the send-packet decision for this command (from fake lag);
    /// `original_angles` is the view the player had before the ragebot ran.
    /// Like OsirisAndExtra, the anti-aim leaves a command that the ragebot
    /// aimed untouched so the shot keeps its real angle.
    void run(const game &game, user_cmd &cmd, bool send,
             const Vector &original_angles) noexcept;

    /// Update the manual direction key states (polled input).
    void set_manual_keys(bool left, bool right, bool back,
                         bool forward) noexcept {
        manual_left_ = left;
        manual_right_ = right;
        manual_back_ = back;
        manual_forward_ = forward;
    }

    /// The body yaw the anti-aim chose for the current command (for the
    /// local fake animation).
    [[nodiscard]] float last_yaw() const noexcept { return last_yaw_; }

  private:
    [[nodiscard]] bool auto_direction_sign(const game &game,
                                           const Vector &angles) noexcept;

    anti_aim_options options_{};
    animations *animations_{};
    float spin_base_{};
    float last_yaw_{};
    bool jitter_state_{};
    bool desync_flip_{};
    bool lby_flip_{};
    float lby_timer_{};
    bool manual_left_{};
    bool manual_right_{};
    bool manual_back_{};
    bool manual_forward_{};
};
} // namespace zephyrus::csgo::features
