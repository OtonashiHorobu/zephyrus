// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <expected>
#include <string>

#include <SDL/SDL.h>

namespace zephyrus::sdl {
/// Function table for the SDL 1.2 API.
///
/// Zephyrus deliberately does not link SDL: the injected library resolves
/// every entry point at runtime from the instance already loaded by the host
/// game (`RTLD_DEFAULT`), falling back to `dlopen("libSDL-1.2.so.0")` for
/// standalone builds. That guarantees a single SDL state machine (event
/// queue, video, cursor) shared with the game, regardless of whether the
/// loaded implementation is real SDL 1.2 or a compatibility layer.
struct api {
    Uint32 (*get_ticks)();
    Uint32 (*was_init)(Uint32);
    SDLMod (*get_mod_state)();
    int (*enable_unicode)(int);
    SDL_Cursor *(*create_cursor)(Uint8 *, Uint8 *, int, int, int, int);
    void (*set_cursor)(SDL_Cursor *);
    void (*free_cursor)(SDL_Cursor *);
    int (*show_cursor)(int);
    Uint8 (*get_mouse_state)(int *, int *);
    void (*warp_mouse)(Uint16, Uint16);
    SDL_GrabMode (*wm_grab_input)(SDL_GrabMode);
    void (*delay)(Uint32);
    void *(*gl_get_proc_address)(const char *);
    void (*gl_swap_buffers)();
    int (*init)(Uint32);
    void (*quit)();
    SDL_Surface *(*set_video_mode)(int, int, int, Uint32);
    int (*gl_set_attribute)(SDL_GLattr, int);
    void (*wm_set_caption)(const char *, const char *);
    int (*poll_event)(SDL_Event *);
    const char *(*get_error)();
    int (*enable_key_repeat)(int, int);
};

/// Resolve the SDL function table. Idempotent; returns an error string when a
/// required entry point cannot be found.
[[nodiscard]] std::expected<const api *, std::string> initialize() noexcept;

/// Returns the resolved table, or `nullptr` when `initialize()` has not
/// succeeded yet.
[[nodiscard]] const api *get() noexcept;
} // namespace zephyrus::sdl
