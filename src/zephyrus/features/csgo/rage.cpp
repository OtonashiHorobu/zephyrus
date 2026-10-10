// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Reimplemented from Osiris (MIT) and OsirisAndExtra (behaviour reference).

#include <zephyrus/features/csgo/animations.hpp>
#include <zephyrus/features/csgo/autowall.hpp>
#include <zephyrus/features/csgo/backtrack.hpp>
#include <zephyrus/features/csgo/hitchance.hpp>
#include <zephyrus/features/csgo/rage.hpp>
#include <zephyrus/features/csgo/resolver.hpp>

#include <algorithm>
#include <cmath>
#include <span>

namespace zephyrus::csgo::features {
namespace {
[[nodiscard]] Vector
calculate_relative_angle(const Vector &source, const Vector &destination,
                         const Vector &view_angles) noexcept {
    return ((destination - source).to_angle() - view_angles)
        .normalized_angles();
}

/// Bone sets per hitbox choice (CS:GO skeleton).
constexpr int bone_head{8};
constexpr int bone_neck{7};
constexpr int bone_chest{6};
constexpr int bone_stomach{5};
constexpr int bone_pelvis{0};
constexpr int bone_best[]{8, 4, 3, 7, 6, 5};
constexpr int bone_all[]{8, 7, 6, 5, 4, 3, 0};

[[nodiscard]] std::span<const int> bones_for_hitbox(const int hitbox) noexcept {
    switch (hitbox) {
    case 1:
        return {&bone_head, 1};
    case 2:
        return {&bone_neck, 1};
    case 3:
        return {&bone_chest, 1};
    case 4:
        return {&bone_stomach, 1};
    case 5:
        return {&bone_pelvis, 1};
    case 6:
    case 7:
        return bone_all;
    default:
        return bone_best;
    }
}

[[nodiscard]] weapon_category category_of(const weapon_type type) noexcept {
    switch (type) {
    case weapon_type::pistol:
    case weapon_type::breach_charge:
    case weapon_type::tablet:
        return weapon_category::pistol;
    case weapon_type::submachinegun:
        return weapon_category::submachinegun;
    case weapon_type::rifle:
        return weapon_category::rifle;
    case weapon_type::shotgun:
        return weapon_category::shotgun;
    case weapon_type::sniper_rifle:
        return weapon_category::sniper;
    case weapon_type::machinegun:
        return weapon_category::machinegun;
    default:
        return weapon_category::other;
    }
}
} // namespace

rage_weapon_profile &
rage::category_profile(const weapon_category category) noexcept {
    const auto index{static_cast<std::size_t>(category)};
    if (index >= categories_.size()) {
        return categories_[static_cast<std::size_t>(weapon_category::other)];
    }
    return categories_[index];
}

const rage_weapon_profile &
rage::category_profile(const weapon_category category) const noexcept {
    const auto index{static_cast<std::size_t>(category)};
    if (index >= categories_.size()) {
        return categories_[static_cast<std::size_t>(weapon_category::other)];
    }
    return categories_[index];
}

const rage_weapon_profile &
rage::profile_for(const entity &weapon) const noexcept {
    if (!options_.per_weapon || !weapon.valid()) {
        return options_.defaults;
    }
    return category_profile(category_of(weapon.type()));
}

void rage::update_shot_tracking(const game &game) noexcept {
    if (!tracked_target_.valid()) {
        return;
    }
    if (tracked_target_.is_dormant() || !tracked_target_.is_alive()) {
        tracked_target_ = {};
        return;
    }
    const int health{tracked_target_.health()};
    if (health < tracked_health_) {
        if (resolver_ != nullptr) {
            resolver_->on_hit(tracked_target_);
        }
        tracked_target_ = {};
        return;
    }
    if (game.current_time() - tracked_time_ > 0.3F) {
        if (resolver_ != nullptr) {
            resolver_->on_miss(tracked_target_);
        }
        tracked_target_ = {};
    }
}

void rage::run(const game &game, user_cmd &cmd) noexcept {
    update_shot_tracking(game);

    if (!options_.enabled || game.client() == nullptr) {
        return;
    }

    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive() || local.is_defusing() ||
        local.wait_for_no_attack()) {
        return;
    }

