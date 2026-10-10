// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#pragma once

#include <zephyrus/csgo/game.hpp>

#include <string>
#include <vector>

namespace zephyrus::csgo::features {
/// One projected player for the ESP overlay (screen coordinates in pixels).
struct esp_drawable {
    float left{};
    float top{};
    float right{};
    float bottom{};
    float health{1.0F};
    std::string name;
    std::string weapon;
    float distance{};
    bool visible{};
    bool teammate{};
};

/// Player ESP data collection: projects bounding boxes and gathers the
/// labels. Rendering itself happens in the overlay layer (RmlUi elements).
class esp {
  public:
    struct esp_options {
        bool enabled{false};
        bool box{true};
        bool name{true};
        bool health{true};
        bool weapon{true};
        bool distance{true};
        bool snapline{true};
        bool team{false};
        bool visible_only{false};
    };

    [[nodiscard]] esp_options &options() noexcept { return options_; }
    [[nodiscard]] const esp_options &options() const noexcept {
        return options_;
    }

    [[nodiscard]] std::vector<esp_drawable>
    collect(const game &game, int screen_width, int screen_height) const;

  private:
    esp_options options_{};
};
} // namespace zephyrus::csgo::features
