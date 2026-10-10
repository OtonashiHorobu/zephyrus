// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <expected>
#include <optional>
#include <string>
#include <unordered_set>

#include <zephyrus/acr/game.hpp>
#include <zephyrus/config/config.hpp>
#include <zephyrus/features/acr/esp.hpp>
#include <zephyrus/features/acr/exploits.hpp>
#include <zephyrus/features/acr/movement.hpp>
#include <zephyrus/features/acr/rage.hpp>
#include <zephyrus/features/acr/thirdperson.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/input/keybind.hpp>
#include <zephyrus/overlay/acr/menu.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

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
    [[nodiscard]] features::movement &movement() noexcept {
        return movement_;
    }
    [[nodiscard]] features::exploits &exploits() noexcept {
        return exploits_;
    }

    /// Configuration entry points.
    ///
    /// Startup always uses the built-in defaults; `ZEPHYRUS_ACR_CONFIG` can
    /// point at a file to load instead. Every change is snapshotted into
    /// `latest.toml` automatically (recovery only, never loaded implicitly);
    /// the menu offers explicit "load latest" and profile save/load.
    void save_config() noexcept;         ///< write the latest snapshot now
    void load_latest_config() noexcept;  ///< apply `latest.toml`
    void save_profile_config() noexcept; ///< write a timestamped profile
    void reset_config() noexcept;        ///< restore defaults
    /// Name based variants used by the Config tab ("latest" = snapshot).
    void save_config_named(const std::string &name) noexcept;
    void load_config_named(const std::string &name) noexcept;
    void remove_config_named(const std::string &name) noexcept;
    /// Display names for the profile list; "latest" comes first.
    [[nodiscard]] std::vector<std::string> list_configs() const noexcept;
    void open_config_folder() noexcept;
    void export_config_clipboard() noexcept;
    void import_config_clipboard() noexcept;

  private:
    void initialize_runtime() noexcept;
    void on_present() noexcept;
    bool on_key(int code, bool isdown, int cooked, int mod) noexcept;
    bool on_mouse_move(int dx, int dy) noexcept;
    void set_visible(bool visible) noexcept;
    void update_cursor() noexcept;
    void draw_fade(int width, int height) noexcept;
    /// Refresh the TOML tree from the current options and note differences.
    void sync_config() noexcept;
    /// Apply a specific profile file.
    void load_config_file(const std::filesystem::path &path) noexcept;
    /// Serialized current options (used for change detection).
    [[nodiscard]] std::string current_config_dump() noexcept;
    /// Poll the file-based test command channel (one command per line).
    void poll_commands() noexcept;

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
    features::movement movement_{};
    features::exploits exploits_{};
    menu menu_{};
    Rml::ElementDocument *menu_document_{};

    config::document config_{};
    std::string config_snapshot_;
    bool config_ready_{};
    int config_tick_{};
    std::string command_path_{};
    int command_tick_{};

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
    // Window-drag state: virtual cursor (window coordinates) and the last
    // sampled physical position used to derive per-frame deltas while the
    // pointer is recentred.
    int drag_cursor_x_{};
    int drag_cursor_y_{};
    int drag_sample_x_{};
    int drag_sample_y_{};
    std::unordered_set<int> pressed_keys_;
};
} // namespace zephyrus::overlay
