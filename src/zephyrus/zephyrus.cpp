// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/log/logging_manager.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>
#include <zephyrus/version.hpp>
#include <zephyrus/zephyrus.hpp>

#include <fmt/format.h>

namespace zephyrus {
zephyrus::result_type zephyrus::init_impl() noexcept {
    if (auto result{logging_manager_.init()}; !result) {
        return std::unexpected{
            fmt::format("zephyrus: failed to initialize logging system: {}",
                        result.error())};
    }
    ZEPHYRUS_INFO(version::full_version());
    ZEPHYRUS_INFO("initializing zephyrus");
    // RmlUi is initialised by whoever owns the frame loop: the standalone
    // entry point calls rmlui_manager().init() directly, the injected overlay
    // defers it to the first present where the game's GL context is current.
    return {};
}

void zephyrus::shutdown_impl() noexcept {
    rmlui_manager_.shutdown();
    logging_manager_.shutdown();
}
} // namespace zephyrus
