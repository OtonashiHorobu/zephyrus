// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/platform/detection.hpp>
#include <zephyrus/version.hpp>

#if ZEPHYRUS_OS_LINUX
__attribute__((constructor)) void zephyrus_ctor() noexcept {
    zephyrus::setup_log();
    auto logger = spdlog::get("zephyrus");
    SPDLOG_LOGGER_INFO(logger, zephyrus::full_version());
}
#endif
