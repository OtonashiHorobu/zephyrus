// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <cstddef>

namespace zephyrus::acr::offsets {
/// Field offsets for the ACR 2.18.3 Linux client.
///
/// Generated with `offsetof` against the game's own headers (the tool lives in
/// the development notes; the binary and /datassd/Clone/acr-git/source are the
/// matching pair). Regenerate whenever the client binary changes.

// struct vec { float x, y, z; }
inline constexpr std::size_t vec_x = 0;
inline constexpr std::size_t vec_y = 4;
inline constexpr std::size_t vec_z = 8;

// struct physent (first member is the vptr from its virtual destructor)
inline constexpr std::size_t physent_o = 8;
inline constexpr std::size_t physent_vel = 20;
inline constexpr std::size_t physent_yaw = 68;
inline constexpr std::size_t physent_pitch = 72;
inline constexpr std::size_t physent_roll = 76;
inline constexpr std::size_t physent_pitchvel = 80;
inline constexpr std::size_t physent_yawvel = 84;
inline constexpr std::size_t physent_radius = 96;
inline constexpr std::size_t physent_eyeheight = 100;
inline constexpr std::size_t physent_maxeyeheight = 104;
inline constexpr std::size_t physent_aboveeye = 108;
inline constexpr std::size_t physent_onfloor = 113;
inline constexpr std::size_t physent_crouching = 116;
inline constexpr std::size_t physent_move = 136;
inline constexpr std::size_t physent_strafe = 137;
inline constexpr std::size_t physent_state = 138;
inline constexpr std::size_t physent_type = 139;

// class playerent : public dynent, public playerstate
inline constexpr std::size_t playerent_health = 280;
inline constexpr std::size_t playerent_armour = 284;
inline constexpr std::size_t playerent_playerstate = 272;
inline constexpr std::size_t playerstate_perk2 = 36;
inline constexpr std::size_t playerstate_gunselect = 56;
inline constexpr std::size_t playerstate_scoping = 61;
inline constexpr std::size_t playerent_zoomed = 148;
inline constexpr std::size_t playerent_clientnum = 728;
inline constexpr std::size_t playerent_lastupdate = 732;
inline constexpr std::size_t playerent_ping = 740;
inline constexpr std::size_t playerent_lifesequence = 744;
inline constexpr std::size_t playerent_radarmillis = 764;
inline constexpr std::size_t playerent_nametagmillis = 768;
inline constexpr std::size_t playerent_frags = 772;
inline constexpr std::size_t playerent_deaths = 784;
inline constexpr std::size_t playerent_points = 788;
inline constexpr std::size_t playerent_attacking = 848;
inline constexpr std::size_t playerent_team = 1112;
inline constexpr std::size_t playerent_weaponchanging = 1120;
inline constexpr std::size_t playerent_spectatemode = 1128;
inline constexpr std::size_t playerent_thirdperson = 1132;
inline constexpr std::size_t playerent_weaponsel = 1504;
inline constexpr std::size_t playerent_lastattackweapon = 1520;
inline constexpr std::size_t playerent_head = 1668;
inline constexpr std::size_t playerent_ownernum = 1720;
inline constexpr std::size_t playerent_enemy = 1736;
inline constexpr std::size_t playerent_name = 850; // char[MAXSTRLEN]

// struct weapon (ammo/mag/gunwait are references into playerstate)
inline constexpr std::size_t weapon_type = 8;
inline constexpr std::size_t weapon_owner = 16;
inline constexpr std::size_t weapon_ammo = 32;
inline constexpr std::size_t weapon_mag = 40;

// template <class T> struct vector { T *buf; int alen, ulen; }
inline constexpr std::size_t vector_buf = 0;
inline constexpr std::size_t vector_alen = 8;
inline constexpr std::size_t vector_ulen = 12;

inline constexpr int maxstrlen = 260;
inline constexpr int cs_alive = 0;
inline constexpr int team_cla = 0;
inline constexpr int team_rvsf = 1;
inline constexpr int team_spect = 4;
} // namespace zephyrus::acr::offsets
