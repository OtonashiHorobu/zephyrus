// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/platform/detection.hpp>
#include <zephyrus/zephyrus.hpp>

#if ZEPHYRUS_OS_LINUX
__attribute__((constructor)) void zephyrus_ctor() noexcept {
    static_cast<void>(zephyrus::g_zephyrus().init());
}

__attribute__((destructor)) void zephyrus_dtor() noexcept {
    zephyrus::g_zephyrus().shutdown();
}
#endif
