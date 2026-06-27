// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>

namespace zephyrus::log {
void setup_logger() {
    if (spdlog::get("zephyrus")) {
        return;
    }
    auto console_sink{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()};
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "zephyrus.log", 10240, 10, true);
    auto zephyrus_logger = std::make_shared<spdlog::logger>(
        "zephyrus", spdlog::sinks_init_list{console_sink, file_sink});
    spdlog::register_logger(zephyrus_logger);
}
} // namespace zephyrus::log
