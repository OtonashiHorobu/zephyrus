// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/rmlui/backend/render_interface_gl2.hpp>
#include <zephyrus/rmlui/backend/system_interface_sdl12.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Math.h>
#include <RmlUi_Backend.h>
#include <SDL/SDL.h>

namespace {
struct backend_data {
    zephyrus::backend::system_interface_sdl12 system_interface;
    zephyrus::backend::render_interface_gl2 render_interface;

    SDL_Surface *sdl_surface{};
    bool running{true};
    bool injected{};
};

backend_data &bd() {
    static backend_data instance{};
    return instance;
}

bool g_initialized{};
} // namespace

namespace Backend {
bool Initialize(const char *const window_name, const int width,
                const int height, [[maybe_unused]] const bool allow_resize) {
    if (g_initialized) {
        return false;
    }

    auto &data{bd()};

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_Init failed: %s",
                          SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);

    Uint32 flags{SDL_OPENGL | SDL_RESIZABLE};
    data.sdl_surface = SDL_SetVideoMode(width, height, 0, flags);

    if (!data.sdl_surface) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        data.sdl_surface = SDL_SetVideoMode(width, height, 0, flags);
    }

    if (!data.sdl_surface) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "SDL_SetVideoMode failed: %s",
                          SDL_GetError());
        return false;
    }

    SDL_WM_SetCaption(window_name, nullptr);

    data.render_interface.set_viewport(data.sdl_surface->w,
                                       data.sdl_surface->h);

    data.system_interface.init_cursors();

    g_initialized = true;
    return true;
}

bool InitializeInjected() {
    if (g_initialized) {
        return false;
    }

    auto &data{bd()};
    data.injected = true;

    data.system_interface.init_cursors();

    g_initialized = true;
    return true;
}

void Shutdown() {
    if (!g_initialized) {
        return;
    }

    auto &data{bd()};
    data.system_interface.free_cursors();

    if (!data.injected) {
        data.sdl_surface = nullptr;
        SDL_Quit();
    }

    g_initialized = false;
}

Rml::SystemInterface *GetSystemInterface() {
    return g_initialized ? &bd().system_interface : nullptr;
}

Rml::RenderInterface *GetRenderInterface() {
    return g_initialized ? &bd().render_interface : nullptr;
}

bool ProcessEvents(Rml::Context *const context, const KeyDownCallback callback,
                   const bool power_save) {
    using zephyrus::backend::system_interface_sdl12;

    if (!g_initialized) {
        return false;
    }

    auto &data{bd()};

    bool result{data.running};
    data.running = true;

    SDL_Event ev;
    bool has_event{};

    if (power_save) {
        const int timeout_ms{static_cast<int>(
            Rml::Math::Min(context->GetNextUpdateDelay(), 10.0) * 1000)};
        const Uint32 start{SDL_GetTicks()};
        while (!has_event &&
               (SDL_GetTicks() - start) < static_cast<Uint32>(timeout_ms)) {
            has_event = SDL_PollEvent(&ev);
            if (!has_event) {
                SDL_Delay(1);
            }
        }
    } else {
        has_event = SDL_PollEvent(&ev);
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
            data.sdl_surface = SDL_SetVideoMode(ev.resize.w, ev.resize.h, 0,
                                                SDL_OPENGL | SDL_RESIZABLE);
            if (data.sdl_surface) {
                data.render_interface.set_viewport(data.sdl_surface->w,
                                                   data.sdl_surface->h);
                Rml::Vector2i dimensions{data.sdl_surface->w,
                                         data.sdl_surface->h};
                context->SetDimensions(dimensions);
            }
        } break;

        default:
            break;
        }

        if (propagate_event) {
            system_interface_sdl12::handle_input_event(context, ev);
        }

        has_event = SDL_PollEvent(&ev);
    }

    return result;
}

void RequestExit() { bd().running = false; }

void BeginFrame() {
    auto &data{bd()};
    data.render_interface.clear();
    data.render_interface.begin_frame();
}

void PresentFrame() {
    bd().render_interface.end_frame();
    SDL_GL_SwapBuffers();
}
} // namespace Backend
