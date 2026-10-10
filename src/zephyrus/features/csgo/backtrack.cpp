// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Reimplemented from Osiris (MIT).

#include <zephyrus/features/csgo/animations.hpp>
#include <zephyrus/features/csgo/backtrack.hpp>

#include <algorithm>
#include <cmath>

namespace zephyrus::csgo::features {
namespace {
[[nodiscard]] int time_to_ticks(const game &game, const float time) noexcept {
    return static_cast<int>(0.5F + time / game.interval_per_tick());
}

[[nodiscard]] float cvar_float(const game &game, const char *name,
                               const float fallback) noexcept {
    if (game.cvar() == nullptr) {
        return fallback;
    }
    using find_var_fn = void *(*)(void *, const char *);
    auto **cvar_vtable{*reinterpret_cast<void ***>(game.cvar())};
    void *variable{
        reinterpret_cast<find_var_fn>(cvar_vtable[15])(game.cvar(), name)};
    if (variable == nullptr) {
        return fallback;
    }
    using get_float_fn = float (*)(void *);
    auto **variable_vtable{*reinterpret_cast<void ***>(variable)};
    return reinterpret_cast<get_float_fn>(variable_vtable[15])(variable);
}

[[nodiscard]] animation_record
live_record(const animation_player &animation) noexcept {
    animation_record record{};
    record.simulation_time = animation.simulation_time;
    record.origin = animation.origin;
    record.eye_angles = animation.eye_angles;
    record.velocity = animation.velocity;
    record.flags = animation.flags;
    record.duck_amount = animation.duck_amount;
    if (animation.has_matrix) {
        for (std::size_t i{}; i < animation_record::record_bones.size(); ++i) {
            record.bones[i] =
                animation.matrix[animation_record::record_bones[i]].origin();
        }
    }
    return record;
}
} // namespace

void backtrack::reset() noexcept { animations_ = nullptr; }

float backtrack::get_lerp(const game &game) const noexcept {
    const float interp{cvar_float(game, "cl_interp", 0.031F)};
    const float ratio{cvar_float(game, "cl_interp_ratio", 2.0F)};
    const float min_ratio{cvar_float(game, "sv_client_min_interp_ratio", 1.0F)};
    const float max_ratio{cvar_float(game, "sv_client_max_interp_ratio", 6.0F)};
    const float max_update{cvar_float(game, "sv_maxupdaterate", 128.0F)};
    const float update{cvar_float(game, "cl_updaterate", 128.0F)};
    const float clamped_ratio{std::clamp(ratio, min_ratio, max_ratio)};
    const float rate{max_update > 0.0F ? max_update : update};
    return std::max(interp, clamped_ratio / (rate > 0.0F ? rate : 128.0F));
}

bool backtrack::valid(const game &game, const float simtime) const noexcept {
    if (game.engine() == nullptr) {
        return false;
    }
    using get_network_fn = void *(*)(void *);
    auto **engine_vtable{*reinterpret_cast<void ***>(game.engine())};
    void *network{
        reinterpret_cast<get_network_fn>(engine_vtable[78])(game.engine())};
    if (network == nullptr) {
        return false;
    }
    using get_latency_fn = float (*)(void *, int);
    auto **network_vtable{*reinterpret_cast<void ***>(network)};
    const auto latency_0{
        reinterpret_cast<get_latency_fn>(network_vtable[9])(network, 0)};
    const auto latency_1{
        reinterpret_cast<get_latency_fn>(network_vtable[9])(network, 1)};
    const float max_unlag{cvar_float(game, "sv_maxunlag", 1.0F)};
    const float delta{
        std::clamp(latency_0 + latency_1 + get_lerp(game), 0.0F, max_unlag) -
        (game.server_time() - simtime)};
    return std::abs(delta) <= 0.2F;
}

backtrack_target backtrack::select(const game &game, const user_cmd &cmd,
                                   const entity &target,
                                   const Vector &punch) const noexcept {
    backtrack_target result{};
    result.target = target;
    if (!target.valid() || animations_ == nullptr) {
        return result;
    }

    const int index{target.index()};
    const animation_player &animation{animations_->player(index)};
    result.record = live_record(animation);

    if (!options_.enabled || animation.history.size() <= 1) {
        return result;
    }

    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return result;
    }
    const Vector eye{local.eye_position()};

    const Vector view{cmd.viewangles + punch};
    const auto score_of = [&](const animation_record &record) {
        // Aim at the best visible point of the head/chest, like the live aim.
        float best{255.0F};
        for (const int bone : {8, 6, 4}) {
            const Vector position{record.bone(bone)};
            if (position.x == 0.0F && position.y == 0.0F &&
                position.z == 0.0F) {
                continue;
            }
            const Vector angle{
                ((position - eye).to_angle() - view).normalized_angles()};
            best = std::min(best, std::hypot(angle.x, angle.y));
        }
        return best;
    };

    const animation_record *best_record{};
    float best_score{score_of(result.record)};
    const float max_age{static_cast<float>(options_.time_limit) / 1000.0F};
    const float server_time{game.server_time()};

    for (const animation_record &record : animation.history) {
        if (server_time - record.simulation_time > max_age) {
            break; // deque is newest-first: older entries only get worse
        }
        if (!valid(game, record.simulation_time)) {
            continue;
        }
        const float score{score_of(record)};
        if (score < best_score) {
            best_score = score;
            best_record = &record;
        }
    }

    if (best_record != nullptr) {
        // Smoke is checked once for the winning record instead of for every
        // history entry (a trace per record per player would be too costly).
        if (!options_.ignore_smoke &&
            game.line_goes_through_smoke(eye, best_record->origin)) {
            return result;
        }
        result.record = *best_record;
        result.historical = true;
    }
    return result;
}

void backtrack::rewind(const game &game, user_cmd &cmd,
                       const backtrack_target &target) const noexcept {
    if (!target.historical || !target.target.valid()) {
        return;
    }

    game.set_abs_origin(target.target, target.origin());
    target.target.set_simulation_time(target.simulation_time());
    cmd.tick_count =
        time_to_ticks(game, target.simulation_time() + get_lerp(game));
}
} // namespace zephyrus::csgo::features
