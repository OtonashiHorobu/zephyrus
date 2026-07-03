// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>

#include <RmlUi/Core/Core.h>

namespace zephyrus::rmlui {
rmlui_manager::result_type rmlui_manager::init_impl() noexcept {
    ZEPHYRUS_INFO("initializing rmlui");

    ZEPHYRUS_TRACE("setting rmlui file interface to {:#x}",
                   reinterpret_cast<std::uintptr_t>(file_interface_));
    if (!file_interface_) {
        return std::unexpected{"invalid file interface pointer"};
    }
    Rml::SetFileInterface(file_interface_);

    return {};
}

void rmlui_manager::shutdown_impl() noexcept {
    ZEPHYRUS_INFO("shutting down rmlui");

    ZEPHYRUS_TRACE("emptying rmlui file interface");
    Rml::SetFileInterface(nullptr);
}
} // namespace zephyrus::rmlui
