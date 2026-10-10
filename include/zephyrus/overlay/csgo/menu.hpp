// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/config/config.hpp>
#include <zephyrus/input/keybind.hpp>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Rml {
class ElementDocument;
class ElementFormControlInput;
}

namespace zephyrus::csgo::overlay {
/// Controller for the CS:GO menu document: navigation, window handling
/// (drag/resize/scroll), widget feedback, key binds with modes and the config
/// tab (profiles live in `~/.config/zephyrus/profiles`).
class menu final : public Rml::EventListener {
  public:
    menu() noexcept = default;
    ~menu() noexcept override;
    menu(const menu &) = delete;
    menu(menu &&) = delete;
    menu &operator=(const menu &) = delete;
    menu &operator=(menu &&) = delete;

    void initialize(Rml::ElementDocument &document) noexcept;
    void shutdown() noexcept;

    /// Per-frame bookkeeping: fps readout, config autosave, scroll clamp.
    void tick() noexcept;

    /// Window size in pixels, fed by the overlay (drag/resize clamping).
    void set_viewport(int width, int height) noexcept;

    /// Scripted navigation (test/automation helper).
    void show_tab(std::string_view tab) noexcept { select_tab(tab); }
    void show_panel(std::string_view panel) noexcept { select_panel(panel); }

    /// Bind a checkbox (`id`) to a persistent boolean option. A stored value
    /// (under `[features]`) wins over `default_value`; `handler` is invoked
    /// once at bind time and on every toggle.
    void bind_toggle(std::string_view id, bool default_value,
                     std::function<void(bool)> handler) noexcept;
    /// As above, but persist under `storage_key` (used by the per-weapon
    /// pages where the element is reused while the key changes).
    void bind_toggle(std::string_view id, bool default_value,
                     std::function<void(bool)> handler,
                     std::string_view storage_key) noexcept;
    /// Bind a slider (`sl-<name>`) to a persistent number option.
    void bind_slider(std::string_view id, float default_value,
                     std::function<void(float)> handler) noexcept;
    /// As above, but persist under `storage_key`.
    void bind_slider(std::string_view id, float default_value,
                     std::function<void(float)> handler,
                     std::string_view storage_key) noexcept;
    /// Bind a combo (element `id` with `.combo-item` children) to a
    /// persistent index option.
    void bind_combo(std::string_view id, int default_value,
                    std::function<void(int)> handler) noexcept;
    /// As above, but persist under `storage_key`.
    void bind_combo(std::string_view id, int default_value,
                    std::function<void(int)> handler,
                    std::string_view storage_key) noexcept;

    /// Bind a key button to a live `keybind` (left click rebinds, right click
    /// opens the Off/Always/Hold/Toggle/Unset popup). Persisted as a TOML
    /// table under `[features].<storage_key>` (defaults to the element id).
    void bind_key(std::string_view id, keybind &binding,
                  std::string_view storage_key = {}) noexcept;

    /// Bind an arbitrary button to an action.
    void bind_button(std::string_view id,
                     std::function<void()> action) noexcept;

    /// Feed a raw SDL scancode while a key binding is being captured.
    /// Returns true when the event was consumed.
    bool capture_key_event(int scancode) noexcept;
    [[nodiscard]] bool capturing_key() const noexcept {
        return capture_binding_ != nullptr || capture_handler_ != nullptr;
    }

    // --- config tab -----------------------------------------------------
    [[nodiscard]] std::vector<std::string> config_names() const;
    [[nodiscard]] std::string config_name_value() const;
    void config_save(std::string_view name) noexcept;
    void config_load(std::string_view name) noexcept;
    void config_delete(std::string_view name) noexcept;
    void config_reset() noexcept;
    void config_export() noexcept;
    void config_import() noexcept;
    void open_config_folder() noexcept;

    /// Invoked after load/reset/import so the owner re-applies the stored
    /// values to the live feature objects (re-runs its `menu_setup`).
    std::function<void()> on_config_reloaded;

    /// Window geometry in pixels.
    struct geometry {
        float x{160.0F};
        float y{160.0F};
        float width{900.0F};
        float height{620.0F};
    };
    [[nodiscard]] geometry window_geometry() const noexcept;
    void apply_geometry(const geometry &value) noexcept;

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
        std::function<void(float)> on_change;
    };

    struct keybind_binding {
        keybind *bind{};
        Rml::Element *button{};
        Rml::Element *text{};
        Rml::Element *popup{};
        std::string storage_key;
    };

