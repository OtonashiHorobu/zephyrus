// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński. Linux pattern tables only (legacy CS:GO 1.38.8.1 linux64).

#pragma once

#include <zephyrus/hook/hook.hpp>

#include <cstdint>

namespace zephyrus::csgo {
/// Runtime-resolved addresses for the client and engine modules.
///
/// Every field is null when the pattern did not match (the total count is
/// reported through `missing`). Chain resolution follows Osiris' Linux
/// finders: `abs()` consumes a rip-relative disp32 at the current position.
struct patterns {
    patterns(const hook::module_info &client,
             const hook::module_info &engine) noexcept;

    // --- client module ---
    void *local_player_slot{};    // C_CSPlayer** (`EntityPOD**`)
    void *view_render_beams{};    // ViewRenderBeamsPOD*
    void *glow_object_manager{};  // GlowObjectManager*
    void *weapon_system{};        // WeaponSystemPOD*
    void *player_resource_slot{}; // PlayerResource**
    void *planted_c4s{};          // UtlVector<PlantedC4POD*>*
    void *game_rules_slot{};      // EntityPOD** (proxy)
    void *view_render{};          // ViewRender*
    void *move_data{};            // MoveData*
    void *move_helper{};          // MoveHelperPOD*

    // --- client module functions ---
    void *line_goes_through_smoke{};      // bool(Vector, Vector, short)
    void *is_other_enemy{};               // bool(EntityPOD*, EntityPOD*)
    void *set_abs_origin{};               // void(EntityPOD*, const Vector&)
    void *set_abs_angle{};                // void(EntityPOD*, const Vector&)
    void *update_client_side_animation{}; // void(EntityPOD*)
    void *create_anim_state{};            // void(AnimState*, EntityPOD*)
    void *update_anim_state{};            // void(AnimState*, const Vector&)
    void *restore_entity_to_predicted_frame{}; // void(int, int)
    void *trace_to_exit{}; // bool(trace&, Vector&, Vector&, float, int)

    // --- client module globals ---
    int *prediction_player{};      // `m_nPredictionPlayer`
    int *prediction_random_seed{}; // `m_nPredictionRandomSeed`

    // --- engine module ---
    void *send_clan_tag{}; // void(const char*, const char*)
    void *client_state{};  // CClientState*
    void *send_datagram{}; // int(CNetChan*, void*)

    int missing{};
};
} // namespace zephyrus::csgo
