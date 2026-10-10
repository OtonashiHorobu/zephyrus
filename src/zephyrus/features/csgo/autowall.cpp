// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Penetration damage model rewritten from the OsirisAndExtra reference
// (behaviour only; the `UTIL_TraceToExit` logic is reimplemented on top of
// the engine trace interface so no Windows-only function is required).

#include <zephyrus/features/csgo/autowall.hpp>

#include <cmath>

namespace zephyrus::csgo::features {
namespace {
constexpr unsigned int mask_shot{0x4600400BU};
constexpr unsigned int mask_shot_hull{0x600400BU};
constexpr int max_penetrations{4};
} // namespace

float autowall::hitgroup_damage_multiplier(const int hitgroup,
                                           const weapon_info &weapon,
                                           const entity &victim) noexcept {
    float multiplier{1.0F};
    switch (hitgroup) {
    case hit_group::head:
        multiplier = weapon.headshot_multiplier;
        break;
    case hit_group::stomach:
        multiplier = 1.25F;
        break;
    case hit_group::left_leg:
    case hit_group::right_leg:
        multiplier = 0.75F;
        break;
    default:
        break;
    }
    if (victim.has_heavy_armor() && hitgroup != hit_group::head) {
        multiplier *= 0.5F;
    }
    return multiplier;
}

void autowall::calculate_armor_damage(const float armor_ratio,
                                      const int armor_value,
                                      const bool heavy_armor,
                                      float &damage) noexcept {
    float scale{1.0F};
    float bonus_ratio{0.5F};
    float ratio{armor_ratio};
    if (heavy_armor) {
        ratio *= 0.2F;
        bonus_ratio = 0.33F;
        scale = 0.25F;
    }

    float new_damage{damage * ratio};
    const float estiminated_damage{(damage - damage * ratio) * scale *
                                   bonus_ratio};
    if (estiminated_damage > static_cast<float>(armor_value)) {
        new_damage = damage - static_cast<float>(armor_value) / bonus_ratio;
    }
    damage = new_damage;
}

bool autowall::trace_to_exit(const game &game, const trace &enter_trace,
                             const Vector &start, const Vector &direction,
                             Vector &exit, trace &exit_trace) noexcept {
    constexpr float range{90.0F};
    constexpr float step{4.0F};

    float distance{};
    while (distance <= range) {
        distance += step;
        const Vector origin{start + direction * distance};
        const int current_contents{game.point_contents(origin, mask_shot)};

        // Exited into empty space (or crossed a water/glass boundary): trace
        // back towards the impact to find the exact exit plane.
        if ((current_contents & 0x600400B) == 0) {
            const Vector destination{origin - direction * step};
            trace_filter_skip filter_enter{enter_trace.entity};
            exit_trace =
                game.trace_ray(origin, destination, mask_shot, filter_enter);

            if (exit_trace.start_solid &&
                (exit_trace.surface.flags & 0x8000U) != 0) {
                // The impact surface is see-through (grate/fence): the
                // bullet can pass straight on.
                trace_filter filter_all{nullptr};
                exit_trace =
                    game.trace_ray(origin, start, mask_shot_hull, filter_all);
                if (exit_trace.fraction < 1.0F && !exit_trace.start_solid) {
                    exit = exit_trace.endpos;
                    return true;
                }
                continue;
            }

            if (exit_trace.fraction < 1.0F && !exit_trace.start_solid) {
                exit = exit_trace.endpos;
                return true;
            }
        }
    }
    return false;
}

float autowall::handle_bullet_penetration(
    const game &game, const surface_data &enter_surface,
    const trace &enter_trace, const Vector &direction, Vector &start,
    const float penetration, float damage) noexcept {
    Vector exit{};
    trace exit_trace{};
    if (!trace_to_exit(game, enter_trace, enter_trace.endpos, direction, exit,
                       exit_trace)) {
        return -1.0F;
    }

    const surface_data *exit_surface{
        game.surface_props(exit_trace.surface.surface_props)};
    const float exit_penetration_modifier{
        exit_surface != nullptr ? exit_surface->penetration_modifier : 1.0F};

    float damage_modifier{0.16F};
    float penetration_modifier{
        (enter_surface.penetration_modifier + exit_penetration_modifier) /
        2.0F};

    if (enter_surface.material == 71 || enter_surface.material == 89) {
        damage_modifier = 0.05F;
        penetration_modifier = 3.0F;
    } else if (((enter_trace.contents >> 3) & 1) != 0 ||
               ((enter_trace.surface.flags >> 7) & 1) != 0) {
        penetration_modifier = 1.0F;
    }

    if (exit_surface != nullptr &&
        enter_surface.material == exit_surface->material) {
        if (exit_surface->material == 85 || exit_surface->material == 87) {
            penetration_modifier = 3.0F;
        } else if (exit_surface->material == 76) {
            penetration_modifier = 2.0F;
        }
    }

    damage -= 11.25F / penetration / penetration_modifier +
              damage * damage_modifier +
              (exit_trace.endpos - enter_trace.endpos).square_length() / 24.0F /
                  penetration_modifier;

    start = exit;
    return damage;
}

autowall_result autowall::scan(const game &game, const entity &attacker,
                               const entity &target, const Vector &destination,
                               const bool friendly_fire) const noexcept {
    autowall_result result{};
    if (!attacker.valid() || !target.valid()) {
        return result;
    }

    const entity weapon{attacker.active_weapon()};
    const weapon_info *weapon_data{weapon.valid() ? weapon.weapon_data()
                                                  : nullptr};
    if (weapon_data == nullptr || weapon_data->damage <= 0) {
        return result;
    }

    float damage{static_cast<float>(weapon_data->damage)};
    Vector start{attacker.eye_position()};
    Vector direction{destination - start};
    const float max_distance{direction.length()};
    if (max_distance <= 1.0F) {
        return result;
    }
    direction /= max_distance;
    float current_distance{};

    int hits_left{max_penetrations};
    while (damage >= 1.0F && hits_left > 0) {
        const trace hit{
            game.trace_ray(start, destination, mask_shot, attacker.pod())};

        if (!friendly_fire && hit.entity != nullptr) {
            const entity hit_entity{hit.entity};
            if (hit_entity.valid() && hit_entity.is_player() &&
                !game.is_other_enemy(hit_entity, attacker)) {
                return result;
            }
        }

        if (hit.fraction == 1.0F) {
            break;
        }

        current_distance += hit.fraction * (max_distance - current_distance);
        damage *=
            std::pow(weapon_data->range_modifier, current_distance / 500.0F);

        if (hit.entity == target.pod() && hit.hitgroup > hit_group::generic &&
            hit.hitgroup <= hit_group::right_leg) {
            damage *=
                hitgroup_damage_multiplier(hit.hitgroup, *weapon_data, target);
            if (target.is_hitgroup_armored(hit.hitgroup)) {
                calculate_armor_damage(weapon_data->armor_ratio / 2.0F,
                                       target.armor(), target.has_heavy_armor(),
                                       damage);
            }
            result.damage = damage;
            result.hitgroup = hit.hitgroup;
            result.hit = damage >= 1.0F;
            return result;
        }

        const surface_data *surface{
            game.surface_props(hit.surface.surface_props)};
        if (surface == nullptr || surface->penetration_modifier < 0.1F) {
            break;
        }

        const float penetrated{
            handle_bullet_penetration(game, *surface, hit, direction, start,
                                      weapon_data->penetration, damage)};
        if (penetrated < 0.0F) {
            break;
        }
        damage = penetrated;
        --hits_left;
    }
    return result;
}
} // namespace zephyrus::csgo::features
