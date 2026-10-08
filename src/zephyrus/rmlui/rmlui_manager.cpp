// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>
#include <zephyrus/sdl/sdl.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <SDL/SDL.h>

#include <array>

namespace zephyrus::rmlui {
namespace {
void load_default_font() noexcept {
    // The Analysts' design language is monospace; JetBrains Mono is the
    // reference face, DejaVu covers the remaining glyphs and acts as the
    // final fallback.
    constexpr std::array mono_candidates{
        "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/ttf-jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
    };
    constexpr std::array mono_bold_candidates{
        "/usr/share/fonts/TTF/JetBrainsMono-Bold.ttf",
        "/usr/share/fonts/ttf-jetbrains-mono/JetBrainsMono-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono-Bold.ttf",
    };
    constexpr std::array fallback_candidates{
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationMono-Regular.ttf",
    };

    bool loaded{};
    for (const char *path : mono_candidates) {
        if (Rml::LoadFontFace(path, false, Rml::Style::FontWeight::Normal)) {
            ZEPHYRUS_INFO("font loaded: {}", path);
            loaded = true;
            break;
        }
    }
    for (const char *path : mono_bold_candidates) {
        if (Rml::LoadFontFace(path, false, Rml::Style::FontWeight::Bold)) {
            ZEPHYRUS_INFO("bold font loaded: {}", path);
            break;
        }
    }
    for (const char *path : fallback_candidates) {
        if (Rml::LoadFontFace(path, true)) {
            ZEPHYRUS_INFO("fallback font loaded: {}", path);
            break;
        }
    }
    if (!loaded) {
        ZEPHYRUS_WARN("no system font could be loaded");
    }
}
} // namespace

rmlui_manager::result_type rmlui_manager::init_impl() noexcept {
    ZEPHYRUS_INFO("initializing rmlui");

    ZEPHYRUS_TRACE("setting rmlui file interface to {:#x}",
                   reinterpret_cast<std::uintptr_t>(file_interface_.get()));
    if (!file_interface_) {
        return std::unexpected{"invalid file interface pointer"};
    }
    Rml::SetFileInterface(file_interface_.get());

    const sdl::api *sdl_api{sdl::get()};
    if (auto result{sdl::initialize()}; !result) {
        return std::unexpected{"SDL is not available: " + result.error()};
    }
    sdl_api = sdl::get();
    if (sdl_api == nullptr) {
        return std::unexpected{"SDL function table is not available"};
    }

    ZEPHYRUS_INFO("initializing sdl12/gl2 backend");
    if (mode_ == backend_mode::standalone) {
        if (!Backend::Initialize("Zephyrus", width_, height_, true)) {
            return std::unexpected{"failed to initialize sdl12 backend"};
        }
    } else {
        if (!Backend::InitializeInjected()) {
            return std::unexpected{"failed to initialize injected backend"};
        }
    }

    ZEPHYRUS_INFO("initializing rmlui core");
    Rml::SetSystemInterface(Backend::GetSystemInterface());
    Rml::SetRenderInterface(Backend::GetRenderInterface());
    Rml::Initialise();

    context_ = Rml::CreateContext("main", Rml::Vector2i{width_, height_});
    if (context_ == nullptr) {
        return std::unexpected{"failed to create rmlui context"};
    }

    load_default_font();

    return {};
}

void rmlui_manager::shutdown_impl() noexcept {
    ZEPHYRUS_INFO("shutting down rmlui");

    context_ = nullptr;
    document_ = nullptr;

    const sdl::api *api{sdl::get()};
    const bool gl_valid{mode_ == backend_mode::standalone ||
                        (api != nullptr &&
                         api->was_init(SDL_INIT_VIDEO) != 0)};

    if (gl_valid) {
        // Release every GPU resource while the host's GL context is still
        // current, then destroy the backend interfaces. Both must outlive
        // Rml::Shutdown().
        ZEPHYRUS_TRACE("shutting down rmlui core");
        Rml::Shutdown();

        ZEPHYRUS_TRACE("shutting down sdl12/gl2 backend");
        Backend::Shutdown();
    } else {
        ZEPHYRUS_WARN(
            "rmlui: GL context unavailable, abandoning backend resources "
            "(expected when the host exits without calling cleanup)");
        Backend::Abandon();
    }

    ZEPHYRUS_TRACE("emptying rmlui file interface");
    Rml::SetFileInterface(nullptr);
}

void rmlui_manager::set_dimensions(int width, int height) noexcept {
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) {
        return;
    }
    width_ = width;
    height_ = height;
    Backend::SetViewport(width_, height_);
    if (context_ != nullptr) {
        context_->SetDimensions(Rml::Vector2i{width_, height_});
    }
}

void rmlui_manager::update() noexcept {
    if (context_ != nullptr) {
        context_->Update();
    }
}

void rmlui_manager::render() noexcept {
    if (context_ != nullptr) {
        context_->Render();
    }
}

rmlui_manager::result_type
rmlui_manager::load_document(std::string_view path,
                             std::string_view id) noexcept {
    if (context_ == nullptr) {
        return std::unexpected{"rmlui context is not created"};
    }
    Rml::ElementDocument *document{
        context_->LoadDocument(Rml::String{path})};
    if (document == nullptr) {
        return std::unexpected{"failed to load document: " +
                               std::string{path}};
    }
    if (!id.empty()) {
        document->SetId(Rml::String{id});
    }
    document_ = document;
    document->Show();
    return {};
}
} // namespace zephyrus::rmlui
