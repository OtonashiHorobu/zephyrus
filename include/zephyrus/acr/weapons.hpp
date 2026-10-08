// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <string_view>

namespace zephyrus::acr {
/// `GUN_*` ids from the client's weapon.h, in enum order.
enum gun_type : int {
    gun_knife = 0,
    gun_pistol,
    gun_shotgun,
    gun_subgun,
    gun_sniper,
    gun_assault,
    gun_grenade,
    gun_akimbo,
    gun_bolt,
    gun_heal,
    gun_sword,
    gun_rpg,
    gun_assault2,
    gun_sniper2,
    gun_sniper3,
    gun_pistol2,
    gun_assault_pro,
    gun_shotgun_pro,
    gun_acr_pro,
    gun_count,
};

/// Display names matching the game's own weapon list.
[[nodiscard]] constexpr std::string_view weapon_name(int type) noexcept {
    constexpr std::string_view names[gun_count]{
        "Knife", "USP",   "M1014", "MP5",      "M21",         "M16A3",
        "M67",   "Akimbo", "Intervention", "Heal", "Sword",   "RPG-7",
        "AK-47", "M82",   "MK12",  "M1911",    "M16 Pro",     "M1014 Pro",
        "ACR Pro",
    };
    if (type < 0 || type >= gun_count) {
        return "Unknown";
    }
    return names[type];
}

/// Sniper-style weapons that support the scope (and thus auto scope).
[[nodiscard]] constexpr bool weapon_scoped(int type) noexcept {
    switch (type) {
    case gun_sniper:
    case gun_bolt:
    case gun_sniper2:
    case gun_sniper3:
        return true;
    default:
        return false;
    }
}
} // namespace zephyrus::acr
