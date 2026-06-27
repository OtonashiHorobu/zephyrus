// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/version.hpp>

int main() {
    zephyrus::log::setup_logger();
    auto logger{spdlog::get("zephyrus")};
    SPDLOG_LOGGER_INFO(logger, zephyrus::version::full_version());
    return 0;
}
