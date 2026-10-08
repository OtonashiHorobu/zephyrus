// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/log/logging_manager.hpp>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace zephyrus::log {
logging_manager::result_type logging_manager::init_impl() noexcept {
    if (spdlog::get(ZEPHYRUS_LOGGER_NAME)) { // already exists
        return std::unexpected{"logger already initialized"};
    }
    auto console_sink{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
    // spdlog colours the level name; the default bright white is too loud for
    // the trace spam, use the muted gray of the Analysts' palette instead.
    console_sink->set_color(spdlog::level::trace, "\x1b[90m");
    auto file_sink{std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "zephyrus.log", 10240, 10, true)};
    // Synchronous logging: the volume is low (hot paths are throttled) and it
    // keeps messages deterministic across runtime unload/reload cycles, where
    // an async queue would be dropped by spdlog::shutdown().
    auto zephyrus_logger{std::make_shared<spdlog::logger>(
        ZEPHYRUS_LOGGER_NAME,
        spdlog::sinks_init_list{std::move(console_sink), std::move(file_sink)})};
    zephyrus_logger->set_level(
#ifndef NDEBUG
        spdlog::level::trace
#else
        spdlog::level::info
#endif
    );
    // Explicit pattern instead of the default `%+`: the full formatter pulls
    // in spdlog's MDC, whose non-trivial `thread_local` map registers a TLS
    // destructor and thereby prevents `dlclose` from ever unloading the
    // module. `%^%$` keeps the level name colour range.
#ifndef NDEBUG
    zephyrus_logger->set_pattern(
        "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%s:%#] %v");
#else
    zephyrus_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
#endif
    zephyrus_logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(zephyrus_logger);
    spdlog::set_default_logger(zephyrus_logger);
    return {};
}

void logging_manager::shutdown_impl() noexcept {
    if (auto logger{spdlog::get(ZEPHYRUS_LOGGER_NAME)}; logger != nullptr) {
        logger->flush();
    }
    spdlog::shutdown();
}
} // namespace zephyrus::log
