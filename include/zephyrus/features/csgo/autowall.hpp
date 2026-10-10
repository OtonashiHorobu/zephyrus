// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Penetration damage model rewritten from the OsirisAndExtra reference
// (behaviour only; the `UTIL_TraceToExit` logic is reimplemented on top of
// the engine trace interface so no Windows-only function is required).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>

namespace zephyrus::csgo::features {
/// Result of a wallbang scan towards a point.
struct autowall_result {
    float damage{};
    int hitgroup{hit_group::invalid};
    bool hit{};

    [[nodiscard]] explicit operator bool() const noexcept { return hit; }
};

/// Damage estimation through world geometry, modeled on the CS:GO bullet
/// penetration rules (4 surfaces max, per-hitgroup multipliers, armor and
/// distance falloff).
class autowall {
  public:
    /// Scan from the attacker's eyes to `destination`; `target` is the entity
    /// whose damage should be reported (other players stop the scan when
    /// `friendly_fire` is false).
    [[nodiscard]] autowall_result scan(const game &game, const entity &attacker,
                                       const entity &target,
                                       const Vector &destination,
                                       bool friendly_fire) const noexcept;

  private:
    [[nodiscard]] static float
    hitgroup_damage_multiplier(int hitgroup, const weapon_info &weapon,
                               const entity &victim) noexcept;
    static void calculate_armor_damage(float armor_ratio, int armor_value,
                                       bool heavy_armor,
                                       float &damage) noexcept;
    /// Ported `UTIL_TraceToExit`: walk along `direction` from the impact
    /// point until a trace leaves solid geometry, returning the exit point.
    [[nodiscard]] static bool
    trace_to_exit(const game &game, const trace &enter_trace,
                  const Vector &start, const Vector &direction, Vector &exit,
                  trace &exit_trace) noexcept;
    [[nodiscard]] static float handle_bullet_penetration(
        const game &game, const surface_data &enter_surface,
        const trace &enter_trace, const Vector &direction, Vector &start,
        float penetration, float damage) noexcept;
};
} // namespace zephyrus::csgo::features
