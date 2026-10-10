// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/csgo/offsets.hpp>
#include <zephyrus/features/csgo/resolver.hpp>

#include <cmath>

namespace zephyrus::csgo::features {
namespace {
constexpr float moving_speed{25.0F};

[[nodiscard]] float brute_offset(const int side, const int misses) noexcept {
    constexpr float offsets[]{0.0F,  58.0F,  -58.0F, 29.0F,  -29.0F,
                              87.0F, -87.0F, 116.0F, -116.0F};
    float offset{offsets[misses % std::size(offsets)]};
    if (side == 1 && offset < 0.0F) {
        offset = -offset;
    } else if (side == 2 && offset > 0.0F) {
        offset = -offset;
    }
    return offset;
}
} // namespace

resolver_result
resolver::resolve(const entity &target,
                  const animation_player &animation) const noexcept {
    resolver_result result{};
    if (!options_.enabled || !target.valid()) {
        return result;
    }

    const float eye_yaw{animation.eye_angles.y};
    const Vector velocity{animation.velocity};
    const float speed{velocity.length2d()};

    if (speed > moving_speed) {
        // While moving, the body follows the movement direction.
        result.yaw = velocity.to_angle().y;
        result.mode = 1;
        result.resolved = true;
        return result;
    }

    if (options_.mode != 1 && options_.use_foot_yaw &&
        offset_valid(offsets::anim_state.foot_yaw)) {
        const anim_state_view state{target.anim_state_ptr()};
        if (state.valid()) {
            const float foot_yaw{state.foot_yaw()};
            // Sanity: a real feet yaw is bounded and within the desync limit
            // of the eye yaw. A garbage value (wrong offset) falls through to
            // the LBY/brute-force detectors instead of mis-rotating hitboxes.
            const float delta{std::abs(Vector::angle_diff(foot_yaw, eye_yaw))};
            if (foot_yaw >= -180.0F && foot_yaw <= 180.0F && delta <= 65.0F &&
                delta > 1.0F) {
                result.yaw = foot_yaw;
                result.mode = 2;
                result.resolved = true;
                return result;
            }
        }
    }

    if (options_.mode != 1) {
        const float lower_body_yaw{target.lower_body_yaw()};
        if (std::abs(Vector::angle_diff(lower_body_yaw, eye_yaw)) > 1.0F) {
            result.yaw = lower_body_yaw;
            result.mode = 3;
            result.resolved = true;
            return result;
        }
    }

    const int index{target.index()};
    if (index > 0 && index < static_cast<int>(states_.size())) {
        const player_state &state{states_[index]};
        // Brute force: explicitly selected, or as the fallback after a miss
        // that none of the detectors could explain.
        if (options_.mode == 1 ||
            (options_.mode == 0 && state.misses > 0)) {
            result.yaw =
                eye_yaw + brute_offset(options_.side, state.misses);
            result.mode = 4;
            result.resolved = true;
        }
    }
    return result;
}

Vector resolver::apply(const Vector &origin, const Vector &bone_position,
                       const float eye_yaw,
                       const float resolved_yaw) const noexcept {
    if (!options_.enabled) {
        return bone_position;
    }
    const float delta{Vector::normalize_yaw(resolved_yaw - eye_yaw)};
    if (std::abs(delta) < 1.0F) {
        return bone_position;
    }

    const float radians{delta * degrees_to_radians};
    const float cosine{std::cos(radians)};
    const float sine{std::sin(radians)};
    const float dx{bone_position.x - origin.x};
    const float dy{bone_position.y - origin.y};
    return {origin.x + dx * cosine - dy * sine,
            origin.y + dx * sine + dy * cosine, bone_position.z};
}

void resolver::on_shot(const entity &target) noexcept {
    if (!target.valid()) {
        return;
    }
    const int index{target.index()};
    if (index > 0 && index < static_cast<int>(states_.size())) {
        ++states_[index].shots;
    }
}

void resolver::on_miss(const entity &target) noexcept {
    if (!target.valid()) {
        return;
    }
    const int index{target.index()};
    if (index > 0 && index < static_cast<int>(states_.size())) {
        ++states_[index].misses;
    }
}

void resolver::on_hit(const entity &target) noexcept {
    if (!target.valid()) {
        return;
    }
    const int index{target.index()};
    if (index > 0 && index < static_cast<int>(states_.size())) {
        states_[index].shots = 0;
        states_[index].misses = 0;
    }
}

void resolver::reset() noexcept {
    for (auto &state : states_) {
        state = {};
    }
}
} // namespace zephyrus::csgo::features
