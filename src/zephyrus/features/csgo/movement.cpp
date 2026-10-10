// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/features/csgo/movement.hpp>

#include <cmath>

namespace zephyrus::csgo::features {
void movement::fix_movement(user_cmd &cmd, const float original_yaw) noexcept {
    const float delta_degrees{
        Vector::normalize_yaw(cmd.viewangles.y - original_yaw)};
    if (delta_degrees == 0.0F) {
        return;
    }
    const float delta{delta_degrees * degrees_to_radians};
    const float cosine{std::cos(delta)};
    const float sine{std::sin(delta)};
    const float forward{cmd.forwardmove};
    const float side{cmd.sidemove};
    cmd.forwardmove = cosine * forward - sine * side;
    cmd.sidemove = cosine * side + sine * forward;
}

void movement::create_move(const game &game, user_cmd &cmd) noexcept {
    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return;
    }

    const bool on_ground{local.is_on_ground()};

    // CS:GO only jumps on a fresh press: hold the bit while grounded so the
    // landing tick always carries a press, and release it in the air.
    if (options_.bhop && (cmd.buttons & user_cmd::in_jump) != 0) {
        if (on_ground) {
            cmd.buttons |= user_cmd::in_jump;
        } else {
            cmd.buttons &= ~user_cmd::in_jump;
        }
    }

    if (options_.auto_strafe && !on_ground) {
        if (cmd.mousedx < 0) {
            cmd.sidemove = -450.0F;
        } else if (cmd.mousedx > 0) {
            cmd.sidemove = 450.0F;
        }
    }

    if (options_.slow_walk && slow_walk_active_) {
        // Walk speed modifier (0.52) times the duck modifier for the usual
        // "slow walk" feel.
        cmd.forwardmove *= 0.52F;
        cmd.sidemove *= 0.52F;
    }
}
} // namespace zephyrus::csgo::features
