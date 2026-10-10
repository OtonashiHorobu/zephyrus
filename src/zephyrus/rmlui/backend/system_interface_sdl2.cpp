// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/backend/system_interface_sdl2.hpp>
#include <zephyrus/sdl2/sdl2.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>

#include <chrono>

namespace {
// Same XBM cursor set as the SDL 1.2 interface (MSB-first, 16x16):
// data=1 & mask=1 -> black, data=0 & mask=1 -> white, mask=0 -> transparent.
// clang-format off
constexpr Uint8 arrow_data[] = {
    0x00,0x00, 0x40,0x00, 0x60,0x00, 0x70,0x00,
    0x78,0x00, 0x7C,0x00, 0x7E,0x00, 0x7F,0x00,
    0x7F,0x80, 0x7C,0x00, 0x6C,0x00, 0x46,0x00,
    0x06,0x00, 0x03,0x00, 0x03,0x00, 0x00,0x00};
constexpr Uint8 arrow_mask[] = {
    0xC0,0x00, 0xE0,0x00, 0xF0,0x00, 0xF8,0x00,
    0xFC,0x00, 0xFE,0x00, 0xFF,0x00, 0xFF,0x80,
    0xFF,0xC0, 0xFF,0xC0, 0xFE,0x00, 0xEF,0x00,
    0x0F,0x00, 0x07,0x80, 0x07,0x80, 0x03,0x80};

constexpr Uint8 move_data[] = {
    0x01,0x00, 0x03,0x80, 0x07,0xC0, 0x01,0x00,
    0x01,0x00, 0x11,0x10, 0x31,0x18, 0x7F,0xFC,
    0x7F,0xFC, 0x31,0x18, 0x11,0x10, 0x01,0x00,
    0x01,0x00, 0x07,0xC0, 0x03,0x80, 0x01,0x00};
constexpr Uint8 move_mask[] = {
    0x01,0x00, 0x03,0x80, 0x07,0xC0, 0x0F,0xE0,
    0x1B,0x30, 0x31,0x18, 0x71,0x1C, 0xFF,0xFE,
    0xFF,0xFE, 0x71,0x1C, 0x31,0x18, 0x1B,0x30,
    0x0F,0xE0, 0x07,0xC0, 0x03,0x80, 0x01,0x00};

constexpr Uint8 pointer_data[] = {
    0x00,0x00, 0x06,0x00, 0x09,0x00, 0x09,0x00,
    0x08,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
    0x08,0x00, 0x08,0x00, 0x08,0x00, 0x09,0x80,
    0x18,0x00, 0x20,0x00, 0x00,0x00, 0x00,0x00};
constexpr Uint8 pointer_mask[] = {
    0x06,0x00, 0x0F,0x00, 0x1F,0x80, 0x1F,0x80,
    0x1F,0x00, 0x1F,0x00, 0x1F,0x00, 0x1F,0x00,
    0x1F,0x00, 0x1F,0x00, 0x1F,0x80, 0x1F,0xC0,
    0x3E,0x00, 0x7C,0x00, 0x20,0x00, 0x00,0x00};

constexpr Uint8 resize_data[] = {
    0x00,0x00, 0x00,0x06, 0x00,0x0E, 0x00,0x1E,
    0x00,0x3E, 0x00,0x7E, 0x00,0xFE, 0x01,0xFE,
    0x18,0x06, 0x3C,0x00, 0x7E,0x00, 0xFF,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00};
constexpr Uint8 resize_mask[] = {
    0x00,0x0F, 0x00,0x1F, 0x00,0x3F, 0x00,0x7F,
    0x00,0xFF, 0x01,0xFF, 0x03,0xFF, 0x7F,0xFF,
    0xFF,0xFF, 0x7F,0xFE, 0xFF,0x00, 0xFF,0x80,
    0xFF,0xC0, 0x00,0x00, 0x00,0x00, 0x00,0x00};

constexpr Uint8 cross_data[] = {
    0x00,0x00, 0x01,0x00, 0x01,0x00, 0x01,0x00,
    0x01,0x00, 0x01,0x00, 0x00,0x80, 0x7C,0x7C,
    0x00,0x80, 0x01,0x00, 0x01,0x00, 0x01,0x00,
    0x01,0x00, 0x01,0x00, 0x00,0x00, 0x00,0x00};
constexpr Uint8 cross_mask[] = {
    0x03,0x80, 0x03,0x80, 0x03,0x80, 0x03,0x80,
    0x03,0x80, 0x03,0x80, 0x7F,0xFE, 0xFF,0xFE,
    0x7F,0xFE, 0x03,0x80, 0x03,0x80, 0x03,0x80,
    0x03,0x80, 0x03,0x80, 0x03,0x80, 0x00,0x00};

constexpr Uint8 text_data[] = {
    0x00,0x00, 0x00,0x00, 0x02,0x40, 0x02,0x40,
    0x02,0x40, 0x02,0x40, 0x02,0x40, 0x02,0x40,
    0x3F,0xF8, 0x02,0x40, 0x02,0x40, 0x02,0x40,
    0x02,0x40, 0x02,0x40, 0x02,0x40, 0x00,0x00};
constexpr Uint8 text_mask[] = {
    0x00,0x00, 0x07,0xE0, 0x07,0xE0, 0x07,0xE0,
    0x07,0xE0, 0x07,0xE0, 0x07,0xE0, 0x3F,0xF8,
    0xFF,0xFE, 0x3F,0xF8, 0x07,0xE0, 0x07,0xE0,
    0x07,0xE0, 0x07,0xE0, 0x07,0xE0, 0x00,0x00};

constexpr Uint8 unavailable_data[] = {
    0x03,0xC0, 0x0C,0x30, 0x10,0x08, 0x21,0x84,
    0x23,0xC4, 0x47,0xE2, 0x47,0xE2, 0x47,0xE2,
    0x47,0xE2, 0x47,0xE2, 0x23,0xC4, 0x21,0x84,
    0x10,0x08, 0x0C,0x30, 0x03,0xC0, 0x00,0x00};
constexpr Uint8 unavailable_mask[] = {
    0x07,0xE0, 0x1F,0xF8, 0x3F,0xFC, 0x7F,0xFE,
    0x7F,0xFE, 0xFF,0xFF, 0xFF,0xFF, 0xFF,0xFF,
    0xFF,0xFF, 0xFF,0xFF, 0x7F,0xFE, 0x7F,0xFE,
    0x3F,0xFC, 0x1F,0xF8, 0x07,0xE0, 0x00,0x00};
// clang-format on

constexpr int cursor_w{16};
constexpr int cursor_h{16};

struct cursor_def {
    const Uint8 *data;
    const Uint8 *mask;
    int hot_x;
    int hot_y;
};

constexpr cursor_def cursor_defs[] = {
    {arrow_data, arrow_mask, 0, 0},
    {move_data, move_mask, 7, 7},
    {pointer_data, pointer_mask, 5, 0},
    {resize_data, resize_mask, 10, 7},
    {cross_data, cross_mask, 7, 7},
    {text_data, text_mask, 7, 7},
    {unavailable_data, unavailable_mask, 7, 7},
};
} // namespace

