// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Animation framework: per-player records (bones, pose parameters, animation
// layers, simulation time) plus the local player's real/fake matrices used by
// anti-aim and the resolver. Behaviour modeled after OsirisAndExtra
// (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>

#include <array>
#include <deque>

namespace zephyrus::csgo::features {
/// Compact historical snapshot of one player (used by backtrack and the
/// resolver). Full bone matrices are kept only for the live pose.
struct animation_record {
    float simulation_time{-1.0F};
    int tick{};
    Vector origin{};
    Vector eye_angles{};
    Vector velocity{};
    int flags{};
    float duck_amount{};

    /// Body bones captured in the order of `record_bones`.
    static constexpr std::array<int, 6> record_bones{8, 7, 6, 4, 3, 0};
    std::array<Vector, record_bones.size()> bones{};

    [[nodiscard]] const Vector &bone(const int index) const noexcept {
        for (std::size_t i{}; i < record_bones.size(); ++i) {
            if (record_bones[i] == index) {
                return bones[i];
            }
        }
        return bones[0];
    }
};

/// One player's live animation data.
struct animation_player {
    matrix3x4 matrix[256]{};
    bool has_matrix{};
    std::array<float, 24> pose{};
    bool has_pose{};
    std::array<animation_layer, 13> layers{};
    int layer_count{};
    float simulation_time{-1.0F};
    Vector origin{};
    Vector eye_angles{};
    Vector velocity{};
    int flags{};
    float duck_amount{};
    std::deque<animation_record> history;
};

/// Per-player animation cache, refreshed once per frame at render start.
class animations {
  public:
    static constexpr std::size_t max_history{48};

    animations() noexcept = default;

    /// Record every player from `Client::FrameStageNotify`.
    void update(const game &game, int frame_stage) noexcept;
    void reset() noexcept;

    [[nodiscard]] const animation_player &player(int index) const noexcept;
    [[nodiscard]] const animation_player &local() const noexcept {
        return players_[local_index_];
    }
    [[nodiscard]] int local_index() const noexcept { return local_index_; }

  private:
    [[nodiscard]] bool capture(const game &game, int index,
                               animation_player &out) const noexcept;

    std::array<animation_player, 65> players_{};
    int local_index_{};
};
} // namespace zephyrus::csgo::features
