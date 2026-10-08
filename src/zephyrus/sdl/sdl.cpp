// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/sdl/sdl.hpp>

#include <dlfcn.h>

#include <mutex>
#include <optional>
#include <type_traits>

namespace zephyrus::sdl {
namespace {
std::once_flag initialization_flag;
std::optional<api> resolved_api;
std::string initialization_error;

[[nodiscard]] std::expected<api, std::string> resolve_api() {
    void *fallback{nullptr};
    dlerror();
    if (dlsym(RTLD_DEFAULT, "SDL_GL_SwapBuffers") == nullptr) {
        fallback = dlopen("libSDL-1.2.so.0", RTLD_NOW | RTLD_LOCAL);
        if (fallback == nullptr) {
            const char *error{dlerror()};
            return std::unexpected{
                "SDL is not loaded by the host and cannot be opened: " +
                std::string{error != nullptr ? error : "unknown error"}};
        }
    }

    const auto fetch{[fallback](const char *name) -> void * {
        dlerror();
        if (void *symbol{dlsym(RTLD_DEFAULT, name)}; symbol != nullptr) {
            return symbol;
        }
        if (fallback != nullptr) {
            dlerror();
            return dlsym(fallback, name);
        }
        return nullptr;
    }};

    api table{};
    std::string missing;

    const auto assign{[&](auto &slot, const char *name) {
        void *symbol{fetch(name)};
        if (symbol == nullptr) {
            if (missing.empty()) {
                missing = name;
            }
            return false;
        }
        slot = reinterpret_cast<std::decay_t<decltype(slot)>>(symbol);
        return true;
    }};

    bool complete{true};
    complete &= assign(table.get_ticks, "SDL_GetTicks");
    complete &= assign(table.was_init, "SDL_WasInit");
    complete &= assign(table.get_mod_state, "SDL_GetModState");
    complete &= assign(table.enable_unicode, "SDL_EnableUNICODE");
    complete &= assign(table.create_cursor, "SDL_CreateCursor");
    complete &= assign(table.set_cursor, "SDL_SetCursor");
    complete &= assign(table.free_cursor, "SDL_FreeCursor");
    complete &= assign(table.show_cursor, "SDL_ShowCursor");
    complete &= assign(table.get_mouse_state, "SDL_GetMouseState");
    complete &= assign(table.warp_mouse, "SDL_WarpMouse");
    complete &= assign(table.wm_grab_input, "SDL_WM_GrabInput");
    complete &= assign(table.delay, "SDL_Delay");
    complete &= assign(table.gl_get_proc_address, "SDL_GL_GetProcAddress");
    complete &= assign(table.gl_swap_buffers, "SDL_GL_SwapBuffers");
    complete &= assign(table.init, "SDL_Init");
    complete &= assign(table.quit, "SDL_Quit");
    complete &= assign(table.set_video_mode, "SDL_SetVideoMode");
    complete &= assign(table.gl_set_attribute, "SDL_GL_SetAttribute");
    complete &= assign(table.wm_set_caption, "SDL_WM_SetCaption");
    complete &= assign(table.poll_event, "SDL_PollEvent");
    complete &= assign(table.get_error, "SDL_GetError");
    complete &= assign(table.enable_key_repeat, "SDL_EnableKeyRepeat");

    if (!complete) {
        return std::unexpected{"SDL symbol not found: " + missing};
    }
    return table;
}
} // namespace

std::expected<const api *, std::string> initialize() noexcept {
    std::call_once(initialization_flag, [] {
        if (auto result{resolve_api()}; result) {
            resolved_api = *result;
        } else {
            initialization_error = result.error();
        }
    });

    if (resolved_api) {
        return &*resolved_api;
    }
    return std::unexpected{initialization_error};
}

const api *get() noexcept { return resolved_api ? &*resolved_api : nullptr; }
} // namespace zephyrus::sdl
