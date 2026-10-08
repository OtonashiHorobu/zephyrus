// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/entity.hpp>
#include <zephyrus/features/esp.hpp>
#include <zephyrus/features/rage.hpp>
#include <zephyrus/features/thirdperson.hpp>
#include <zephyrus/input/keybind.hpp>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace zephyrus::acr {
class game;
}
namespace Rml {
class ElementDocument;
}

namespace zephyrus::overlay {
/// Controller for the in-game menu document: binds DOM controls to feature
/// options, drives tab/sidebar navigation and keeps dynamic readouts fresh.
class menu final : public Rml::EventListener {
  public:
    menu() noexcept = default;
    ~menu() noexcept override;

    menu(const menu &) = delete;
    menu(menu &&) = delete;
    menu &operator=(const menu &) = delete;
    menu &operator=(menu &&) = delete;

    void initialize(Rml::ElementDocument &document, const acr::game *game,
                    features::esp *esp, features::rage *rage,
                    features::thirdperson *thirdperson, keybind *menu_key,
                    std::function<void()> spawn_bot) noexcept;
    void shutdown() noexcept;

    /// Update dynamic readouts; throttled internally to avoid relayout churn.
    void sync(int fps, bool overlay_visible) noexcept;

    /// Re-read feature options into the controls (after hotkey changes).
    void refresh() noexcept;

    /// True while the next key press should be captured as a new bind.
    [[nodiscard]] bool capturing() const noexcept {
        return listening_ != nullptr;
    }
    void capture_key(int code) noexcept;

    /// Latest cursor position in window pixels, fed by the overlay so drag
    /// math does not depend on event-relative coordinates.
    void set_cursor(int x, int y) noexcept {
        cursor_x_ = x;
        cursor_y_ = y;
    }

    /// Window size in pixels, fed by the overlay before every update. Do not
    /// read it from the document: RmlUi reports layout sizes in pixels while
    /// the root document element is not guaranteed to cover the viewport.
    void set_viewport(int width, int height) noexcept {
        viewport_width_ = width;
        viewport_height_ = height;
    }

    void ProcessEvent(Rml::Event &event) noexcept override;

  private:
    struct slider_binding {
        Rml::Element *slider{};
        Rml::Element *fill{};
        Rml::Element *knob{};
        Rml::Element *value_label{};
        float min{};
        float max{};
        int decimals{};
        std::string suffix;
        std::function<void(float)> apply;
    };

    struct combo_binding {
        Rml::Element *combo{};
        Rml::Element *popup{};
        Rml::Element *text{};
        std::vector<Rml::Element *> items;
        std::vector<std::string> labels;
        std::function<void(int)> apply;
        int *value{};
    };

    /// Osiris style key binder: left click rebinds, right click selects the
    /// mode (Off / Always / Hold / Toggle), Unset clears the key.
    struct keybind_binding {
        keybind *bind{};
        Rml::Element *button{};
        Rml::Element *text{};
        Rml::Element *popup{};
    };

    void bind_checkbox(std::string_view id, bool *value,
                       std::function<void()> on_change = {}) noexcept;
    void bind_slider(std::string_view id, std::string_view fill_id,
                     std::string_view knob_id, std::string_view value_id,
                     float min, float max, int decimals, std::string suffix,
                     std::function<void(float)> apply) noexcept;
    void bind_combo(std::string_view id, std::string_view popup_id,
                    std::string_view text_id,
                    const std::vector<std::string_view> &item_ids,
                    std::vector<std::string> labels, int *value,
                    std::function<void(int)> apply) noexcept;
    void bind_keybind(std::string_view id, std::string_view text_id,
                      std::string_view popup_id, keybind *bind) noexcept;
    void bind_click(std::string_view id, std::function<void()> action) noexcept;

    void select_tab(std::string_view tab) noexcept;
    void select_panel(std::string_view panel) noexcept;
    void refresh_glider() noexcept;
    void animate_active_panel() noexcept;
    void set_cached_number(std::string_view id, int value,
                           std::string_view suffix = {}) noexcept;

    void update_slider_from_mouse(slider_binding &binding, int mouse_x) noexcept;
    void set_slider_value(slider_binding &binding, float value) noexcept;

