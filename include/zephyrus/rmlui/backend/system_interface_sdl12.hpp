// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>

#include <SDL/SDL.h>

#include <string_view>
#include <unordered_map>

namespace zephyrus::backend {
class system_interface_sdl12 : public Rml::SystemInterface {
  public:
    system_interface_sdl12() = default;
    ~system_interface_sdl12() override;

    system_interface_sdl12(const system_interface_sdl12 &) = delete;
    system_interface_sdl12 &operator=(const system_interface_sdl12 &) = delete;

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

    static bool handle_input_event(Rml::Context *context, SDL_Event &ev);
    static Rml::Input::KeyIdentifier convert_key(int sdl_key);
    static int convert_mouse_button(int sdl_button);
    static int get_key_modifier_state();

    /// Clipboard helpers (thin wrappers around the host SDL2/3 library).
    static void SetClipboard(const std::string_view &text);
    [[nodiscard]] static Rml::String GetClipboard();

  private:
    std::unordered_map<std::string_view, SDL_Cursor *> cursors_;
};
} // namespace zephyrus::backend
