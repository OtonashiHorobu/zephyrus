// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/features/csgo/fake_lag.hpp>

#include <cstdint>

namespace zephyrus::csgo::features {
namespace {
/// Deterministic per-tick jitter so the send decision does not change between
/// the anti-aim preview and the real `run` in the same command.
[[nodiscard]] std::uint32_t tick_hash(const int tick) noexcept {
    std::uint32_t value{static_cast<std::uint32_t>(tick)};
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}
} // namespace

int fake_lag::effective_limit(const user_cmd &cmd) const noexcept {
    const int limit{options_.limit < 1 ? 1 : options_.limit};
    switch (options_.mode) {
    case 1: { // adaptive
        const int value{limit +
                        static_cast<int>(tick_hash(cmd.tick_count) % 5) - 2};
        return value < 1 ? 1 : value;
    }
    case 2: { // random
        return 1 + static_cast<int>(tick_hash(cmd.tick_count) %
                                    static_cast<std::uint32_t>(limit));
    }
    default:
        return limit;
    }
}

bool fake_lag::preview(const game &game, const user_cmd &cmd,
                       const bool original_send) const noexcept {
    if (!options_.enabled) {
        return original_send;
    }
    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return original_send;
    }
    if (options_.on_shot && (cmd.buttons & user_cmd::in_attack) != 0) {
        return true;
    }
    if (!original_send) {
        return false;
    }
    return choked_ >= effective_limit(cmd);
}

bool fake_lag::run(const game &game, user_cmd &cmd,
                   const bool original_send) noexcept {
    if (!options_.enabled) {
        choked_ = 0;
        return original_send;
    }

    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        choked_ = 0;
        return original_send;
    }

    if (options_.on_shot && (cmd.buttons & user_cmd::in_attack) != 0) {
        choked_ = 0;
        return true;
    }

    if (!original_send) {
        // The engine already wants to choke this command (e.g. burst fire).
        return false;
    }

    if (choked_ >= effective_limit(cmd)) {
        choked_ = 0;
        return true;
    }

    ++choked_;
    return false;
}
} // namespace zephyrus::csgo::features
