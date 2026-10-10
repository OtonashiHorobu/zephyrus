// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Hit chance estimation: samples the engine's spread distribution (the same
// random model the weapon code uses) against the target's recorded bone
// positions. Rewritten from the OsirisAndExtra reference.

#pragma once

#include <zephyrus/features/csgo/animations.hpp>
#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/usercmd.hpp>

namespace zephyrus::csgo::features {
/// Estimated probability (0..1) that a shot fired at `point` hits one of the
/// target's body hitboxes, given the weapon's current spread/inaccuracy.
/// `record` is the pose that is actually aimed at (live or backtrack).
/// Returns 1.0 when the weapon is perfectly accurate or when the record has no
/// usable bone data (the caller then fires without a gate).
[[nodiscard]] float estimate_hit_chance(const game &game, const entity &local,
                                        const animation_record &record,
                                        const entity &weapon,
                                        const Vector &point,
                                        int samples = 64) noexcept;
} // namespace zephyrus::csgo::features
