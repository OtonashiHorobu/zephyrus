// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>

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

    // CJK fallback: the mono faces above carry no Han glyphs, so Chinese
    // text would render as tofu without this.
    struct cjk_candidate {
        const char *path;
        int face_index;
    };
    constexpr std::array cjk_candidates{
        cjk_candidate{"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
                      7}, // Noto Sans Mono CJK SC
        cjk_candidate{"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
                      2}, // Noto Sans CJK SC
        cjk_candidate{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                      7},
        cjk_candidate{"/usr/share/fonts/wqy-microhei/wqy-microhei.ttc", 0},
        cjk_candidate{"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc", 0},
    };

    bool cjk_loaded{};
    for (const auto &candidate : cjk_candidates) {
        if (Rml::LoadFontFace(candidate.path, true,
                              Rml::Style::FontWeight::Normal,
                              candidate.face_index)) {
            ZEPHYRUS_INFO("cjk fallback font loaded: {} (face {})",
                          candidate.path, candidate.face_index);
            cjk_loaded = true;
            break;
        }
    }
    if (!cjk_loaded) {
        ZEPHYRUS_WARN("no CJK fallback font could be loaded");
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

    ZEPHYRUS_INFO("initializing platform backend");
    if (!Backend::Initialize(mode_, "Zephyrus", width_, height_, true)) {
        return std::unexpected{"failed to initialize the platform backend"};
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

    const bool gl_valid{Backend::ContextAlive()};

    if (gl_valid) {
        // Release every GPU resource while the host's GL context is still
        // current, then destroy the backend interfaces. Both must outlive
        // Rml::Shutdown().
        ZEPHYRUS_TRACE("shutting down rmlui core");
        Rml::Shutdown();

        ZEPHYRUS_TRACE("shutting down platform backend");
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
