// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

namespace zephyrus::acr::protocol {
/// Network message ids (`protocol.h`, PROTOCOL_VERSION 140). Client and
/// server share the same enum; the direction hints come from the game.
enum sv_message : int {
    // general
    sv_servinfo = 0, // before connection is complete
    sv_welcome,
    sv_initclient,
    sv_initai,
    sv_cdis,
    sv_delai,
    sv_reassignai,
    sv_resume,
    sv_mapident, // connection and disconnection
    sv_client,
    sv_pos,
    sv_posc,
    sv_sound,
    sv_pingpong,
    sv_clientping, // automatic from client
    sv_text,
    sv_typing,
    sv_whois,
    sv_switchname,
    sv_switchskin,
    sv_thirdperson,
    sv_level,
    sv_setteam, // user-initiated
    sv_callvote,
    sv_callvoteerr,
    sv_vote,
    sv_votestatus,
    sv_voteresult, // votes
    sv_listdemos,
    sv_getdemo,
    sv_demoplayback, // demos
    sv_auth_acr_req,
    sv_auth_acr_chal, // auth
    sv_claimpriv,
    sv_setpriv, // privileges
    sv_sendmap,
    sv_recvmap,
    sv_removemap, // map transit
    // for editmode ONLY
    sv_editmode,
    sv_edith,
    sv_editt,
    sv_edits,
    sv_editd,
    sv_edite,
    sv_editw,
    sv_editent,
    sv_newmap,
    // game events
    sv_shoot,
    sv_shootc,
    sv_explode,
    sv_akimbo,
    sv_reload, // client to server
    sv_suicide,
    sv_loadout,
    sv_quickswitch,
    sv_weapchange,
    sv_throwknife,
    sv_thrownade, // directly handled
    sv_sg,
    sv_ricochet,
    sv_headshot,
    sv_regen,
    sv_heal,
    sv_bleed,
    sv_streakready,
    sv_streakuse, // server to client
    sv_knifeadd,
    sv_kniferemove, // knives
    sv_confirmadd,
    sv_confirmremove, // kill confirmed
    // gameplay
    sv_points,
    sv_score,
    sv_teamscore,
    sv_discscores,
    sv_kill,
    sv_damage,
    sv_damageobjective, // scoring
    sv_tryspawn,
    sv_spawnstate,
    sv_spawn,
    sv_forcedeath, // spawning
    sv_itemspawn,
    sv_itemacc, // items
    sv_dropflag,
    sv_flaginfo,
    sv_flagmsg,
    sv_flagsecure,
    sv_flagoverload, // flags
    sv_mapchange, // maps
    sv_arenawin,
    sv_zombieswin,
    sv_convertwin, // round end/remaining
    sv_timeup,
    // ???
    sv_teamdeny,
    sv_servermode,
    sv_servmsg,
    sv_extension,
    sv_num,
    sv_connect = sv_num,
};

enum game_mode : int {
    g_demo = 0,
    g_edit,
    g_dm,
    g_ctf,
    g_stf,
    g_htf,
    g_ktf,
    g_bomber,
    g_zombie,
    g_overload,
    g_max,
};

enum mutator_bits : int {
    g_m_none = 0,
    g_m_team = 1 << 0,
    g_m_classic = 1 << 1,
    g_m_confirm = 1 << 2,
    g_m_vampire = 1 << 3,
    g_m_convert = 1 << 4,
    g_m_psychic = 1 << 5,
    g_m_void = 1 << 6,
    g_m_juggernaut = 1 << 7,
    g_m_real = 1 << 8,
    g_m_expert = 1 << 9,
    g_m_insta = 1 << 10,
    g_m_sniping = 1 << 11,
    g_m_pistol = 1 << 12,
    g_m_gib = 1 << 13,
    g_m_explosive = 1 << 14,
    g_m_gsp1 = 1 << 15,
};

enum team_id : int {
    team_cla = 0,
    team_rvsf,
    team_cla_spect,
    team_rvsf_spect,
    team_spect,
    team_num,
};

/// `m_flags()` coverage: the game modes that use flags / objectives.
inline constexpr int flag_modes_mask = (1 << g_ctf) | (1 << g_stf) |
                                       (1 << g_htf) | (1 << g_ktf) |
                                       (1 << g_bomber) | (1 << g_overload);

/// Base spawn protection durations from `entity.h` (`SPAWNPROTECT`).
inline constexpr int spawn_protect_classic = 1000;
inline constexpr int spawn_protect_flags = 1500;
inline constexpr int spawn_protect_team = 2000;
inline constexpr int spawn_protect_default = 1750;

/// `gametype[mode].implied` layout of the game's `gametypes` struct.
struct gametypes_entry {
    int type;
    int implied;
    int mutators[2];
    const char *name;
    const char *gsp[1];
};
} // namespace zephyrus::acr::protocol
