// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/input/keybind.hpp>

#include <SDL/SDL.h>

namespace zephyrus {
std::string_view key_name(int key) noexcept {
    switch (key) {
    case SDLK_UNKNOWN:
        return "Unknown";
    case SDLK_BACKSPACE:
        return "Backspace";
    case SDLK_TAB:
        return "Tab";
    case SDLK_CLEAR:
        return "Clear";
    case SDLK_RETURN:
        return "Enter";
    case SDLK_PAUSE:
        return "Pause";
    case SDLK_ESCAPE:
        return "Escape";
    case SDLK_SPACE:
        return "Space";
    case SDLK_QUOTEDBL:
        return "\"";
    case SDLK_HASH:
        return "#";
    case SDLK_DOLLAR:
        return "$";
    case SDLK_AMPERSAND:
        return "&";
    case SDLK_QUOTE:
        return "'";
    case SDLK_LEFTPAREN:
        return "(";
    case SDLK_RIGHTPAREN:
        return ")";
    case SDLK_ASTERISK:
        return "*";
    case SDLK_PLUS:
        return "+";
    case SDLK_COMMA:
        return ",";
    case SDLK_MINUS:
        return "-";
    case SDLK_PERIOD:
        return ".";
    case SDLK_SLASH:
        return "/";
    case SDLK_0:
        return "0";
    case SDLK_1:
        return "1";
    case SDLK_2:
        return "2";
    case SDLK_3:
        return "3";
    case SDLK_4:
        return "4";
    case SDLK_5:
        return "5";
    case SDLK_6:
        return "6";
    case SDLK_7:
        return "7";
    case SDLK_8:
        return "8";
    case SDLK_9:
        return "9";
    case SDLK_COLON:
        return ":";
    case SDLK_SEMICOLON:
        return ";";
    case SDLK_LESS:
        return "<";
    case SDLK_EQUALS:
        return "=";
    case SDLK_GREATER:
        return ">";
    case SDLK_QUESTION:
        return "?";
    case SDLK_AT:
        return "@";
    case SDLK_LEFTBRACKET:
        return "[";
    case SDLK_BACKSLASH:
        return "\\";
    case SDLK_RIGHTBRACKET:
        return "]";
    case SDLK_CARET:
        return "^";
    case SDLK_UNDERSCORE:
        return "_";
    case SDLK_BACKQUOTE:
        return "`";
    case SDLK_a:
        return "A";
    case SDLK_b:
        return "B";
    case SDLK_c:
        return "C";
    case SDLK_d:
        return "D";
    case SDLK_e:
        return "E";
    case SDLK_f:
        return "F";
    case SDLK_g:
        return "G";
    case SDLK_h:
        return "H";
    case SDLK_i:
        return "I";
    case SDLK_j:
        return "J";
    case SDLK_k:
        return "K";
    case SDLK_l:
        return "L";
    case SDLK_m:
        return "M";
    case SDLK_n:
        return "N";
    case SDLK_o:
        return "O";
    case SDLK_p:
        return "P";
    case SDLK_q:
        return "Q";
    case SDLK_r:
        return "R";
    case SDLK_s:
        return "S";
    case SDLK_t:
        return "T";
    case SDLK_u:
        return "U";
    case SDLK_v:
        return "V";
    case SDLK_w:
        return "W";
    case SDLK_x:
        return "X";
    case SDLK_y:
        return "Y";
    case SDLK_z:
        return "Z";
    case SDLK_DELETE:
        return "Delete";
    case SDLK_KP0:
        return "Num 0";
    case SDLK_KP1:
        return "Num 1";
    case SDLK_KP2:
        return "Num 2";
    case SDLK_KP3:
        return "Num 3";
    case SDLK_KP4:
        return "Num 4";
    case SDLK_KP5:
        return "Num 5";
    case SDLK_KP6:
        return "Num 6";
    case SDLK_KP7:
        return "Num 7";
    case SDLK_KP8:
        return "Num 8";
    case SDLK_KP9:
        return "Num 9";
    case SDLK_KP_PERIOD:
        return "Num .";
    case SDLK_KP_DIVIDE:
        return "Num /";
    case SDLK_KP_MULTIPLY:
        return "Num *";
    case SDLK_KP_MINUS:
        return "Num -";
    case SDLK_KP_PLUS:
        return "Num +";
    case SDLK_KP_ENTER:
        return "Num Enter";
    case SDLK_KP_EQUALS:
        return "Num =";
    case SDLK_UP:
        return "Up";
    case SDLK_DOWN:
        return "Down";
    case SDLK_RIGHT:
        return "Right";
    case SDLK_LEFT:
        return "Left";
    case SDLK_INSERT:
        return "Insert";
    case SDLK_HOME:
        return "Home";
    case SDLK_END:
        return "End";
    case SDLK_PAGEUP:
        return "Page Up";
    case SDLK_PAGEDOWN:
        return "Page Down";
    case SDLK_F1:
        return "F1";
    case SDLK_F2:
        return "F2";
    case SDLK_F3:
        return "F3";
    case SDLK_F4:
        return "F4";
    case SDLK_F5:
        return "F5";
    case SDLK_F6:
        return "F6";
    case SDLK_F7:
        return "F7";
    case SDLK_F8:
        return "F8";
    case SDLK_F9:
        return "F9";
    case SDLK_F10:
        return "F10";
    case SDLK_F11:
        return "F11";
    case SDLK_F12:
        return "F12";
    case SDLK_F13:
        return "F13";
    case SDLK_F14:
        return "F14";
    case SDLK_F15:
        return "F15";
    case SDLK_NUMLOCK:
        return "Num Lock";
    case SDLK_CAPSLOCK:
        return "Caps Lock";
    case SDLK_SCROLLOCK:
        return "Scroll Lock";
    case SDLK_RSHIFT:
        return "R Shift";
    case SDLK_LSHIFT:
        return "L Shift";
    case SDLK_RCTRL:
        return "R Ctrl";
    case SDLK_LCTRL:
        return "L Ctrl";
    case SDLK_RALT:
        return "R Alt";
    case SDLK_LALT:
        return "L Alt";
    case SDLK_LMETA:
        return "L Meta";
    case SDLK_RMETA:
        return "R Meta";
    case SDLK_MODE:
        return "Mode";
    case SDLK_HELP:
        return "Help";
    case SDLK_PRINT:
        return "Print";
    case SDLK_SYSREQ:
        return "SysRq";
    case SDLK_MENU:
        return "Menu";
    case SDLK_POWER:
        return "Power";
    case SDLK_UNDO:
        return "Undo";
    default:
        return "Key";
    }
}

int key_from_name(std::string_view name) noexcept {
    for (int code{}; code < SDLK_LAST; ++code) {
        const std::string_view candidate{key_name(code)};
        if (candidate == name && candidate != "Key" &&
            candidate != "Unknown") {
            return code;
        }
    }
    return 0;
}

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