    const entity weapon{local.active_weapon()};
    if (!weapon.valid() || weapon.clip() <= 0) {
        return;
    }

    const rage_weapon_profile &profile{profile_for(weapon)};
    if (!profile.enabled) {
        return;
    }
    if (local.shots_fired() > 0 && !weapon.is_full_auto()) {
        return;
    }
    if (options_.ignore_flash && local.is_flashed()) {
        return;
    }

    const bool attack_held{(cmd.buttons & user_cmd::in_attack) != 0};
    if (!attack_held && !profile.auto_fire) {
        return;
    }
    if (local.next_attack() > game.server_time(&cmd)) {
        return;
    }

    const Vector eye{local.eye_position()};
    const Vector punch{options_.no_recoil ? local.aim_punch_angle() : Vector{}};

    struct candidate {
        entity target{};
        backtrack_target history{};
        Vector position{};
        float fov{255.0F};
        float distance{};
        int damage{};
        int health{100};
        bool valid{};
    };
    candidate best{};

    const std::span<const int> bones{bones_for_hitbox(profile.hitbox)};
    autowall scanner{};

    for (int index{1}; index <= game.max_clients(); ++index) {
        const entity target{game.get_entity(index)};
        if (!target.valid() || target.is_dormant() ||
            target.raw() == local.raw() || !target.is_alive() ||
            target.gun_game_immunity()) {
            continue;
        }
        if (!game.is_other_enemy(target, local) && !options_.friendly_fire) {
            continue;
        }

        backtrack_target history{};
        if (backtrack_ != nullptr) {
            history = backtrack_->select(game, cmd, target, punch);
        } else {
            history.target = target;
        }
        if (!history.target.valid()) {
            history.target = target;
        }

        const animation_player *animation{};
        if (animations_ != nullptr) {
            animation = &animations_->player(target.index());
        }

        resolver_result resolved{};
        if (resolver_ != nullptr && animation != nullptr) {
            resolved = resolver_->resolve(target, *animation);
        }

        const float target_distance{(history.origin() - eye).length()};

        for (const int bone : bones) {
            Vector position{history.bone(bone)};
            if (position.x == 0.0F && position.y == 0.0F &&
                position.z == 0.0F) {
                continue;
            }
            if (resolved.resolved && animation != nullptr) {
                position =
                    resolver_->apply(history.origin(), position,
                                     history.record.eye_angles.y, resolved.yaw);
            }

            const Vector angle{calculate_relative_angle(
                eye, position, cmd.viewangles + punch)};
            const float fov{std::hypot(angle.x, angle.y)};
            if (fov >= profile.fov) {
                continue;
            }
            if (!options_.ignore_smoke &&
                game.line_goes_through_smoke(eye, position)) {
                continue;
            }

            const bool visible{
                target.is_visible(game.engine_trace(), eye, position)};
            if (!visible && options_.visible_only) {
                continue;
            }
            const float bone_distance{(position - eye).length()};
            int damage{};
            if (!visible) {
                if (!profile.autowall) {
                    continue;
                }
                const autowall_result scan{scanner.scan(
                    game, local, target, position, options_.friendly_fire)};
                if (!scan.hit) {
                    continue;
                }
                damage = static_cast<int>(scan.damage);
                if (damage < profile.min_damage ||
                    (profile.hitbox == 7 && damage < target.health())) {
                    continue;
                }
            } else if (profile.min_damage > 0 && profile.autowall) {
                const autowall_result scan{scanner.scan(
                    game, local, target, position, options_.friendly_fire)};
                damage = scan.hit ? static_cast<int>(scan.damage) : 0;
                if (damage < profile.min_damage && damage < target.health()) {
                    continue;
                }
            } else if (profile.hitbox == 7) {
                const autowall_result scan{scanner.scan(
                    game, local, target, position, options_.friendly_fire)};
                damage = scan.hit ? static_cast<int>(scan.damage) : 0;
                if (damage < target.health()) {
                    continue;
                }
            }

            // Priority ordering.
            bool better{};
            switch (profile.priority) {
            case 1:
                better = target_distance < best.distance;
                break;
            case 2:
                better = target.health() < best.health;
                break;
            case 3:
                better = damage > best.damage;
                break;
            default:
                if (profile.hitbox == 6) {
                    better = bone_distance < best.distance;
                } else {
                    better = fov < best.fov;
                }
                break;
            }
            if (!best.valid || better) {
                best.target = target;
                best.history = history;
                best.position = position;
                best.fov = fov;
                best.distance =
                    profile.hitbox == 6 ? bone_distance : target_distance;
                best.damage = damage;
                best.health = target.health();
                best.valid = true;
            }
            if (profile.hitbox != 0 && profile.hitbox != 7) {
                break;
            }
        }
    }