namespace zephyrus::backend {
system_interface_sdl2::~system_interface_sdl2() { free_cursors(); }

void system_interface_sdl2::init_cursors() {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr) {
        return;
    }

    struct name_def {
        std::string_view name;
        std::size_t def_index;
    };
    constexpr name_def entries[] = {
        {"arrow", 0}, {"move", 1}, {"pointer", 2},     {"resize", 3},
        {"cross", 4}, {"text", 5}, {"unavailable", 6},
    };

    for (const auto &e : entries) {
        const auto &d{cursor_defs[e.def_index]};
        cursors_[e.name] =
            api->create_cursor(d.data, d.mask, cursor_w, cursor_h, d.hot_x,
                               d.hot_y);
    }
}

void system_interface_sdl2::free_cursors() {
    const sdl2::api *api{sdl2::get()};
    // SDL_Quit destroys every cursor it owns; freeing them afterwards would
    // be a double free. Screenshot the subsystem state before touching SDL.
    const bool video_alive{api != nullptr &&
                           api->was_init(SDL_INIT_VIDEO) != 0};
    for (auto &[name, cursor] : cursors_) {
        if (cursor != nullptr && video_alive) {
            api->free_cursor(cursor);
        }
        cursor = nullptr;
    }
    cursors_.clear();
}

double system_interface_sdl2::GetElapsedTime() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start{clock::now()};
    return std::chrono::duration<double>(clock::now() - start).count();
}

