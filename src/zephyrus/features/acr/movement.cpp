// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/features/acr/movement.hpp>

#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/acr/symbols.hpp>
#include <zephyrus/log.hpp>

#include <cmath>

namespace zephyrus::features {
std::expected<void, std::string>
movement::initialize(const acr::game &game) noexcept {
    game_ = &game;

    auto step{hook::function_hook<void(void *, int, bool, int)>::install(
        acr::symbols::moveplayer_step,
        [this](void *physent, int moveres, bool local, int curtime) {
            on_step(physent, moveres, local, curtime);
        })};
    if (!step) {
        return std::unexpected{"failed to hook moveplayer(step): " +
                               step.error()};
    }
    step_hook_ = std::move(*step);

    ZEPHYRUS_INFO("movement hooks installed");
    return {};
}

void movement::shutdown() noexcept { step_hook_ = {}; }

void movement::on_step(void *physent, int moveres, bool local,
                       int curtime) noexcept {
    const bool active{toggle_key.active() && game_ != nullptr &&
                      game_->valid() && local && physent != nullptr &&
                      physent == game_->local_player().base()};
    if (!active) {
        step_hook_.call_original(physent, moveres, local, curtime);
        return;
    }

    acr::player me{physent};
    const float saved_maxspeed{me.maxspeed()};

    if (options_.speedhack && options_.speedhack_multiplier > 1.0F) {
        me.set_maxspeed(saved_maxspeed * options_.speedhack_multiplier);
    }

    if (options_.bhop && me.onfloor() &&
        (!options_.bhop_hold_jump || jump_held_)) {
        me.set_jumpnext(true);
    }

    if (options_.air_control && !me.onfloor()) {
        const char move{me.move_input()};
        const char strafe{me.strafe_input()};
        acr::vec3 velocity{me.velocity()};
        if (move != 0 || strafe != 0) {
            acr::vec3 direction{};
            game_->vec_from_yaw_pitch(me.yaw(), move, strafe, direction);
            const float length{
                std::sqrt(direction.x * direction.x +
                          direction.y * direction.y)};
            if (length > 1.0e-4F) {
                float speed{
                    std::sqrt(velocity.x * velocity.x +
                              velocity.y * velocity.y)};
                if (options_.air_control_boost && speed < 0.7F) {
                    speed = 0.7F;
                }
                if (speed > 1.0e-4F) {
                    me.set_velocity({direction.x / length * speed,
                                     direction.y / length * speed,
                                     velocity.z});
                }
            }
        } else if (options_.air_control_stop) {
            me.set_velocity({0.0F, 0.0F, velocity.z});
        }
    }

    step_hook_.call_original(physent, moveres, local, curtime);

    if (options_.speedhack && options_.speedhack_multiplier > 1.0F) {
        me.set_maxspeed(saved_maxspeed);
    }

    static std::size_t probe{};
    if ((++probe % 2000) == 0) {
        const acr::vec3 velocity{me.velocity()};
        const acr::vec3 origin{me.origin()};
        ZEPHYRUS_TRACE(
            "movement: floor={} move={} strafe={} jump={} vel=({:.2f},{:.2f}) "
            "hspd={:.2f} o=({:.1f},{:.1f},{:.1f})",
            me.onfloor(), static_cast<int>(me.move_input()),
            static_cast<int>(me.strafe_input()), me.jumpnext(),
            static_cast<double>(velocity.x), static_cast<double>(velocity.y),
            static_cast<double>(std::sqrt(velocity.x * velocity.x +
                                          velocity.y * velocity.y)),
            static_cast<double>(origin.x), static_cast<double>(origin.y),
            static_cast<double>(origin.z));
    }
}
} // namespace zephyrus::features
