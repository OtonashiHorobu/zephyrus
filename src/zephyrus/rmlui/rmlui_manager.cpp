// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi_Backend.h>

namespace Backend {
bool InitializeInjected();
}

namespace zephyrus::rmlui {
rmlui_manager::result_type rmlui_manager::init_impl() noexcept {
    ZEPHYRUS_INFO("initializing rmlui");

    ZEPHYRUS_TRACE("setting rmlui file interface to {:#x}",
                   reinterpret_cast<std::uintptr_t>(file_interface_));
    if (!file_interface_) {
        return std::unexpected{"invalid file interface pointer"};
    }
    Rml::SetFileInterface(file_interface_);

    ZEPHYRUS_INFO("initializing sdl12/gl2 backend");
    if (mode_ == backend_mode::standalone) {
        if (!Backend::Initialize("Zephyrus", 1280, 720, true)) {
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

    context_ = Rml::CreateContext("main", Rml::Vector2i{1280, 720});
    if (!context_) {
        return std::unexpected{"failed to create rmlui context"};
    }

    return {};
}

void rmlui_manager::shutdown_impl() noexcept {
    ZEPHYRUS_INFO("shutting down rmlui");

    context_ = nullptr;

    ZEPHYRUS_TRACE("shutting down rmlui core");
    Rml::Shutdown();

    ZEPHYRUS_TRACE("shutting down sdl12+gl2 backend");
    Backend::Shutdown();

    ZEPHYRUS_TRACE("emptying rmlui file interface");
    Rml::SetFileInterface(nullptr);
}

Rml::Context *rmlui_manager::get_context() const noexcept { return context_; }
} // namespace zephyrus::rmlui
