// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/platform/detection.hpp>
#include <zephyrus/zephyrus.hpp>

#if ZEPHYRUS_OS_LINUX
__attribute__((constructor)) void zephyrus_ctor() noexcept {
    using namespace zephyrus;
    g_zephyrus = std::make_unique<class zephyrus>();
    g_zephyrus->init();
}
#endif
