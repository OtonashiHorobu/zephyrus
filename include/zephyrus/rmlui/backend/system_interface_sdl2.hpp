// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>

#include <SDL2/SDL.h>

#include <string_view>
#include <unordered_map>

namespace zephyrus::backend {
/// RmlUi system interface backed by the SDL2 instance loaded by the host.
/// Mirrors `system_interface_sdl12` but speaks SDL2 events (`SDL_Event`,
/// `SDL_TEXTINPUT`, `SDL_MOUSEWHEEL`, ...).
class system_interface_sdl2 : public Rml::SystemInterface {
  public:
    system_interface_sdl2() = default;
    ~system_interface_sdl2() override;

    system_interface_sdl2(const system_interface_sdl2 &) = delete;
    system_interface_sdl2 &operator=(const system_interface_sdl2 &) = delete;

    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String &message) override;
    void SetMouseCursor(const Rml::String &cursor_name) override;
    void SetClipboardText(const Rml::String &text) override;
    void GetClipboardText(Rml::String &text) override;
    void ActivateKeyboard(Rml::Vector2f caret_position,
                          float line_height) override;
    void DeactivateKeyboard() override;

    void init_cursors();
    void free_cursors();

    /// Feed one SDL2 event to the RmlUi context. Returns false when the
    /// context rejected the event.
    static bool handle_input_event(Rml::Context *context, SDL_Event &ev);
    static Rml::Input::KeyIdentifier convert_key(int sdl_key);
    static int convert_mouse_button(int sdl_button);
    static int get_key_modifier_state();

    /// Clipboard helpers (thin wrappers around the host SDL2 library).
    static void SetClipboard(const std::string_view &text);
    [[nodiscard]] static Rml::String GetClipboard();

  private:
    std::unordered_map<std::string_view, SDL_Cursor *> cursors_;
};
} // namespace zephyrus::backend
