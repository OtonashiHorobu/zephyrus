// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/log/logging_manager.hpp>

#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace zephyrus::log {
logging_manager::result_type logging_manager::init_impl() noexcept {
    if (spdlog::get(ZEPHYRUS_LOGGER_NAME) ||
        spdlog::thread_pool()) { // already exists
        return std::unexpected{"logger already initialized"};
    }
    spdlog::init_thread_pool(8192, 1);
    auto console_sink{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
    auto file_sink{std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "zephyrus.log", 10240, 10, true)};
    auto zephyrus_logger{std::make_shared<spdlog::async_logger>(
        ZEPHYRUS_LOGGER_NAME,
        spdlog::sinks_init_list{std::move(console_sink), std::move(file_sink)},
        spdlog::thread_pool())};
    zephyrus_logger->set_level(
#ifndef NDEBUG
        spdlog::level::trace
#else
        spdlog::level::info
#endif
    );
    spdlog::register_logger(zephyrus_logger);
    spdlog::set_default_logger(zephyrus_logger);
    return {};
}

void logging_manager::shutdown_impl() noexcept { spdlog::shutdown(); }
} // namespace zephyrus::log
