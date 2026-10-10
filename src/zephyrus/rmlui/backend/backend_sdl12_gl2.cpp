// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <memory>
#include <mutex>

#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/rmlui/backend/render_interface_gl2.hpp>
#include <zephyrus/rmlui/backend/system_interface_sdl12.hpp>
#include <zephyrus/sdl/sdl.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Math.h>
#include <SDL/SDL.h>

namespace {
struct backend_data {
    zephyrus::backend::system_interface_sdl12 system_interface;
    zephyrus::backend::render_interface_gl2 render_interface;

    SDL_Surface *sdl_surface{};
    bool running{true};
    bool injected{};
};

std::mutex bd_mutex;
std::unique_ptr<backend_data> bd;
} // namespace

namespace Backend {
bool Initialize(const zephyrus::backend_mode mode, const char *const window_name,
                const int width, const int height,
                [[maybe_unused]] const bool allow_resize) {
    std::lock_guard<std::mutex> lock{bd_mutex};

    if (bd) {
        return false;
    }

    if (mode != zephyrus::backend_mode::standalone) {
        // Injected mode: the host game already owns the window, SDL and GL
        // context. Only the interfaces are created here; the RmlUi runtime is
        // initialized later, from the first present callback.
        bd = std::make_unique<backend_data>();
        bd->injected = true;
        bd->system_interface.init_cursors();
        return true;
    }

    if (auto result{zephyrus::sdl::initialize()}; !result) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL function table unavailable: %s",
                          result.error().c_str());
        return false;
    }

    const zephyrus::sdl::api *api{zephyrus::sdl::get()};
    if (api == nullptr) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL function table unavailable");
        return false;
    }

    if (api->init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_Init failed: %s",
                          api->get_error());
        return false;
    }

    api->gl_set_attribute(SDL_GL_STENCIL_SIZE, 8);
    api->gl_set_attribute(SDL_GL_DOUBLEBUFFER, 1);

    api->gl_set_attribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    api->gl_set_attribute(SDL_GL_MULTISAMPLESAMPLES, 2);

    Uint32 flags{SDL_OPENGL | SDL_RESIZABLE};
    SDL_Surface *surface{api->set_video_mode(width, height, 0, flags)};

    if (surface == nullptr) {
        api->gl_set_attribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        api->gl_set_attribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        surface = api->set_video_mode(width, height, 0, flags);
    }

    if (surface == nullptr) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_SetVideoMode failed: %s",
                          api->get_error());
        return false;
    }

    bd = std::make_unique<backend_data>();
    bd->sdl_surface = surface;

    api->wm_set_caption(window_name, nullptr);

    bd->render_interface.set_viewport(bd->sdl_surface->w, bd->sdl_surface->h);

    bd->system_interface.init_cursors();

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
    const zephyrus::sdl::api *api{zephyrus::sdl::get()};
    return api != nullptr && api->was_init(SDL_INIT_VIDEO) != 0;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock{bd_mutex};

    if (!bd) {
        return;
    }

    bd->system_interface.free_cursors();

    if (!bd->injected) {
        bd->sdl_surface = nullptr;
        if (const zephyrus::sdl::api *api{zephyrus::sdl::get()};
            api != nullptr) {
            api->quit();
        }
    }

    bd.reset();
}

void Abandon() {
    std::lock_guard<std::mutex> lock{bd_mutex};

    if (!bd) {
        return;
    }

    // SDL_Quit may already have released the cursors; free_cursors() detects
    // that via SDL_WasInit and skips the SDL calls in that case.
    bd->system_interface.free_cursors();

    // RmlUi is still initialised (Rml::Shutdown could not run without a GL
    // context) and references these interfaces, so destroying them here
    // would be a use-after-free. Leak them: the process is exiting anyway.
    (void)bd.release();
}

void SetViewport(int width, int height) {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd && width > 0 && height > 0) {
        bd->render_interface.set_viewport(width, height);
    }
}

Rml::SystemInterface *GetSystemInterface() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    return bd ? &bd->system_interface : nullptr;
}

Rml::RenderInterface *GetRenderInterface() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    return bd ? &bd->render_interface : nullptr;
}

bool ProcessEvents(Rml::Context *const context, const KeyDownCallback callback,
                   const bool power_save) {
    using zephyrus::backend::system_interface_sdl12;

    const zephyrus::sdl::api *api{zephyrus::sdl::get()};
    if (api == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock{bd_mutex};

    if (!bd) {
        return false;
    }

    bool result{bd->running};
    bd->running = true;

    SDL_Event ev;
    bool has_event{};

    if (power_save) {
        const int timeout_ms{static_cast<int>(
            Rml::Math::Min(context->GetNextUpdateDelay(), 10.0) * 1000)};
        const Uint32 start{api->get_ticks()};
        while (!has_event &&
               (api->get_ticks() - start) < static_cast<Uint32>(timeout_ms)) {
            has_event = api->poll_event(&ev);
            if (!has_event) {
                api->delay(1);
            }
        }
    } else {
        has_event = api->poll_event(&ev);
    }

    while (has_event) {
        bool propagate_event{true};

        switch (ev.type) {
        case SDL_QUIT:
            propagate_event = false;
            result = false;
            break;

        case SDL_KEYDOWN: {
            propagate_event = false;
            const Rml::Input::KeyIdentifier key{
                system_interface_sdl12::convert_key(ev.key.keysym.sym)};
            const int key_modifier{
                system_interface_sdl12::get_key_modifier_state()};
            constexpr float native_dp_ratio{1.0f};

            if (callback &&
                !callback(context, key, key_modifier, native_dp_ratio, true)) {
                break;
            }
            if (!system_interface_sdl12::handle_input_event(context, ev)) {
                break;
            }
            if (callback &&
                !callback(context, key, key_modifier, native_dp_ratio, false)) {
                break;
            }
        } break;

        case SDL_VIDEORESIZE: {
            bd->sdl_surface = api->set_video_mode(ev.resize.w, ev.resize.h, 0,
                                                  SDL_OPENGL | SDL_RESIZABLE);
            if (bd->sdl_surface) {
                bd->render_interface.set_viewport(bd->sdl_surface->w,
                                                  bd->sdl_surface->h);
                Rml::Vector2i dimensions{bd->sdl_surface->w,
                                         bd->sdl_surface->h};
                context->SetDimensions(dimensions);
            }
        } break;

        default:
            break;
        }

        if (propagate_event) {
            system_interface_sdl12::handle_input_event(context, ev);
        }

        has_event = api->poll_event(&ev);
    }

    return result;
}

void RequestExit() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd) {
        bd->running = false;
    }
}

void BeginFrame() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd) {
        // Injected mode draws into the host's live framebuffer: never clear it.
        if (!bd->injected) {
            bd->render_interface.clear();
        }
        bd->render_interface.begin_frame();
    }
}

void PresentFrame() {
    std::lock_guard<std::mutex> lock{bd_mutex};
    if (bd) {
        bd->render_interface.end_frame();
        // Injected mode is called from inside the host's swap hook; the swap
        // itself is performed by the caller.
        if (!bd->injected) {
            if (const zephyrus::sdl::api *api{zephyrus::sdl::get()};
                api != nullptr) {
                api->gl_swap_buffers();
            }
        }
    }
}
} // namespace Backend