    void start_listening(keybind_binding &binding) noexcept;
    void set_keybind_mode(keybind_binding &binding,
                          keybind_mode mode) noexcept;
    void update_keybind_text(keybind_binding &binding) noexcept;
    void refresh_keybinds() noexcept;
    void open_keybind_popup(keybind_binding &binding) noexcept;

    void materialize_position() noexcept;
    void start_drag() noexcept;
    void update_drag() noexcept;
    void start_resize() noexcept;
    void update_resize() noexcept;

    /// RmlUi reports absolute/client sizes in pixels while property values are
    /// in dp; with a density ratio != 1 the two differ.
    [[nodiscard]] float dp_scale() const noexcept;

    /// Manual scrolling: RmlUi's `overflow` breaks the flex layout used here,
    /// so the content is translated instead.
    void scroll_by(float delta) noexcept;
    void reset_scroll() noexcept;

    /// RmlUi keeps the previous layout of the deep content tree when an
    /// ancestor element is resized, so the scroll container is given an
    /// explicit width whenever the panel size changes.
    void refresh_content_width() noexcept;

    /// Popups live inside the scrolling content; move them to the menu root
    /// and place them under their anchor so they are not clipped.
    void position_popup(Rml::Element *popup, Rml::Element *anchor) noexcept;

    void close_popups() noexcept;
    void set_text(Rml::Element *element, std::string_view text) noexcept;
    void set_cached(std::string_view id, std::string_view text) noexcept;
    void store_edit() noexcept;

    Rml::ElementDocument *document_{};
    const acr::game *game_{};
    features::esp *esp_{};
    features::rage *rage_{};
    features::thirdperson *thirdperson_{};
    keybind *menu_key_{};
    std::function<void()> spawn_bot_;

    Rml::Element *menu_root_{};
    Rml::Element *header_{};
    Rml::Element *tabs_{};
    Rml::Element *drag_handle_{};
    Rml::Element *resize_handle_{};
    Rml::Element *content_area_{};
    Rml::Element *scroll_content_{};
    float scroll_offset_{};
    float scroll_min_{};
    float scroll_max_{};
    Rml::Element *sidebar_{};
    Rml::Element *glider_{};

    std::unordered_map<Rml::Element *, std::function<void()>> click_actions_;
    std::vector<std::unique_ptr<slider_binding>> sliders_;
    std::unordered_map<Rml::Element *, slider_binding *> slider_lookup_;
    std::vector<std::unique_ptr<combo_binding>> combos_;
    std::unordered_map<Rml::Element *, combo_binding *> combo_lookup_;
    std::vector<std::pair<Rml::Element *, bool *>> checkbox_bindings_;
    std::vector<std::pair<Rml::Element *, std::string>> nav_items_;
    std::vector<std::unique_ptr<keybind_binding>> keybinds_;
    std::unordered_map<Rml::Element *, keybind_binding *> keybind_lookup_;

    slider_binding *active_slider_{};
    combo_binding *open_combo_{};
    keybind_binding *open_keybind_{};
    keybind_binding *listening_{};

    bool dragging_{};
    bool resizing_{};
    bool menu_moved_{};
    bool menu_resized_{};
    float drag_offset_x_{};
    float drag_offset_y_{};
    float resize_start_w_{};
    float resize_start_h_{};
    float resize_start_x_{};
    float resize_start_y_{};
    int drag_applied_x_dp_{};
    int drag_applied_y_dp_{};
    int resize_applied_w_dp_{};
    int resize_applied_h_dp_{};
    int cursor_x_{};
    int cursor_y_{};
    int viewport_width_{};
    int viewport_height_{};
    int sync_view_w_{};
    int sync_view_h_{};
    int content_width_dp_{};

    std::string active_tab_{"aim"};
    std::string active_panel_{"aim-main"};
    std::unordered_map<std::string, std::string> text_cache_;
    std::unordered_map<std::string, float> number_display_;
    std::uint32_t sync_counter_{};

    int edit_index_{features::rage::all_weapons};
    features::rage_profile edit_profile_{};
    int profile_ui_index_{0};
    int priority_ui_{0};
    int hitbox_ui_{0};
    bool thirdperson_enabled_{};
    int thirdperson_distance_ui_{10};
};
} // namespace zephyrus::overlay
