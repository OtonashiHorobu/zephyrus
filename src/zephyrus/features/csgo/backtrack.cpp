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

/// Default client interpolation (`cl_interp` 0.031) used for the tick
/// rewind. The cvar lookup and the network-channel latency check that Osiris
/// uses were removed: their vtable slots could not be verified against this
/// build, and the record age is already bounded by `time_limit`.
constexpr float default_interp{0.031F};

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
        const float score{score_of(record)};
        if (score < best_score) {
            best_score = score;
            best_record = &record;
        }
    }

    if (best_record != nullptr) {
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
    cmd.tick_count = time_to_ticks(game, target.simulation_time()) +
                     time_to_ticks(game, default_interp);
}
} // namespace zephyrus::csgo::features
