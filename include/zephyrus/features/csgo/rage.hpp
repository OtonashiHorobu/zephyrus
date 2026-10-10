// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Reimplemented from Osiris (MIT) and OsirisAndExtra (behaviour reference).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>

#include <array>
#include <cstdint>

namespace zephyrus::csgo::features {
class resolver;
class backtrack;
class animations;

/// Per-weapon ragebot settings. Weapons without an explicit override derive
/// their profile from their category (pistol/SMG/rifle/...).
struct rage_weapon_profile {
    bool enabled{true};
    /// 0 = best, 1 = head, 2 = neck, 3 = chest, 4 = stomach, 5 = pelvis,
    /// 6 = nearest, 7 = lethal.
    int hitbox{0};
    int min_damage{0};
    /// Minimum estimated hit chance (percent) required to fire.
    int hitchance{60};
    float fov{5.0F};
    bool auto_fire{false};
    bool auto_scope{false};
    bool autostop{false};
    bool silent{true};
    /// Allow wallbangs (autowall damage model).
    bool autowall{true};
    /// 0 = FOV, 1 = distance, 2 = health, 3 = damage.
    int priority{0};
};

/// Weapon categories used for profile defaults.
enum class weapon_category : int {
    pistol = 0,
    submachinegun,
    rifle,
    shotgun,
    sniper,
    machinegun,
    other,
    count,
};

/// Ragebot: target selection, silent aim, autofire, autoscope, no-recoil,
/// hit chance, autowall, per-category configuration.
class rage {
  public:
    struct rage_options {
        bool enabled{false};
        bool friendly_fire{false};
        bool ignore_smoke{false};
        bool ignore_flash{false};
        bool no_recoil{true};
        /// Only aim at targets the engine can see (disables wallbangs even
        /// with autowall on).
        bool visible_only{false};
        /// When enabled, use the per-category profile.
        bool per_weapon{true};
        /// Mirror of the default profile for the menu/TOML (kept in sync with
        /// the category profiles).
        rage_weapon_profile defaults{};
    };

    [[nodiscard]] rage_options &options() noexcept { return options_; }
    [[nodiscard]] const rage_options &options() const noexcept {
        return options_;
    }

    /// Per-category profiles (`weapon_category` index).
    [[nodiscard]] rage_weapon_profile &
    category_profile(weapon_category category) noexcept;
    [[nodiscard]] const rage_weapon_profile &
    category_profile(weapon_category category) const noexcept;
    [[nodiscard]] const rage_weapon_profile &
    profile_for(const entity &weapon) const noexcept;

    void run(const game &game, user_cmd &cmd) noexcept;

    /// Optional resolver: corrects bone positions for desynced enemies.
    void set_resolver(resolver *value) noexcept { resolver_ = value; }
    /// Optional backtrack: picks historical records for the shot.
    void set_backtrack(backtrack *value) noexcept { backtrack_ = value; }
    /// Optional animation cache: live bone/pose data.
    void set_animations(animations *value) noexcept { animations_ = value; }

  private:
    /// Confirm hits/misses of the last fired shot from health deltas (there is
    /// no cheap game-event path in the client hooks yet).
    void update_shot_tracking(const game &game) noexcept;

    rage_options options_{};
    std::array<rage_weapon_profile,
               static_cast<std::size_t>(weapon_category::count)>
        categories_{};
    resolver *resolver_{};
    backtrack *backtrack_{};
    animations *animations_{};

    entity tracked_target_{};
    int tracked_health_{};
    float tracked_time_{};
};
} // namespace zephyrus::csgo::features
