// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

// CS:GO injected backend: attaches the RmlUi GL2 render interface and the
// SDL2 system interface to the host's live window/GL context. Event delivery
// is owned by the overlay (it hooks SDL_PollEvent); this backend only manages
// the interfaces, the viewport and the frame brackets.

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/rmlui/backend/render_interface_gl3.hpp>
#include <zephyrus/rmlui/backend/system_interface_sdl2.hpp>
#include <zephyrus/sdl2/sdl2.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Log.h>

#include <GL/gl.h>

#include <memory>
#include <mutex>

namespace {
struct backend_data {
    zephyrus::backend::system_interface_sdl2 system_interface;
    zephyrus::backend::render_interface_gl3 render_interface;
    bool injected{};
};

std::mutex bd_mutex;
std::unique_ptr<backend_data> bd;
bool gl3_initialized{};
} // namespace

namespace Backend {
bool Initialize(const zephyrus::backend_mode mode,
                [[maybe_unused]] const char *window_name,
                [[maybe_unused]] int width,
                [[maybe_unused]] int height,
                [[maybe_unused]] bool allow_resize) {
    std::lock_guard<std::mutex> lock{bd_mutex};

    if (bd) {
        return false;
    }
    if (mode == zephyrus::backend_mode::standalone) {
        Rml::Log::Message(Rml::Log::LT_ERROR,
                          "the csgo backend only supports injected mode");
        return false;
    }

    if (auto result{zephyrus::sdl2::initialize()}; !result) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL2 unavailable: %s",
                          result.error().c_str());
        return false;
    }

    // Resolve the GL 3.3 entry points for the current context (glad's own
    // loader uses glXGetProcAddress/dlsym internally).
    if (!gl3_initialized) {
        Rml::String message;
        if (!zephyrus::backend::RmlGL3::Initialize(&message)) {
            Rml::Log::Message(Rml::Log::LT_ERROR, "GL3 loader failed: %s",
                              message.c_str());
            return false;
        }
        gl3_initialized = true;
        Rml::Log::Message(Rml::Log::LT_INFO, "%s", message.c_str());
    }

    bd = std::make_unique<backend_data>();
    bd->injected = true;
    if (!bd->render_interface) {
        Rml::Log::Message(Rml::Log::LT_ERROR,
                          "GL3 render interface construction failed");
        bd.reset();
        return false;
    }
    bd->system_interface.init_cursors();

    // The game's GL context is current here (the overlay initializes the
    // runtime from the first present callback); size the viewport from the
    // real GL drawable. SDL can report the logical window size under
    // compositor scaling, which desynchronises RmlUi's layer math from the
    // framebuffer and produces compositing artifacts.
    int drawable_width{};
    int drawable_height{};
    GLint gl_viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, gl_viewport);
    if (gl_viewport[2] > 0 && gl_viewport[3] > 0) {
        drawable_width = gl_viewport[2];
        drawable_height = gl_viewport[3];
    } else if (const zephyrus::sdl2::api *api{zephyrus::sdl2::get()};
               api != nullptr) {
        if (SDL_Window *window{api->gl_get_current_window()};
            window != nullptr) {
            api->gl_get_drawable_size(window, &drawable_width,
                                      &drawable_height);
        }
    }
    if (drawable_width > 0 && drawable_height > 0) {
        bd->render_interface.SetViewport(drawable_width, drawable_height);
    } else {
        // The overlay calls SetViewport from its first present, so a sane
        // default is enough until then.
        bd->render_interface.SetViewport(1920, 1080);
    }
    return true;
}

bool ContextAlive() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (!bd) {
        return false;
    }
    if (!bd->injected) {
        return true;
    }
    const zephyrus::sdl2::api *api{zephyrus::sdl2::get()};
    return api != nullptr && api->was_init(SDL_INIT_VIDEO) != 0;
}

void SetViewport(int width, int height) {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd && width > 0 && height > 0) {
        bd->render_interface.SetViewport(width, height);
    }
}

void Shutdown() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (!bd) {
        return;
    }
    bd->system_interface.free_cursors();
    bd.reset();
    if (gl3_initialized) {
        zephyrus::backend::RmlGL3::Shutdown();
        gl3_initialized = false;
    }
}

void Abandon() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (!bd) {
        return;
    }

    // SDL_Quit may already have released the cursors; free_cursors() detects
    // that via SDL_WasInit and skips the SDL calls in that case. RmlUi still
    // references these interfaces (Rml::Shutdown could not run without a GL
    // context), so leak them: the process is exiting anyway.
    bd->system_interface.free_cursors();
    (void)bd.release();
}

Rml::SystemInterface *GetSystemInterface() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    return bd ? &bd->system_interface : nullptr;
}

Rml::RenderInterface *GetRenderInterface() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    return bd ? &bd->render_interface : nullptr;
}

bool ProcessEvents([[maybe_unused]] Rml::Context *context,
                   [[maybe_unused]] KeyDownCallback key_down_callback,
                   [[maybe_unused]] bool power_save) {
    // Injected mode: the host owns the event loop and the overlay receives
    // events from its SDL_PollEvent hook; there is nothing to pump here.
    return false;
}

void RequestExit() {}

void BeginFrame() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd) {
        // Injected mode draws into the host's live framebuffer: never clear.
        bd->render_interface.BeginFrame();
    }
}

void PresentFrame() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd) {
        bd->render_interface.EndFrame();
        // The swap itself is performed by the host's swap hook.
    }
}
} // namespace Backend
