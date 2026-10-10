// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/features/csgo/anti_aim.hpp>

#include <algorithm>
#include <cmath>

namespace zephyrus::csgo::features {
bool anti_aim::auto_direction_sign(const game &game,
                                   const Vector &angles) noexcept {
    constexpr float range{8192.0F};
    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return false;
    }

    Vector flat{angles.x, angles.y, 0.0F};
    Vector left_angles{flat};
    Vector right_angles{flat};
    left_angles.y += 45.0F;
    right_angles.y -= 45.0F;

    const Vector start{local.eye_position()};
    const Vector left_end{start +
                          Vector::forward_from_angle(left_angles) * range};
    const Vector right_end{start +
                           Vector::forward_from_angle(right_angles) * range};

    const trace left_trace{
        game.trace_ray(start, left_end, 0x4600400B, local.pod())};
    const trace right_trace{
        game.trace_ray(start, right_end, 0x4600400B, local.pod())};

    const float left_distance{(right_trace.endpos - start).length()};
    const float right_distance{(left_trace.endpos - start).length()};
    return left_distance > right_distance;
}

void anti_aim::run(const game &game, user_cmd &cmd, const bool send,
                   const Vector &original_angles) noexcept {
    if (!options_.enabled) {
        return;
    }

    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return;
    }

    // The ragebot aimed this command: keep the shot angle untouched (pitch,
    // yaw and desync would otherwise rotate the bullet away).
    if (cmd.viewangles.y != original_angles.y) {
        last_yaw_ = cmd.viewangles.y;
        return;
    }

    if (options_.disable_on_shot &&
        (cmd.buttons & (user_cmd::in_attack | user_cmd::in_attack2)) != 0) {
        last_yaw_ = cmd.viewangles.y;
        return;
    }

    if (cmd.viewangles.x == original_angles.x) {
        switch (options_.pitch_mode) {
        case 1:
            cmd.viewangles.x = 89.0F;
            break;
        case 2:
            cmd.viewangles.x = -89.0F;
            break;
        case 3:
            cmd.viewangles.x = 0.0F;
            break;
        case 4:
            cmd.viewangles.x = send ? cmd.viewangles.x : 89.0F;
            break;
        case 5:
            cmd.viewangles.x = send ? cmd.viewangles.x : -89.0F;
            break;
        default:
            break;
        }
    }

    const float base_yaw{cmd.viewangles.y};
    float yaw{};
    const bool manual{manual_left_ || manual_right_ || manual_back_ ||
                      manual_forward_};

    if (manual) {
        if (manual_back_) {
            yaw = 180.0F;
            if (manual_left_) {
                yaw -= 45.0F;
            } else if (manual_right_) {
                yaw += 45.0F;
            }
        } else if (manual_left_) {
            yaw = 90.0F;
            if (manual_forward_) {
                yaw -= 45.0F;
            }
        } else if (manual_right_) {
            yaw = -90.0F;
            if (manual_forward_) {
                yaw += 45.0F;
            }
        } else if (manual_forward_) {
            yaw = 0.0F;
        }
    } else {
        switch (options_.yaw_mode) {
        case 1:
            yaw = 180.0F;
            break;
        case 2:
            yaw = -90.0F;
            break;
        case 3:
            yaw = 90.0F;
            break;
        case 4: { // jitter
            if (send) {
                jitter_state_ = !jitter_state_;
            }
            yaw = 180.0F + (jitter_state_ ? options_.jitter_range
                                          : -options_.jitter_range);
            break;
        }
        case 5: // spin
            spin_base_ += options_.spin_speed;
            spin_base_ = Vector::normalize_yaw(spin_base_);
            yaw = spin_base_ - base_yaw;
            break;
        case 6: // static
            yaw = 0.0F;
            break;
        case 7: { // at targets
            float best_fov{255.0F};
            float yaw_angle{};
            for (int index{1}; index <= game.max_clients(); ++index) {
                const entity target{game.get_entity(index)};
                if (!target.valid() || target.raw() == local.raw() ||
                    target.is_dormant() || !target.is_alive() ||
                    !game.is_other_enemy(target, local) ||
                    target.gun_game_immunity()) {
                    continue;
                }
                const Vector angle{
                    ((target.abs_origin() - local.eye_position()).to_angle() -
                     cmd.viewangles)
                        .normalized_angles()};
                const float fov{std::hypot(angle.x, angle.y)};
                if (fov < best_fov) {
                    best_fov = fov;
                    yaw_angle = angle.y;
                }
            }
            yaw = yaw_angle;
            break;
        }
        default:
            break;
        }
        if (options_.yaw_mode != 7) {
            yaw += options_.yaw_add;
        }
    }

    // LBY breaker: flick the body yaw on the tick the server is about to
    // update its lower body yaw (0.22s after movement, else every 1.1s).
    const float speed{local.velocity().length2d()};
    const float vertical_speed{std::abs(local.velocity().z)};
    const float now{game.server_time(&cmd)};
    if (options_.lby_mode != 0) {
        bool flick{};
        if (!local.is_on_ground() || speed > 0.1F || vertical_speed > 100.0F) {
            lby_timer_ = now + 0.22F;
        } else if (now > lby_timer_) {
            lby_timer_ = now + 1.1F;
            lby_flip_ = !lby_flip_;
            flick = true;
        }
        if (flick) {
            yaw = lby_flip_ ? 119.0F : -119.0F;
        }
    }

    float final_yaw{Vector::normalize_yaw(base_yaw + yaw)};

    if (options_.desync) {
        bool invert{desync_flip_};
        switch (options_.desync_mode) {
        case 1:
            if (send) {
                desync_flip_ = !desync_flip_;
            }
            invert = desync_flip_;
            break;
        case 2:
            invert = lby_flip_;
            break;
        case 3:
            invert = auto_direction_sign(game, cmd.viewangles);
            break;
        default:
            if (options_.auto_direction) {
                invert = auto_direction_sign(game, cmd.viewangles);
            }
            break;
        }
        const float magnitude{options_.desync_angle};
        float offset{};
        if (options_.desync_side == 1) {
            offset = magnitude;
        } else if (options_.desync_side == 2) {
            offset = -magnitude;
        } else {
            offset = invert ? magnitude : -magnitude;
        }
        // Real yaw on sent commands, fake yaw while choking.
        if (!send) {
            final_yaw = Vector::normalize_yaw(final_yaw + offset);
        }
    }

    cmd.viewangles.y = final_yaw;
    last_yaw_ = final_yaw;
    if (animations_ != nullptr) {
        animations_->set_fake_angles(cmd.viewangles.x, final_yaw);
    }
}
} // namespace zephyrus::csgo::features