bool system_interface_sdl2::LogMessage(const Rml::Log::Type type,
                                       const Rml::String &message) {
    switch (type) {
    case Rml::Log::LT_ERROR:
        ZEPHYRUS_ERROR(message);
        break;
    case Rml::Log::LT_ASSERT:
        ZEPHYRUS_CRITICAL(message);
        break;
    case Rml::Log::LT_INFO:
        ZEPHYRUS_INFO(message);
        break;
    case Rml::Log::LT_WARNING:
        ZEPHYRUS_WARN(message);
        break;
    case Rml::Log::LT_DEBUG:
        ZEPHYRUS_DEBUG(message);
        break;
    case Rml::Log::LT_ALWAYS:
    default:
        ZEPHYRUS_INFO(message);
    }
    return true;
}

void system_interface_sdl2::SetMouseCursor(const Rml::String &cursor_name) {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr) {
        return;
    }

    const std::string_view lookup{cursor_name.empty()
                                      ? std::string_view{"arrow"}
                                      : std::string_view{cursor_name}};

    auto it{cursors_.find(lookup)};
    if (it != cursors_.end()) {
        api->set_cursor(it->second);
        return;
    }

    if (cursor_name.starts_with("rmlui-scroll")) {
        auto scroll_it{cursors_.find("move")};
        if (scroll_it != cursors_.end()) {
            api->set_cursor(scroll_it->second);
        }
    }
}

void system_interface_sdl2::SetClipboardText(const Rml::String &text) {
    const sdl2::api *api{sdl2::get()};
    if (api != nullptr) {
        api->set_clipboard_text(text.c_str());
    }
}

void system_interface_sdl2::GetClipboardText(Rml::String &text) {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr) {
        return;
    }
    if (char *value{api->get_clipboard_text()}; value != nullptr) {
        text = value;
        api->free_memory(value);
    }
}

void system_interface_sdl2::SetClipboard(const std::string_view &text) {
    const sdl2::api *api{sdl2::get()};
    if (api != nullptr) {
        api->set_clipboard_text(std::string{text}.c_str());
    }
}

Rml::String system_interface_sdl2::GetClipboard() {
    Rml::String text;
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr) {
        return text;
    }
    if (char *value{api->get_clipboard_text()}; value != nullptr) {
        text = value;
        api->free_memory(value);
    }
    return text;
}

void system_interface_sdl2::ActivateKeyboard(Rml::Vector2f /*caret*/,
                                             float /*line_height*/) {
    if (const sdl2::api *api{sdl2::get()};
        api != nullptr && api->start_text_input != nullptr) {
        api->start_text_input();
    }
}

void system_interface_sdl2::DeactivateKeyboard() {
    if (const sdl2::api *api{sdl2::get()};
        api != nullptr && api->stop_text_input != nullptr) {
        api->stop_text_input();
    }
}

bool system_interface_sdl2::handle_input_event(Rml::Context *context,
                                               SDL_Event &ev) {
    if (context == nullptr) {
        return false;
    }

    bool result{true};

    switch (ev.type) {
    case SDL_MOUSEMOTION:
        result = context->ProcessMouseMove(ev.motion.x, ev.motion.y,
                                           get_key_modifier_state());
        break;

    case SDL_MOUSEWHEEL: {
        float delta{-static_cast<float>(ev.wheel.y)};
        if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            delta = -delta;
        }
        result = context->ProcessMouseWheel(delta, get_key_modifier_state());
    } break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        // Buttons are sampled from `SDL_GetMouseState` by the overlay every
        // frame: the engine consumes button events through its own event
        // paths, so the queue cannot be trusted for clicks.
        result = true;
        break;

    case SDL_KEYDOWN:
        result = context->ProcessKeyDown(convert_key(ev.key.keysym.sym),
                                         get_key_modifier_state());
        if (ev.key.keysym.sym == SDLK_RETURN ||
            ev.key.keysym.sym == SDLK_KP_ENTER) {
            result &= context->ProcessTextInput('\n');
        }
        break;

    case SDL_KEYUP:
        result = context->ProcessKeyUp(convert_key(ev.key.keysym.sym),
                                       get_key_modifier_state());
        break;

    case SDL_TEXTINPUT:
        result = context->ProcessTextInput(Rml::String{ev.text.text});
        break;

    case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            context->SetDimensions(Rml::Vector2i{ev.window.data1,
                                                 ev.window.data2});
        }
        break;

    default:
        break;
    }

    return result;
}

