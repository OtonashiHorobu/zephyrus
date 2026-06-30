// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/zephyrus.hpp>

int main() {
    using namespace zephyrus;
    g_zephyrus = std::make_unique<class zephyrus>();
    return g_zephyrus->init();
}
