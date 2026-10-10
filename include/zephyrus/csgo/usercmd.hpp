// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <zephyrus/csgo/math.hpp>

namespace zephyrus::csgo {
/// `CUserCmd` (engine side). Layout is shared between Windows and Linux; the
/// hook layer receives one per tick in `ClientMode::CreateMove`.
struct user_cmd {
    enum button : int {
        in_attack = 1 << 0,
        in_jump = 1 << 1,
        in_duck = 1 << 2,
        in_forward = 1 << 3,
        in_back = 1 << 4,
        in_use = 1 << 5,
        in_cancel = 1 << 6,
        in_left = 1 << 7,
        in_right = 1 << 8,
        in_moveleft = 1 << 9,
        in_moveright = 1 << 10,
        in_attack2 = 1 << 11,
        in_run = 1 << 12,
        in_reload = 1 << 13,
        in_alt1 = 1 << 14,
        in_alt2 = 1 << 15,
        in_score = 1 << 16,
        in_speed = 1 << 17,
        in_walk = 1 << 18,
        in_zoom = 1 << 19,
        in_weapon1 = 1 << 20,
        in_weapon2 = 1 << 21,
        in_bullrush = 1 << 22,
        in_grenade1 = 1 << 23,
        in_grenade2 = 1 << 24,
        in_attack3 = 1 << 25,
    };

    void *vmt{};
    int command_number{};
    int tick_count{};
    vector viewangles{};
    vector aimdirection{};
    float forwardmove{};
    float sidemove{};
    float upmove{};
    int buttons{};
    char impulse{};
    int weaponselect{};
    int weaponsubtype{};
    int random_seed{};
    short mousedx{};
    short mousedy{};
    bool has_been_predicted{};
    vector viewangles_backup{};
    int buttons_backup{};
};
static_assert(sizeof(user_cmd) == 0x60, "user_cmd layout changed");
} // namespace zephyrus::csgo
