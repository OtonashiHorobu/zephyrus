// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Animation framework: per-player records (bones, pose parameters, animation
// layers, simulation time) plus the local player's real/fake matrices used by
// anti-aim and the resolver. Behaviour modeled after OsirisAndExtra
// (rewritten).

#include <zephyrus/features/csgo/animations.hpp>
#include <zephyrus/log.hpp>

#include <cstring>

namespace zephyrus::csgo::features {
namespace {
constexpr int frame_stage_render_start{5};
} // namespace

const animation_player &animations::player(const int index) const noexcept {
    if (index <= 0 || index >= static_cast<int>(players_.size())) {
        return players_[0];
    }
    return players_[static_cast<std::size_t>(index)];
}

void animations::reset() noexcept {
    for (auto &entry : players_) {
        entry = {};
    }
    local_index_ = 0;
    has_fake_matrix_ = false;
    has_real_matrix_ = false;
    fake_requested_ = false;
    updating_fake_ = false;
}

bool animations::capture(const game &game, const int index,
                         animation_player &out) const noexcept {
    const entity target{game.get_entity(index)};
    if (!target.valid()) {
        return false;
    }

    out.has_matrix =
        target.setup_bones(out.matrix, 256, 256, game.current_time());

    if (const float *pose{target.pose_parameters()}; pose != nullptr) {
        std::memcpy(out.pose.data(), pose, sizeof(float) * out.pose.size());
        out.has_pose = true;
    } else {
        out.has_pose = false;
    }

    out.layer_count = 0;
    if (const animation_layer *layers{target.animation_layers()};
        layers != nullptr) {
        const int count{target.animation_layer_count()};
        out.layer_count = count < 0
                              ? 0
                              : (count > static_cast<int>(out.layers.size())
                                     ? static_cast<int>(out.layers.size())
                                     : count);
        std::memcpy(out.layers.data(), layers,
                    sizeof(animation_layer) *
                        static_cast<std::size_t>(out.layer_count));
    }

    out.simulation_time = target.simulation_time();
    out.origin = target.abs_origin();
    out.eye_angles = target.eye_angles();
    out.velocity = target.velocity();
    out.flags = target.flags();
    out.duck_amount = target.duck_amount();

    auto &history{out.history};
    if (!out.has_matrix) {
        // A failed bone setup leaves the previous frame's matrices in place;
        // recording them under the new simulation time would poison backtrack.
        return true;
    }
    if (history.empty() ||
        history.front().simulation_time != out.simulation_time) {
        animation_record record{};
        record.simulation_time = out.simulation_time;
        record.tick = game.tick_count();
        record.origin = out.origin;
        record.eye_angles = out.eye_angles;
        record.velocity = out.velocity;
        record.flags = out.flags;
        record.duck_amount = out.duck_amount;
        for (std::size_t i{}; i < animation_record::record_bones.size(); ++i) {
            record.bones[i] =
                out.matrix[animation_record::record_bones[i]].origin();
        }
        history.push_front(record);
        while (history.size() > max_history) {
            history.pop_back();
        }
    }
    return true;
}

void animations::update(const game &game, const int frame_stage) noexcept {
    if (frame_stage != frame_stage_render_start) {
        return;
    }

    const entity local{game.local_player()};
    local_index_ = local.valid() ? local.index() : 0;

    for (int index{1}; index <= game.max_clients(); ++index) {
        const entity target{game.get_entity(index)};
        if (!target.valid() || target.is_dormant() || !target.is_alive()) {
            auto &entry{players_[static_cast<std::size_t>(index)]};
            entry.has_matrix = false;
            entry.history.clear();
            continue;
        }
        (void)capture(game, index, players_[static_cast<std::size_t>(index)]);
    }
}

void animations::update_local_animation() noexcept {
    // Invoked from the `UpdateClientSideAnimation` hook for the local player.
    // The matrix capture only needs the render bone cache, which is available
    // on every build; the fake pose needs the AnimState/abs-angle offsets and
    // is skipped until they were recovered.
    const animation_player &self{local()};
    if (!self.has_matrix) {
        return;
    }
    std::memcpy(real_matrix_.data(), self.matrix, sizeof(real_matrix_));
    has_real_matrix_ = true;

    if (!updating_fake_ && !fake_requested_) {
        return;
    }
    // Fake matrix generation lands here once `csgo/offsets.hpp` carries the
    // AnimState layout: force the foot yaw, rebuild the bones, restore.
    fake_requested_ = false;
}
} // namespace zephyrus::csgo::features
