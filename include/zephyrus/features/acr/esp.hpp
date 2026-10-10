// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/game.hpp>
#include <zephyrus/features/acr/exploits.hpp>
#include <zephyrus/features/acr/skeleton.hpp>
#include <zephyrus/input/keybind.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <vector>

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace zephyrus::features {
enum class esp_box : int {
    none = 0,
    box_2d = 1,
    corners_2d = 2,
    box_3d = 3,
    corners_3d = 4,
};

enum class esp_snapline : int {
    none = 0,
    bottom = 1,
    top = 2,
    crosshair = 3,
};

struct esp_options {
    bool enabled{false};
    esp_box box{esp_box::box_2d};
    bool draw_name{true};
    bool draw_distance{true};
    bool draw_health{true};
    bool draw_weapon{true};
    bool draw_skeleton{true};
    bool glow{true};
    esp_snapline snapline{esp_snapline::none};
    bool visible_only{false};
    bool team_check{true};
    bool show_teammates{false};
    /// Amber outline + "PROT" countdown for spawn protected enemies.
    bool show_spawn_protection{true};
    keybind toggle_key{};
};

/// Shot line data comes from the exploits module's `addshotline` hook.
struct tracer_view {
    const std::deque<shot_tracer> *lines{};
    float window_seconds{1.5F};
};

/// World-space ESP drawn with OpenGL (boxes, glow, skeletons, bars) while all
/// text is rendered by RmlUi as part of the overlay.
class esp {
  public:
    [[nodiscard]] std::expected<void, std::string>
    initialize(const acr::game &game) noexcept;
    void shutdown() noexcept;

    /// Attach the RmlUi document that hosts the text labels.
    void initialize_labels(Rml::ElementDocument &document) noexcept;

    [[nodiscard]] esp_options &options() noexcept { return options_; }
    [[nodiscard]] const esp_options &options() const noexcept {
        return options_;
    }

    /// Draw the GL parts into the current context (inside the overlay guard).
    void render(int width, int height) noexcept;

    /// Refresh the RmlUi label positions/text. Call before `Context::Update`.
    void update_labels() noexcept;

    /// Number of entities drawn by the last `render()` call.
    [[nodiscard]] std::size_t last_drawn() const noexcept {
        return last_drawn_;
    }

    /// Attach the exploit shot lines for tracer rendering.
    void set_tracers(const std::deque<shot_tracer> *lines,
                     float window_seconds = 1.5F) noexcept {
        tracers_.lines = lines;
        tracers_.window_seconds = window_seconds;
    }

  private:
    struct label {
        Rml::Element *root{};
        Rml::Element *name{};
        Rml::Element *meta_root{};
        Rml::Element *meta{};
    };

    void ensure_labels(std::size_t count) noexcept;
    void hide_label(label &entry) noexcept;

    const acr::game *game_{};
    esp_options options_{};
    hook::function_hook<void()> nametag_hook_;
    std::size_t last_drawn_{};
    std::uint32_t last_skeleton_sequence_{};
    skeleton_capture skeletons_;
    tracer_view tracers_{};

    Rml::ElementDocument *label_document_{};
    Rml::Element *label_layer_{};
    std::vector<label> labels_;
};
} // namespace zephyrus::features