    // navigation
    void select_tab(std::string_view tab) noexcept;
    void select_panel(std::string_view panel) noexcept;
    void refresh_glider() noexcept;

    // window
    void start_drag(int mouse_x, int mouse_y) noexcept;
    void update_drag(int mouse_x, int mouse_y) noexcept;
    void start_resize(int mouse_x, int mouse_y) noexcept;
    void update_resize(int mouse_x, int mouse_y) noexcept;
    void save_geometry() noexcept;

    // sliders
    void set_slider_value(slider_binding &binding, float value) noexcept;
    void update_slider_from_mouse(slider_binding &binding,
                                  int mouse_x) noexcept;

    // combos
    void open_combo(Rml::Element *combo) noexcept;
    void close_popups() noexcept;

    // keybinds
    void start_listening(keybind_binding &binding) noexcept;
    void set_keybind_mode(keybind_binding &binding,
                          keybind_mode mode) noexcept;
    void update_keybind_text(keybind_binding &binding) noexcept;
    void open_keybind_popup(keybind_binding &binding) noexcept;
    void persist_keybind(keybind_binding &binding) noexcept;

    // config tab
    void refresh_config_list() noexcept;
    void select_config(std::string_view name) noexcept;
    void reload_from_config() noexcept;
    [[nodiscard]] std::filesystem::path
    profile_path(std::string_view name) const;

    // scrolling
    void scroll_by(float delta) noexcept;
    void reset_scroll() noexcept;
    void clamp_scroll() noexcept;

    static void set_text(Rml::Element *element, std::string_view text) noexcept;
    /// `.text` span of a combo/keybind element (id `<id>` → `<id>-text`).
    [[nodiscard]] Rml::Element *text_element_of(Rml::Element *element) noexcept;

    void store_bool(std::string_view id, bool value) noexcept;
    void store_float(std::string_view id, double value) noexcept;
    void store_int(std::string_view id, std::int64_t value) noexcept;
    [[nodiscard]] bool stored_bool(std::string_view id, bool fallback) noexcept;
    [[nodiscard]] double stored_float(std::string_view id,
                                      double fallback) noexcept;
    [[nodiscard]] std::int64_t stored_int(std::string_view id,
                                          std::int64_t fallback) noexcept;

    Rml::ElementDocument *document_{};
    std::unordered_map<Rml::Element *, std::function<void()>> click_actions_;
    std::vector<std::pair<Rml::Element *, std::string>> nav_items_;
    std::vector<std::unique_ptr<slider_binding>> sliders_;
    std::unordered_map<Rml::Element *, slider_binding *> slider_lookup_;
    std::unordered_map<Rml::Element *, Rml::Element *> combo_lookup_;
    std::unordered_map<Rml::Element *, std::function<void(int)>>
        combo_handlers_;
    std::vector<std::unique_ptr<keybind_binding>> keybinds_;
    std::unordered_map<Rml::Element *, keybind_binding *> keybind_lookup_;
    config::document config_{};
    Rml::Element *menu_root_{};
    Rml::Element *header_{};
    Rml::Element *tabs_{};
    Rml::Element *sidebar_{};
    Rml::Element *glider_{};
    Rml::Element *content_{};
    Rml::Element *content_scroll_{};
    Rml::Element *resize_handle_{};
    Rml::Element *config_list_{};
    Rml::ElementFormControlInput *config_input_{};
    std::vector<std::pair<Rml::Element *, std::string>> config_rows_;
    std::string selected_config_{};

    bool dragging_{};
    bool resizing_{};
    float drag_offset_x_{};
    float drag_offset_y_{};
    float resize_start_x_{};
    float resize_start_y_{};
    float resize_start_w_{};
    float resize_start_h_{};
    float position_x_{160.0F};
    float position_y_{160.0F};
    float window_width_{900.0F};
    float window_height_{620.0F};
    int viewport_width_{1920};
    int viewport_height_{1080};

    float scroll_offset_{};
    float scroll_min_{};

    slider_binding *active_slider_{};
    Rml::Element *open_combo_{};
    keybind_binding *open_keybind_{};

    Rml::Element *capture_element_{};
    keybind_binding *capture_binding_{};
    std::function<void(int)> capture_handler_;
    std::string capture_storage_;
    int capture_previous_key_{};

    std::string active_tab_{"rage"};
    std::string active_panel_{"rage-main"};

    std::chrono::steady_clock::time_point fps_window_{};
    int fps_frames_{};
};
} // namespace zephyrus::csgo::overlay
