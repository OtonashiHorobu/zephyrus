// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace zephyrus::log {
void init_logger() {
    using namespace spdlog;
    if (get("zephyrus")) { // already exists
        return;
    }
    auto console_sink{std::make_shared<sinks::stdout_color_sink_mt>()};
    auto file_sink{std::make_shared<sinks::rotating_file_sink_mt>(
        "zephyrus.log", 10240, 10, true)};
    auto zephyrus_logger{std::make_shared<logger>(
        "zephyrus", sinks_init_list{console_sink, file_sink})};
    register_logger(zephyrus_logger);
}
} // namespace zephyrus::log
