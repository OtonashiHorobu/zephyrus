// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <expected>
#include <optional>
#include <string>
#include <unordered_set>

#include <zephyrus/acr/game.hpp>
#include <zephyrus/features/esp.hpp>
#include <zephyrus/features/rage.hpp>
#include <zephyrus/features/thirdperson.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/input/keybind.hpp>
#include <zephyrus/overlay/menu.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>

#include <chrono>

namespace zephyrus::overlay {
/// In-game RmlUi overlay for ACR, driven entirely from game-level hooks:
///
///  - `SDL_GL_SwapBuffers` draws the UI and the ESP before every present;
///  - `keypress` / `mousemove` route input to the UI while it is visible;
///  - `inputgrab` and `SDL_WarpMouse` are suspended while the UI is visible so
///    the system cursor can move freely instead of being locked to the window
///    center by the game's relative mouse handling.
///
/// All SDL interaction goes through the instance already loaded by the game.
class overlay {
  public:
    overlay() noexcept = default;
    ~overlay() noexcept;
    overlay(const overlay &) = delete;
    overlay(overlay &&) = delete;
    overlay &operator=(const overlay &) = delete;
    overlay &operator=(overlay &&) = delete;

    /// Resolve SDL and install the present hook. The RmlUi runtime itself is
    /// initialised lazily on the first present, where an OpenGL context is
    /// guaranteed to be current.
    [[nodiscard]] std::expected<void, std::string>
    initialize(rmlui::rmlui_manager &rmlui) noexcept;

    /// Uninstall every hook. Safe to call multiple times.
    void shutdown() noexcept;

    /// Toggle overlay visibility (same as pressing INSERT).
    void toggle() noexcept;

    [[nodiscard]] bool visible() const noexcept;

    /// Feature switches, also exposed as exported C entry points for testing.
    void set_esp(bool enabled) noexcept;
    void set_rage(bool enabled) noexcept;
    void request_bots(int count) noexcept;

    [[nodiscard]] features::esp &esp() noexcept { return esp_; }
    [[nodiscard]] features::rage &rage() noexcept { return rage_; }

  private:
    void initialize_runtime() noexcept;
    void on_present() noexcept;
    bool on_key(int code, bool isdown, int cooked, int mod) noexcept;
    bool on_mouse_move(int dx, int dy) noexcept;
    void set_visible(bool visible) noexcept;
    void update_cursor() noexcept;

    rmlui::rmlui_manager *rmlui_{};

    hook::function_hook<void()> present_hook_;
    hook::function_hook<void(int, bool, int, int)> key_hook_;
    hook::function_hook<void(int, int)> mouse_hook_;
    hook::function_hook<void(bool)> grab_hook_;
    hook::function_hook<void(unsigned short, unsigned short)> warp_hook_;

    std::optional<acr::game> game_;
    features::esp esp_{};
    features::rage rage_{};
    features::thirdperson thirdperson_{};
    menu menu_{};
    Rml::ElementDocument *menu_document_{};

    keybind menu_key_{};

    std::chrono::steady_clock::time_point last_present_{};
    int fps_{};

    int bots_requested_{};
    int bots_spawned_{};
    int bot_spawn_cooldown_{};

    bool ready_{};
    bool visible_{};
    bool grab_requested_{true};
    int cursor_x_{};
    int cursor_y_{};
    std::unordered_set<int> pressed_keys_;
};
} // namespace zephyrus::overlay
