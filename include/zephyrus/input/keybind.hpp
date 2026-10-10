// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <string_view>

namespace zephyrus {
/// Binding modes follow the Osiris convention: Off / Always / Hold / Toggle.
/// An unbound key (0) with Always behaves like a plain feature switch.
enum class keybind_mode : int {
    off = 0,
    always = 1,
    hold = 2,
    toggle = 3,
};

struct keybind {
    int key{};
    keybind_mode mode{keybind_mode::always};
    bool toggled{};
    bool key_down{};

    /// Whether the bound feature should currently be active.
    [[nodiscard]] bool active() const noexcept {
        switch (mode) {
        case keybind_mode::off:
            return false;
        case keybind_mode::always:
            return true;
        case keybind_mode::hold:
            return key_down;
        case keybind_mode::toggle:
            return toggled;
        }
        return true;
    }

    [[nodiscard]] bool matches(int code) const noexcept {
        return key != 0 && key == code;
    }

    /// Feed a key event; returns true when the event belongs to this binding.
    bool on_key(int code, bool isdown) noexcept {
        if (!matches(code)) {
            return false;
        }
        const bool was_down{key_down};
        key_down = isdown;
        if (isdown && !was_down) {
            pressed = true;
        }
        if (mode == keybind_mode::toggle && isdown) {
            toggled = !toggled;
        }
        return true;
    }

    /// True exactly once after every fresh press (edge).
    [[nodiscard]] bool consume_pressed() noexcept {
        const bool value{pressed};
        pressed = false;
        return value;
    }

    void reset() noexcept {
        key_down = false;
        toggled = false;
        pressed = false;
    }

    bool pressed{};
};

/// Human readable name for an SDL 1.2 key symbol (e.g. "INSERT").
[[nodiscard]] std::string_view key_name(int key) noexcept;

/// Reverse of `key_name`; returns 0 for unknown names.
[[nodiscard]] int key_from_name(std::string_view name) noexcept;

/// Stable config-file spelling of a bind mode.
[[nodiscard]] std::string_view keybind_mode_name(keybind_mode mode) noexcept;

/// Parse a bind mode; anything unknown becomes `always`.
[[nodiscard]] keybind_mode keybind_mode_from_name(std::string_view name) noexcept;
} // namespace zephyrus
