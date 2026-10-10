// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/hook/hook.hpp>
#include <zephyrus/input/keybind.hpp>
#include <zephyrus/overlay/csgo/menu.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>

#include <expected>
#include <functional>
#include <string>

union SDL_Event;
struct SDL_Window;

namespace Rml {
class Element;
class ElementDocument;
} // namespace Rml

namespace zephyrus::csgo::overlay {
/// In-game RmlUi overlay for legacy CS:GO.
///
/// Hooks the host's bundled SDL2:
///  - `SDL_GL_SwapWindow` draws the menu before every present. The inline
///    hook targets the implementation behind the exported `FF 25` thunk so
///    callers that cached the resolved pointer are covered too; swapping the
///    thunk's pointer slot is kept as a fallback;
///  - `SDL_PollEvent` routes events to RmlUi (same hybrid hooking); while the
///    menu is visible game input is swallowed and the mouse is released from
///    relative mode.
///
/// The RmlUi runtime itself is initialized lazily on the first present, where
/// a current OpenGL context is guaranteed.
class overlay {
  public:
    overlay() noexcept = default;
    ~overlay() noexcept;
    overlay(const overlay &) = delete;
    overlay(overlay &&) = delete;
    overlay &operator=(const overlay &) = delete;
    overlay &operator=(overlay &&) = delete;

    [[nodiscard]] std::expected<void, std::string>
    initialize(rmlui::rmlui_manager &rmlui) noexcept;

    /// Invoked once from the game thread when the runtime becomes ready
    /// (safe point between frames), before the first menu frame is drawn.
    std::function<void()> on_ready;

    /// Invoked right after the menu controller attached to the document;
    /// use it to bind feature options to menu controls.
    std::function<void(menu &)> menu_setup;

    /// Per-frame ESP layer update (called with `#esp-layer` while ESP or the
    /// menu is active).
    std::function<void(Rml::Element &)> esp_draw;
    /// ESP master switch: the document keeps rendering while this is true
    /// even when the menu itself is closed.
    std::function<bool()> esp_enabled;

    /// Raw keyboard events (SDL scancode, pressed) for global bindings that
    /// live outside the menu document (manual anti-aim directions, ...).
    std::function<void(int, bool)> key_event;

    /// Uninstall every hook. Safe to call multiple times.
    void shutdown() noexcept;

    /// Toggle menu visibility (same as pressing the menu key).
    void toggle() noexcept;

    /// Hide the menu (panic key, automation).
    void close() noexcept { set_visible(false); }

    /// The menu controller (config tab actions, scripted navigation).
    [[nodiscard]] menu &menu_controller() noexcept { return menu_; }

    /// The live menu key binding (SDL scancode + Off/Always/Hold/Toggle).
    [[nodiscard]] keybind &menu_key() noexcept { return menu_key_; }

    /// Re-run `menu_setup` on the live menu document (used when the game
    /// resolves after the menu was already initialized, e.g. the overlay
    /// exists in the main menu and the feature objects appear when a map
    /// loads). No-op before the document is ready.
    void refresh_menu_bindings() noexcept;

    [[nodiscard]] bool visible() const noexcept { return visible_; }
    [[nodiscard]] bool ready() const noexcept { return ready_; }

    /// Hook entry points; called from the SDL2 trampolines.
    void on_present() noexcept;
    void on_event(SDL_Event &event) noexcept;

  private:
    void initialize_runtime() noexcept;
    void set_visible(bool visible) noexcept;
    void update_dimensions() noexcept;
    /// Poll `SDL_GetMouseState` for button edges: the engine consumes mouse
    /// button events through its own event paths, so clicks are taken from
    /// the state instead of the event queue.
    void poll_mouse_buttons() noexcept;

    rmlui::rmlui_manager *rmlui_{};

    hook::pointer_slot swap_slot_{};
    hook::pointer_slot poll_slot_{};
    hook::pointer_slot relative_mode_slot_{};
    hook::pointer_slot warp_slot_{};
    hook::pointer_slot show_cursor_slot_{};
    hook::function_hook<void(SDL_Window *)> swap_inline_hook_{};
    hook::function_hook<int(SDL_Event *)> poll_inline_hook_{};
    hook::function_hook<int(int)> relative_mode_inline_hook_{};
    hook::function_hook<void(SDL_Window *, int, int)> warp_inline_hook_{};
    hook::function_hook<int(int)> show_cursor_inline_hook_{};

    Rml::ElementDocument *menu_document_{};
    Rml::Element *menu_root_element_{};
    Rml::Element *esp_layer_{};
    menu menu_{};
    keybind menu_key_{};
    bool ready_{};
    bool visible_{};
    bool menu_ready_{};
    bool runtime_failed_{};
    bool cursor_state_saved_{};
    bool saved_relative_mode_{};
    int saved_cursor_visible_{1};
    std::uint32_t previous_buttons_{};
    int width_{};
    int height_{};
};
} // namespace zephyrus::csgo::overlay
