// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Hit chance estimation: samples the engine's spread distribution (the same
// random model the weapon code uses) against the recorded bone positions of
// the pose being aimed at. Rewritten from the OsirisAndExtra reference.

#include <zephyrus/features/csgo/hitchance.hpp>

#include <array>
#include <cmath>
#include <iterator>
#include <numbers>
#include <random>

namespace zephyrus::csgo::features {
namespace {
/// Body spheres used for the simulation: {bone, radius}. The exact hitbox
/// capsules live in the studio model; these approximations cover head, chest,
/// spine and pelvis and are only used to score the spread cone.
struct body_sphere {
    int bone;
    float radius;
};
constexpr body_sphere body_spheres[]{
    {8, 6.5F}, // head
    {7, 5.5F}, // neck
    {6, 8.5F}, // upper chest
    {4, 9.0F}, // chest
    {3, 9.0F}, // stomach
    {0, 9.0F}, // pelvis
};

/// Distance from `point` to the ray `start + t * direction` (t >= 0).
[[nodiscard]] float ray_point_distance(const Vector &start,
                                       const Vector &direction,
                                       const Vector &point) noexcept {
    const float t{std::max(0.0F, (point - start).dot(direction))};
    return (point - (start + direction * t)).length();
}

[[nodiscard]] bool zero(const Vector &value) noexcept {
    return value.x == 0.0F && value.y == 0.0F && value.z == 0.0F;
}
} // namespace

float estimate_hit_chance(const game &game, const entity &local,
                          const animation_record &record, const entity &weapon,
                          const Vector &point, const int samples) noexcept {
    const float spread{weapon.spread()};
    const float inaccuracy{weapon.inaccuracy()};
    if (spread <= 0.0F && inaccuracy <= 0.0F) {
        return 1.0F;
    }
    if (!local.valid() || samples <= 0) {
        return 1.0F;
    }

    // Collect the sphere centers once. Without usable bone data the caller
    // fires ungated, exactly like the old `setup_bones` fallback.
    struct sphere {
        Vector center;
        float radius;
    };
    std::array<sphere, std::size(body_spheres)> spheres{};
    std::size_t sphere_count{};
    for (const body_sphere &entry : body_spheres) {
        const Vector center{record.bone(entry.bone)};
        if (zero(center)) {
            continue;
        }
        spheres[sphere_count++] = {center, entry.radius};
    }
    if (sphere_count == 0) {
        return 1.0F;
    }

    const Vector eye{local.eye_position()};
    Vector forward{point - eye};
    const float distance{forward.length()};
    if (distance <= 1.0F) {
        return 1.0F;
    }
    forward /= distance;

    const Vector view_angles{forward.to_angle()};
    Vector direction;
    Vector right;
    Vector up;
    Vector::angle_vectors(view_angles, direction, right, up);

    // Deterministic per-record sampling; the weapon RNG is separate.
    std::mt19937 generator{static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(local.raw()) ^
        static_cast<std::uint32_t>(game.tick_count() * 2654435761U))};
    std::uniform_real_distribution<float> unit{0.0F, 1.0F};
    std::uniform_real_distribution<float> angle{
        0.0F, 2.0F * std::numbers::pi_v<float>};

    int hits{};
    for (int sample{}; sample < samples; ++sample) {
        const float spread_angle_x{angle(generator)};
        const float spread_angle_y{angle(generator)};
        const float inaccuracy_scale{unit(generator)};
        const float spread_scale{unit(generator)};

        const float lateral_x{std::cos(spread_angle_x) * inaccuracy *
                                  inaccuracy_scale +
                              std::cos(spread_angle_y) * spread * spread_scale};
        const float lateral_y{std::sin(spread_angle_x) * inaccuracy *
                                  inaccuracy_scale +
                              std::sin(spread_angle_y) * spread * spread_scale};

        Vector shot{direction + right * lateral_x + up * lateral_y};
        shot = shot.normalized();

        for (std::size_t i{}; i < sphere_count; ++i) {
            if (ray_point_distance(eye, shot, spheres[i].center) <=
                spheres[i].radius) {
                ++hits;
                break;
            }
        }
    }
    return static_cast<float>(hits) / static_cast<float>(samples);
}
} // namespace zephyrus::csgo::features
