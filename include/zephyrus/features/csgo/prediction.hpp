// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Engine prediction: runs the game's own movement code on the modified user
// command so features see the resulting velocity/flags, then restores the
// entity to the predicted frame (the later engine prediction must be the only
// one that sticks). Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>

namespace zephyrus::csgo::features {
class prediction {
  public:
    /// True when the interfaces and the restore routine are available.
    [[nodiscard]] static bool available(const game &game) noexcept;

    /// `IClientPrediction::Update` from the network state (run once per
    /// CreateMove before `run`).
    void update(const game &game) noexcept;

    /// Simulate the command and remember the resulting velocity/flags.
    void run(const game &game, user_cmd &cmd) noexcept;

    /// Restore globals and the entity frame. Safe to call when `run` was not
    /// executed.
    void restore(const game &game) noexcept;

    [[nodiscard]] Vector velocity() const noexcept { return velocity_; }
    [[nodiscard]] int flags() const noexcept { return flags_; }
    [[nodiscard]] bool active() const noexcept { return running_; }

  private:
    bool running_{};
    Vector velocity_{};
    int flags_{};
    float old_current_time_{};
    float old_frametime_{};
};
} // namespace zephyrus::csgo::features
