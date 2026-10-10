// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace zephyrus::csgo {
/// Degrees → radians for the float math used throughout the SDK/features.
inline constexpr float degrees_to_radians{std::numbers::pi_v<float> / 180.0F};

/// Minimal CS:GO `Vector` (three floats) with the helpers the features use.
struct vector {
    float x{};
    float y{};
    float z{};

    [[nodiscard]] vector operator+(const vector &other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }
    [[nodiscard]] vector operator-(const vector &other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }
    [[nodiscard]] vector operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar};
    }
    [[nodiscard]] vector operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar, z / scalar};
    }
    vector &operator+=(const vector &other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    vector &operator-=(const vector &other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }
    vector &operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }
    vector &operator/=(float scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }
    [[nodiscard]] float length2d() const noexcept {
        return std::sqrt(x * x + y * y);
    }
    [[nodiscard]] float square_length() const noexcept {
        return x * x + y * y + z * z;
    }
    [[nodiscard]] float dot(const vector &other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }
    [[nodiscard]] vector cross(const vector &other) const noexcept {
        return {y * other.z - z * other.y, z * other.x - x * other.z,
                x * other.y - y * other.x};
    }
    [[nodiscard]] vector normalized() const noexcept {
        const float magnitude{length()};
        if (magnitude <= 0.0F) {
            return {};
        }
        return *this / magnitude;
    }

    /// Direction vector of the given pitch/yaw angles (degrees).
    [[nodiscard]] static vector from_angle(const vector &angles) noexcept {
        const float pitch{angles.x * degrees_to_radians};
        const float yaw{angles.y * degrees_to_radians};
        return {-std::cos(pitch) * std::cos(yaw),
                -std::cos(pitch) * std::sin(yaw), -std::sin(pitch)};
    }

    /// Angles (degrees) pointing along this vector.
    [[nodiscard]] vector to_angle() const noexcept {
        return {std::atan2(-z, std::hypot(x, y)) / degrees_to_radians,
                std::atan2(y, x) / degrees_to_radians, 0.0F};
    }

    /// Normalize yaw to (-180, 180].
    [[nodiscard]] static float normalize_yaw(float yaw) noexcept {
        while (yaw > 180.0F) {
            yaw -= 360.0F;
        }
        while (yaw < -180.0F) {
            yaw += 360.0F;
        }
        return yaw;
    }

    /// Clamp pitch to +-89 degrees and wrap yaw (angle triples).
    [[nodiscard]] vector normalized_angles() const noexcept {
        return {std::clamp(x, -89.0F, 89.0F), normalize_yaw(y), z};
    }

    /// Source `AngleVectors` basis (pitch/yaw in degrees, yaw counter
    /// clockwise from +X, positive pitch looks down).
    static void angle_vectors(const vector &angles, vector &forward,
                              vector &right, vector &up) noexcept {
        const float sr{std::sin(angles.z * degrees_to_radians)};
        const float sp{std::sin(angles.x * degrees_to_radians)};
        const float sy{std::sin(angles.y * degrees_to_radians)};
        const float cr{std::cos(angles.z * degrees_to_radians)};
        const float cp{std::cos(angles.x * degrees_to_radians)};
        const float cy{std::cos(angles.y * degrees_to_radians)};

        forward = {cp * cy, cp * sy, -sp};
        right = {-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp};
        up = {cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp};
    }

    /// `AngleVectors` forward only.
    [[nodiscard]] static vector
    forward_from_angle(const vector &angles) noexcept {
        vector forward;
        vector right;
        vector up;
        angle_vectors(angles, forward, right, up);
        return forward;
    }

    /// Shortest difference between two yaw angles (degrees).
    [[nodiscard]] static float angle_diff(float a, float b) noexcept {
        return normalize_yaw(a - b);
    }
};

/// Source-engine spelling used throughout the ported SDK layers.
using Vector = vector;
} // namespace zephyrus::csgo
