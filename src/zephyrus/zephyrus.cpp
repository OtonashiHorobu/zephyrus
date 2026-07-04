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
    if (auto result{rmlui_manager_.init()}; !result) {
        return std::unexpected{
            fmt::format("failed to initialize rmlui: {}", result.error())};
    }
    return {};
}

void zephyrus::shutdown_impl() noexcept {
    rmlui_manager_.shutdown();
    logging_manager_.shutdown();
}

Rml::Context *zephyrus::get_rmlui_context() const noexcept {
    return rmlui_manager_.get_context();
}
} // namespace zephyrus
