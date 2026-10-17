// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/input/keybind.hpp>

namespace zephyrus {
std::string_view keybind_mode_name(keybind_mode mode) noexcept {
    switch (mode) {
    case keybind_mode::off:
        return "off";
    case keybind_mode::always:
        return "always";
    case keybind_mode::hold:
        return "hold";
    case keybind_mode::toggle:
        return "toggle";
    }
    return "always";
}

keybind_mode keybind_mode_from_name(std::string_view name) noexcept {
    if (name == "off") {
        return keybind_mode::off;
    }
    if (name == "hold") {
        return keybind_mode::hold;
    }
    if (name == "toggle") {
        return keybind_mode::toggle;
    }
    return keybind_mode::always;
}
} // namespace zephyrus
