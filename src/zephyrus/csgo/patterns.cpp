// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński. Linux pattern tables only (legacy CS:GO 1.38.8.1 linux64).

#include <zephyrus/csgo/address.hpp>
#include <zephyrus/csgo/patterns.hpp>
#include <zephyrus/log.hpp>

#include <string_view>

namespace zephyrus::csgo {
namespace {
void *find_in(const hook::module_info &module, const std::string_view pattern,
              int &missing) noexcept {
    auto match{hook::find_pattern(module, pattern)};
    if (!match) {
        ++missing;
        ZEPHYRUS_WARN("pattern not found: {}", pattern);
        return nullptr;
    }
    return const_cast<void *>(static_cast<const void *>(*match));
}
} // namespace

patterns::patterns(const hook::module_info &client,
                   const hook::module_info &engine) noexcept {
    auto find_client{[&](const std::string_view pattern) {
        return find_in(client, pattern, missing);
    }};
    auto find_engine{[&](const std::string_view pattern) {
        return find_in(engine, pattern, missing);
    }};

    local_player_slot =
        address{find_client("83 FF FF 48 8B 05")}.add(6).abs().as<void *>();

    view_render_beams = address{find_client("C7 45 ? ? ? ? ? 4C 8D 25 ? ? ? ? "
                                            "49 8B 3C 24")}
                            .add(10)
                            .abs()
                            .deref()
                            .as<void *>();

    glow_object_manager = address{find_client("E8 ? ? ? ? 4C 89 E7 8B 70 20")}
                              .add(1)
                              .abs()
                              .add(12)
                              .abs()
                              .as<void *>();

    weapon_system = address{find_client("48 8B 3D ? ? ? ? 48 8B 4D F8")}
                        .add(3)
                        .abs()
                        .deref()
                        .as<void *>();

    player_resource_slot = address{find_client("48 8B 15 ? ? ? ? 48 63 C3")}
                               .add(3)
                               .abs()
                               .as<void *>();

    planted_c4s = address{find_client("48 8D 3D ? ? ? ? 42 C6 44 28")}
                      .add(3)
                      .abs()
                      .as<void *>();

    game_rules_slot =
        address{find_client("48 8D 1D ? ? ? ? 48 8B 3B 48 85 FF 74 06")}
            .add(3)
            .abs()
            .as<void *>();

    view_render =
        address{find_client("0F 85 ? ? ? ? 48 8D 05 ? ? ? ? 45 89 F8")}
            .add(9)
            .abs()
            .deref()
            .as<void *>();

    // `m_nPredictionRandomSeed`: B3 was not located (the Osiris Linux anchor
    // actually resolves to `m_nPredictionPlayer`, see below). Left null so
    // `EnginePrediction` keeps its safe fallback until the real int global is
    // recovered.
    prediction_random_seed = nullptr;

    // `m_nPredictionPlayer` (`C_BasePlayer*`): the same global that the
    // Osiris Linux `predictionRandomSeed` anchor resolves to. FUN_00ad17b0
    // writes an entity pointer into it; FUN_00ad1a50 clears it. Verified
    // unique.
    prediction_player =
        address{find_client("41 8D 56 FF 31 C9")}.add(-14).abs().as<int *>();

    // `CBasePlayerAnimState::Update` (module 0x768370, anim vtable slot 1):
    // reads cl_showanimstate, sets AnimState eyeYaw/eyePitch and runs the
    // pose-param sub-steps. Verified unique in client_client.so.
    update_anim_state =
        address{find_client("55 48 89 E5 41 57 41 56 41 55 41 54 53 48 89 FB "
                            "48 83 EC 18 4C 8B 25 ?? ?? ?? ?? F3 0F 11 45 C8 "
                            "F3 0F 11 4D C4")}
            .as<void *>();

    move_data = address{find_client("48 8D 0D ? ? ? ? 48 89 DE 4C 89 FF")}
                    .add(3)
                    .abs()
                    .deref()
                    .as<void *>();

    move_helper =
        address{find_client("48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 FF 50 58 41 0F "
                            "B7 F7")}
            .add(3)
            .abs()
            .deref()
            .as<void *>();

    line_goes_through_smoke =
        address{find_client("E8 ? ? ? ? 44 0F B6 4D ? 84 C0")}
            .add(1)
            .abs()
            .as<void *>();

    is_other_enemy = address{find_client("E8 ? ? ? ? 84 C0 75 94")}
                         .add(1)
                         .abs()
                         .as<void *>();

    set_abs_origin =
        address{find_client("E8 ? ? ? ? 4D 63 B5")}.add(1).abs().as<void *>();

    send_clan_tag =
        address{find_engine("E8 ? ? ? ? E9 ? ? ? ? 0F 1F 44 00 00 48 8B 7D "
                            "B0")}
            .add(1)
            .abs()
            .as<void *>();

    // `CNetChan::SendDatagram` (engine module 0x4AA000):
    // `int(CNetChan*, void*)`. The function's profiler scope name is the
    // string "CNetChan::SendDatagram". Verified unique in engine_client.so.
    send_datagram =
        address{find_engine("55 48 89 E5 41 57 41 56 41 55 49 89 FD 41 54 53 "
                            "48 89 F3 48 81 EC A8 01 10 00 64 48 8B 04 25 28 "
                            "00 00 00 48 89 45 C8")}
            .as<void *>();

    ZEPHYRUS_INFO("patterns: resolved ({} missing)", missing);
}
} // namespace zephyrus::csgo
