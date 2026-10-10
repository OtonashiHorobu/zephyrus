// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>
#include <zephyrus/sdl2/sdl2.hpp>

#include <dlfcn.h>

#include <mutex>
#include <optional>
#include <string>
#include <type_traits>

namespace zephyrus::sdl2 {
namespace {
// Resolution is retried until it succeeds: the overlay installer polls this
// before the engine has loaded SDL2, and caching that first failure would
// disable the overlay for the whole session.
std::mutex resolve_mutex;
std::optional<api> resolved_api;
std::string last_error;

/// Locate the SDL2 instance already loaded by the host. The game opens its
/// bundled copy with a path such as `bin/linux64/libSDL2-2.0.so.0`;
/// RTLD_NOLOAD reuses that mapping instead of loading a second copy, which
/// would create a second event queue / video state.
[[nodiscard]] void *open_loaded_sdl2() {
    if (auto module{hook::find_module("libSDL2-2.0.so.0")}; module) {
        if (void *handle{dlopen(module->path.c_str(),
                                RTLD_LAZY | RTLD_NOLOAD)};
            handle != nullptr) {
            return handle;
        }
    }
    return dlopen("libSDL2-2.0.so.0", RTLD_LAZY | RTLD_NOLOAD);
}

[[nodiscard]] std::expected<api, std::string> resolve_api() {
    void *handle{open_loaded_sdl2()};
    if (handle == nullptr) {
        const char *error{dlerror()};
        return std::unexpected{
            "SDL2 is not loaded by the host (" +
            std::string{error != nullptr ? error : "unknown error"} + ")"};
    }

    api table{};
    std::string missing;

    const auto assign{[&](auto &slot, const char *name) {
        void *symbol{dlsym(handle, name)};
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
    complete &= assign(table.delay, "SDL_Delay");
    complete &= assign(table.get_error, "SDL_GetError");
    complete &= assign(table.was_init, "SDL_WasInit");
    complete &= assign(table.poll_event, "SDL_PollEvent");
    complete &= assign(table.push_event, "SDL_PushEvent");
    complete &= assign(table.get_keyboard_state, "SDL_GetKeyboardState");
    complete &= assign(table.get_mod_state, "SDL_GetModState");
    complete &= assign(table.get_mouse_state, "SDL_GetMouseState");
    complete &= assign(table.warp_mouse_in_window, "SDL_WarpMouseInWindow");
    complete &= assign(table.set_relative_mouse_mode,
                       "SDL_SetRelativeMouseMode");
    complete &= assign(table.get_relative_mouse_mode,
                       "SDL_GetRelativeMouseMode");
    complete &= assign(table.create_cursor, "SDL_CreateCursor");
    complete &= assign(table.set_cursor, "SDL_SetCursor");
    complete &= assign(table.free_cursor, "SDL_FreeCursor");
    complete &= assign(table.show_cursor, "SDL_ShowCursor");
    complete &= assign(table.gl_get_proc_address, "SDL_GL_GetProcAddress");
    complete &= assign(table.gl_swap_window, "SDL_GL_SwapWindow");
    complete &= assign(table.gl_get_current_window,
                       "SDL_GL_GetCurrentWindow");
    complete &= assign(table.gl_get_drawable_size, "SDL_GL_GetDrawableSize");
    complete &= assign(table.set_clipboard_text, "SDL_SetClipboardText");
    complete &= assign(table.get_clipboard_text, "SDL_GetClipboardText");

    // Optional entry points: only needed by less common code paths.
    (void)assign(table.peep_events, "SDL_PeepEvents");
    (void)assign(table.get_global_mouse_state, "SDL_GetGlobalMouseState");
    (void)assign(table.capture_mouse, "SDL_CaptureMouse");
    (void)assign(table.get_keyboard_focus, "SDL_GetKeyboardFocus");
    (void)assign(table.get_window_size, "SDL_GetWindowSize");
    (void)assign(table.gl_get_current_context, "SDL_GL_GetCurrentContext");
    (void)assign(table.start_text_input, "SDL_StartTextInput");
    (void)assign(table.stop_text_input, "SDL_StopTextInput");
    (void)assign(table.has_clipboard_text, "SDL_HasClipboardText");
    complete &= assign(table.free_memory, "SDL_free");

    if (!complete) {
        return std::unexpected{"SDL2 symbol not found: " + missing};
    }
    // The handle is intentionally kept open: it pins the host's SDL2 mapping
    // (and therefore every function pointer above) for the process lifetime.
    return table;
}
} // namespace

std::expected<const api *, std::string> initialize() noexcept {
    std::lock_guard<std::mutex> lock{resolve_mutex};
    if (resolved_api) {
        return &*resolved_api;
    }

    if (auto result{resolve_api()}; result) {
        resolved_api = *result;
        return &*resolved_api;
    } else {
        last_error = result.error();
        return std::unexpected{last_error};
    }
}

const api *get() noexcept { return resolved_api ? &*resolved_api : nullptr; }
} // namespace zephyrus::sdl2