    if (!best.valid) {
        return;
    }

    // Hit chance gate: sample the spread cone against the bone positions of
    // the record that is actually being aimed at (live pose or backtrack).
    bool can_fire{true};
    if (profile.hitchance > 0) {
        const float chance{estimate_hit_chance(
            game, local, best.history.record, weapon, best.position, 64)};
        can_fire = chance * 100.0F >= static_cast<float>(profile.hitchance);
    }

    // Auto scope: stop firing until scoped in.
    bool scoping{false};
    if (profile.auto_scope && weapon.is_sniper_rifle() && !local.is_scoped() &&
        weapon.next_primary_attack() <= game.server_time(&cmd)) {
        cmd.buttons |= user_cmd::in_attack2;
        can_fire = false;
        scoping = true;
    }

    if (!can_fire) {
        if (profile.autostop && !scoping) {
            cmd.forwardmove = 0.0F;
            cmd.sidemove = 0.0F;
        }
        cmd.buttons &= ~user_cmd::in_attack;
    }

    // Aim.
    Vector angle{
        calculate_relative_angle(eye, best.position, cmd.viewangles + punch)};
    constexpr float max_angle_delta{180.0F};
    bool clamped{false};
    if (std::abs(angle.x) > max_angle_delta ||
        std::abs(angle.y) > max_angle_delta) {
        angle.x = std::clamp(angle.x, -max_angle_delta, max_angle_delta);
        angle.y = std::clamp(angle.y, -max_angle_delta, max_angle_delta);
        clamped = true;
    }

    Vector final_angles{cmd.viewangles + angle};
    final_angles.x = std::clamp(final_angles.x, -89.0F, 89.0F);
    final_angles.y = Vector::normalize_yaw(final_angles.y);

    const bool attack_ready{weapon.next_primary_attack() <=
                            game.server_time(&cmd)};
    const bool fire_shot{!clamped && can_fire && attack_ready &&
                         (attack_held || profile.auto_fire)};

    cmd.viewangles = final_angles;

    if (!profile.silent) {
        using set_view_angles_fn = void (*)(void *, const Vector &);
        auto **engine_vtable{*reinterpret_cast<void ***>(game.engine())};
        reinterpret_cast<set_view_angles_fn>(engine_vtable[19])(game.engine(),
                                                                cmd.viewangles);
    }

    if (fire_shot) {
        if (backtrack_ != nullptr) {
            backtrack_->rewind(game, cmd, best.history);
        }
        if (profile.auto_fire) {
            cmd.buttons |= user_cmd::in_attack;
        }
        if (resolver_ != nullptr) {
            resolver_->on_shot(best.target);
        }
        tracked_target_ = best.target;
        tracked_health_ = best.target.health();
        tracked_time_ = game.current_time();
    } else if (attack_held && !can_fire && profile.autostop) {
        cmd.buttons &= ~user_cmd::in_attack;
    }

    if (clamped) {
        cmd.buttons &= ~user_cmd::in_attack;
    }
}
} // namespace zephyrus::csgo::features
