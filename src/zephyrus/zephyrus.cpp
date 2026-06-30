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

    rmlui_.init();
    return 0;
}

zephyrus &global_zephyrus() noexcept {
    static zephyrus static_zephyrus{};
    return static_zephyrus;
}
} // namespace zephyrus
