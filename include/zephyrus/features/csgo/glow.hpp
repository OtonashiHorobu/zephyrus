// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>

namespace zephyrus::csgo::features {
/// Glow ESP: recolours the engine glow objects the game already assigns to
/// players, so enemies are outlined through walls.
class glow {
  public:
    struct glow_options {
        bool enabled{false};
        bool health_based{false};
        bool rainbow{false};
        /// Engine glow style index.
        int style{0};
    };

    [[nodiscard]] glow_options &options() noexcept { return options_; }
    [[nodiscard]] const glow_options &options() const noexcept {
        return options_;
    }

    /// Called from `FrameStageNotify` at render start.
    void update(const game &game) noexcept;

  private:
    glow_options options_{};
};
} // namespace zephyrus::csgo::features
