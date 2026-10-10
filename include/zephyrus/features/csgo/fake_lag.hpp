// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>

namespace zephyrus::csgo::features {
/// Fake lag: suppress outgoing commands up to a limit so the server sees a
/// stuttering player. Works on the `CreateMove` send-packet flag.
class fake_lag {
  public:
    struct fake_lag_options {
        bool enabled{false};
        /// 0 = static, 1 = adaptive, 2 = random.
        int mode{0};
        int limit{6};
        /// Send immediately when firing.
        bool on_shot{true};
    };

    [[nodiscard]] fake_lag_options &options() noexcept { return options_; }
    [[nodiscard]] const fake_lag_options &options() const noexcept {
        return options_;
    }

    /// Returns the send-packet state for this command.
    bool run(const game &game, user_cmd &cmd, bool original_send) noexcept;

    /// What `run` would return for this command without consuming state (used
    /// by the anti-aim to know whether this tick is a fake or real tick).
    [[nodiscard]] bool preview(const game &game, const user_cmd &cmd,
                               bool original_send) const noexcept;

    [[nodiscard]] int choked() const noexcept { return choked_; }

  private:
    [[nodiscard]] int effective_limit(const user_cmd &cmd) const noexcept;

    fake_lag_options options_{};
    int choked_{};
};
} // namespace zephyrus::csgo::features
