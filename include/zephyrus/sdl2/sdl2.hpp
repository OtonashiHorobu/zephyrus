// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <expected>
#include <string>

#include <SDL2/SDL.h>

namespace zephyrus::sdl2 {
/// Function table for the SDL2 API.
///
/// Like the SDL 1.2 module, zephyrus never links SDL: the injected library
/// resolves every entry point from the instance the host game already loaded
/// (`libSDL2-2.0.so.0`, located via `hook::find_module`), so the overlay
/// shares the exact same event queue, video and cursor state as the game.
struct api {
    // Core
    Uint32 (*get_ticks)();
    void (*delay)(Uint32);
    const char *(*get_error)();
    Uint32 (*was_init)(Uint32);

    // Events
    int (*poll_event)(SDL_Event *);
    int (*push_event)(SDL_Event *);
    int (*peep_events)(SDL_Event *, int, SDL_eventaction, Uint32, Uint32);
    const Uint8 *(*get_keyboard_state)(int *);
    SDL_Keymod (*get_mod_state)();

    // Mouse
    Uint32 (*get_mouse_state)(int *, int *);
    Uint32 (*get_global_mouse_state)(int *, int *);
    void (*warp_mouse_in_window)(SDL_Window *, int, int);
    int (*set_relative_mouse_mode)(SDL_bool);
    SDL_bool (*get_relative_mouse_mode)();
    int (*capture_mouse)(SDL_bool);

    // Cursor
    SDL_Cursor *(*create_cursor)(const Uint8 *, const Uint8 *, int, int, int,
                                 int);
    void (*set_cursor)(SDL_Cursor *);
    void (*free_cursor)(SDL_Cursor *);
    int (*show_cursor)(int);

    // Window / GL
    SDL_Window *(*get_keyboard_focus)();
    void (*get_window_size)(SDL_Window *, int *, int *);
    void *(*gl_get_proc_address)(const char *);
    void (*gl_swap_window)(SDL_Window *);
    SDL_Window *(*gl_get_current_window)();
    SDL_GLContext (*gl_get_current_context)();
    void (*gl_get_drawable_size)(SDL_Window *, int *, int *);

    // Text input
    void (*start_text_input)();
    void (*stop_text_input)();

    // Clipboard
    int (*set_clipboard_text)(const char *);
    char *(*get_clipboard_text)();
    SDL_bool (*has_clipboard_text)();
    void (*free_memory)(void *);
};

/// Resolve the SDL2 function table from the host process. Idempotent;
/// returns an error string when SDL2 is not loaded or incomplete.
[[nodiscard]] std::expected<const api *, std::string> initialize() noexcept;

/// Returns the resolved table, or `nullptr` when `initialize()` has not
/// succeeded yet.
[[nodiscard]] const api *get() noexcept;
} // namespace zephyrus::sdl2