Rml::Input::KeyIdentifier system_interface_sdl2::convert_key(int sdlkey) {
    // clang-format off
    switch (sdlkey) {
    case SDLK_UNKNOWN:       return Rml::Input::KI_UNKNOWN;
    case SDLK_ESCAPE:        return Rml::Input::KI_ESCAPE;
    case SDLK_SPACE:         return Rml::Input::KI_SPACE;
    case SDLK_0:             return Rml::Input::KI_0;
    case SDLK_1:             return Rml::Input::KI_1;
    case SDLK_2:             return Rml::Input::KI_2;
    case SDLK_3:             return Rml::Input::KI_3;
    case SDLK_4:             return Rml::Input::KI_4;
    case SDLK_5:             return Rml::Input::KI_5;
    case SDLK_6:             return Rml::Input::KI_6;
    case SDLK_7:             return Rml::Input::KI_7;
    case SDLK_8:             return Rml::Input::KI_8;
    case SDLK_9:             return Rml::Input::KI_9;
    case SDLK_a:             return Rml::Input::KI_A;
    case SDLK_b:             return Rml::Input::KI_B;
    case SDLK_c:             return Rml::Input::KI_C;
    case SDLK_d:             return Rml::Input::KI_D;
    case SDLK_e:             return Rml::Input::KI_E;
    case SDLK_f:             return Rml::Input::KI_F;
    case SDLK_g:             return Rml::Input::KI_G;
    case SDLK_h:             return Rml::Input::KI_H;
    case SDLK_i:             return Rml::Input::KI_I;
    case SDLK_j:             return Rml::Input::KI_J;
    case SDLK_k:             return Rml::Input::KI_K;
    case SDLK_l:             return Rml::Input::KI_L;
    case SDLK_m:             return Rml::Input::KI_M;
    case SDLK_n:             return Rml::Input::KI_N;
    case SDLK_o:             return Rml::Input::KI_O;
    case SDLK_p:             return Rml::Input::KI_P;
    case SDLK_q:             return Rml::Input::KI_Q;
    case SDLK_r:             return Rml::Input::KI_R;
    case SDLK_s:             return Rml::Input::KI_S;
    case SDLK_t:             return Rml::Input::KI_T;
    case SDLK_u:             return Rml::Input::KI_U;
    case SDLK_v:             return Rml::Input::KI_V;
    case SDLK_w:             return Rml::Input::KI_W;
    case SDLK_x:             return Rml::Input::KI_X;
    case SDLK_y:             return Rml::Input::KI_Y;
    case SDLK_z:             return Rml::Input::KI_Z;
    case SDLK_SEMICOLON:     return Rml::Input::KI_OEM_1;
    case SDLK_EQUALS:        return Rml::Input::KI_OEM_PLUS;
    case SDLK_COMMA:         return Rml::Input::KI_OEM_COMMA;
    case SDLK_MINUS:         return Rml::Input::KI_OEM_MINUS;
    case SDLK_PERIOD:        return Rml::Input::KI_OEM_PERIOD;
    case SDLK_SLASH:         return Rml::Input::KI_OEM_2;
    case SDLK_BACKQUOTE:     return Rml::Input::KI_OEM_3;
    case SDLK_LEFTBRACKET:   return Rml::Input::KI_OEM_4;
    case SDLK_BACKSLASH:     return Rml::Input::KI_OEM_5;
    case SDLK_RIGHTBRACKET:  return Rml::Input::KI_OEM_6;
    case SDLK_QUOTEDBL:      return Rml::Input::KI_OEM_7;
    case SDLK_KP_0:          return Rml::Input::KI_NUMPAD0;
    case SDLK_KP_1:          return Rml::Input::KI_NUMPAD1;
    case SDLK_KP_2:          return Rml::Input::KI_NUMPAD2;
    case SDLK_KP_3:          return Rml::Input::KI_NUMPAD3;
    case SDLK_KP_4:          return Rml::Input::KI_NUMPAD4;
    case SDLK_KP_5:          return Rml::Input::KI_NUMPAD5;
    case SDLK_KP_6:          return Rml::Input::KI_NUMPAD6;
    case SDLK_KP_7:          return Rml::Input::KI_NUMPAD7;
    case SDLK_KP_8:          return Rml::Input::KI_NUMPAD8;
    case SDLK_KP_9:          return Rml::Input::KI_NUMPAD9;
    case SDLK_KP_ENTER:      return Rml::Input::KI_NUMPADENTER;
    case SDLK_KP_MULTIPLY:   return Rml::Input::KI_MULTIPLY;
    case SDLK_KP_PLUS:       return Rml::Input::KI_ADD;
    case SDLK_KP_MINUS:      return Rml::Input::KI_SUBTRACT;
    case SDLK_KP_PERIOD:     return Rml::Input::KI_DECIMAL;
    case SDLK_KP_DIVIDE:     return Rml::Input::KI_DIVIDE;
    case SDLK_KP_EQUALS:     return Rml::Input::KI_OEM_NEC_EQUAL;
    case SDLK_BACKSPACE:     return Rml::Input::KI_BACK;
    case SDLK_TAB:           return Rml::Input::KI_TAB;
    case SDLK_CLEAR:         return Rml::Input::KI_CLEAR;
    case SDLK_RETURN:        return Rml::Input::KI_RETURN;
    case SDLK_PAUSE:         return Rml::Input::KI_PAUSE;
    case SDLK_CAPSLOCK:      return Rml::Input::KI_CAPITAL;
    case SDLK_PAGEUP:        return Rml::Input::KI_PRIOR;
    case SDLK_PAGEDOWN:      return Rml::Input::KI_NEXT;
    case SDLK_END:           return Rml::Input::KI_END;
    case SDLK_HOME:          return Rml::Input::KI_HOME;
    case SDLK_LEFT:          return Rml::Input::KI_LEFT;
    case SDLK_UP:            return Rml::Input::KI_UP;
    case SDLK_RIGHT:         return Rml::Input::KI_RIGHT;
    case SDLK_DOWN:          return Rml::Input::KI_DOWN;
    case SDLK_INSERT:        return Rml::Input::KI_INSERT;
    case SDLK_DELETE:        return Rml::Input::KI_DELETE;
    case SDLK_HELP:          return Rml::Input::KI_HELP;
    case SDLK_F1:            return Rml::Input::KI_F1;
    case SDLK_F2:            return Rml::Input::KI_F2;
    case SDLK_F3:            return Rml::Input::KI_F3;
    case SDLK_F4:            return Rml::Input::KI_F4;
    case SDLK_F5:            return Rml::Input::KI_F5;
    case SDLK_F6:            return Rml::Input::KI_F6;
    case SDLK_F7:            return Rml::Input::KI_F7;
    case SDLK_F8:            return Rml::Input::KI_F8;
    case SDLK_F9:            return Rml::Input::KI_F9;
    case SDLK_F10:           return Rml::Input::KI_F10;
    case SDLK_F11:           return Rml::Input::KI_F11;
    case SDLK_F12:           return Rml::Input::KI_F12;
    case SDLK_F13:           return Rml::Input::KI_F13;
    case SDLK_F14:           return Rml::Input::KI_F14;
    case SDLK_F15:           return Rml::Input::KI_F15;
    case SDLK_NUMLOCKCLEAR:  return Rml::Input::KI_NUMLOCK;
    case SDLK_SCROLLLOCK:    return Rml::Input::KI_SCROLL;
    case SDLK_LSHIFT:        return Rml::Input::KI_LSHIFT;
    case SDLK_RSHIFT:        return Rml::Input::KI_RSHIFT;
    case SDLK_LCTRL:         return Rml::Input::KI_LCONTROL;
    case SDLK_RCTRL:         return Rml::Input::KI_RCONTROL;
    case SDLK_LALT:          return Rml::Input::KI_LMENU;
    case SDLK_RALT:          return Rml::Input::KI_RMENU;
    case SDLK_LGUI:          return Rml::Input::KI_LMETA;
    case SDLK_RGUI:          return Rml::Input::KI_RMETA;
    default: break;
    }
    // clang-format on

    return Rml::Input::KI_UNKNOWN;
}

int system_interface_sdl2::convert_mouse_button(int button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return 0;
    case SDL_BUTTON_RIGHT:
        return 1;
    case SDL_BUTTON_MIDDLE:
        return 2;
    default:
        return 3;
    }
}

int system_interface_sdl2::get_key_modifier_state() {
    const sdl2::api *api{sdl2::get()};
    const SDL_Keymod sdl_mods{api != nullptr ? api->get_mod_state()
                                             : KMOD_NONE};

    int retval{};

    if ((sdl_mods & KMOD_CTRL) != 0) {
        retval |= Rml::Input::KM_CTRL;
    }
    if ((sdl_mods & KMOD_SHIFT) != 0) {
        retval |= Rml::Input::KM_SHIFT;
    }
    if ((sdl_mods & KMOD_ALT) != 0) {
        retval |= Rml::Input::KM_ALT;
    }
    if ((sdl_mods & KMOD_NUM) != 0) {
        retval |= Rml::Input::KM_NUMLOCK;
    }
    if ((sdl_mods & KMOD_CAPS) != 0) {
        retval |= Rml::Input::KM_CAPSLOCK;
    }

    return retval;
}
} // namespace zephyrus::backend
