// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/game.hpp>
#include <zephyrus/input/keybind.hpp>

#include <chrono>
#include <expected>
#include <string>

namespace zephyrus::features {
/// Third person camera with a smooth first <-> third transition.
///
/// The client switches `camera1` to a follow camera whenever the exported
/// `thirdperson` distance variable is non-zero, so animating that value gives
/// a pull-back / zoom-in animation without touching the render path.
class thirdperson {
  public:
    static constexpr float max_distance{25.0F};

    [[nodiscard]] std::expected<void, std::string>
    initialize(const acr::game &game) noexcept;
    void shutdown() noexcept;

    keybind toggle_key{};

    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

    void set_distance(float distance) noexcept;
    [[nodiscard]] float distance() const noexcept { return target_distance_; }

    /// Advance the animation; call once per frame.
    void update() noexcept;

  private:
    int *thirdperson_{};
    bool enabled_{};
    float target_distance_{10.0F};
    float current_{};
    std::chrono::steady_clock::time_point last_{};
};
} // namespace zephyrus::features
