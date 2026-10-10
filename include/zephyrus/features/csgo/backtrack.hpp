// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Reimplemented from Osiris (MIT).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>
#include <zephyrus/features/csgo/animations.hpp>

#include <optional>

namespace zephyrus::csgo::features {
/// A shot target: the live pose or a historical record.
struct backtrack_target {
    entity target{};
    animation_record record{};
    /// False when `record` mirrors the live pose.
    bool historical{};

    [[nodiscard]] Vector bone(int index) const noexcept {
        return record.bone(index);
    }
    [[nodiscard]] const Vector &origin() const noexcept {
        return record.origin;
    }
    [[nodiscard]] float simulation_time() const noexcept {
        return record.simulation_time;
    }
};

/// Backtrack: keeps a short history per player (through `animations`) and on
/// shot rewinds the best record and the command tick so the server accepts the
/// shot at the older position.
class backtrack {
  public:
    struct backtrack_options {
        bool enabled{false};
        bool ignore_smoke{false};
        /// Record lifetime in milliseconds.
        int time_limit{200};
    };

    [[nodiscard]] backtrack_options &options() noexcept { return options_; }
    [[nodiscard]] const backtrack_options &options() const noexcept {
        return options_;
    }

    void set_animations(animations *value) noexcept { animations_ = value; }

    /// Pick the record of `target` that is closest to the current view while
    /// still inside the lag compensation window. Returns the live pose when
    /// backtrack is disabled or no valid record exists.
    [[nodiscard]] backtrack_target select(const game &game, const user_cmd &cmd,
                                          const entity &target,
                                          const Vector &punch) const noexcept;

    /// Rewind the entity and the command tick for the chosen record.
    void rewind(const game &game, user_cmd &cmd,
                const backtrack_target &target) const noexcept;
    void reset() noexcept;

  private:
    [[nodiscard]] float get_lerp(const game &game) const noexcept;
    [[nodiscard]] bool valid(const game &game, float simtime) const noexcept;

    backtrack_options options_{};
    animations *animations_{};
};
} // namespace zephyrus::csgo::features
