// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/rmlui.hpp>
#include <zephyrus/version.hpp>
#include <zephyrus/zephyrus.hpp>

namespace zephyrus {
int zephyrus::init() {
    log::init_logger();
    ZEPHYRUS_INFO(version::full_version());

    // rmlui::init_rmlui();
    return 0;
}
} // namespace zephyrus
