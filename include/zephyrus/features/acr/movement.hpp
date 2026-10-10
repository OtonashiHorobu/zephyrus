// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/game.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/input/keybind.hpp>

#include <expected>
#include <string>

namespace zephyrus::features {
/// Movement enhancements, all applied inside the game's physics substep so the
/// result is collision-checked and frame-rate independent (the engine runs the
/// inner `moveplayer` at 200 Hz).
struct movement_options {
    bool speedhack{false};
    float speedhack_multiplier{1.5F};
    bool bhop{true};
    /// When set, auto jump only while the jump key is held; otherwise every
    /// landing jumps.
    bool bhop_hold_jump{false};
    /// Instant air control: WASD changes the horizontal velocity direction
    /// immediately without moving the view.
    bool air_control{true};
    /// Release all keys while airborne -> kill horizontal velocity at once.
    bool air_control_stop{false};
    /// Never lose speed: anything below the ground run speed is raised to it
    /// while steering in the air.
    bool air_control_boost{false};
};

class movement {
  public:
    [[nodiscard]] std::expected<void, std::string>
    initialize(const acr::game &game) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] movement_options &options() noexcept { return options_; }
    [[nodiscard]] const movement_options &options() const noexcept {
        return options_;
    }

    /// Master switch (Off / Always / Hold / Toggle).
    keybind toggle_key{};

    /// Jump key state, fed from the overlay's key routing.
    void set_jump_held(bool held) noexcept { jump_held_ = held; }

  private:
    void on_step(void *physent, int moveres, bool local,
                 int curtime) noexcept;

    const acr::game *game_{};
    hook::function_hook<void(void *, int, bool, int)> step_hook_{};
    movement_options options_{};
    bool jump_held_{};
};
} // namespace zephyrus::features
