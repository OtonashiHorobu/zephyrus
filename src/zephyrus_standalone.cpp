// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/zephyrus.hpp>

int main() {
    const auto result{zephyrus::g_zephyrus().init()};
    zephyrus::g_zephyrus().shutdown();
    return result;
}
