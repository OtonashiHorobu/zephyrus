// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#define ZEPHYRUS_GETTER(field)                                                 \
    template <typename Self> auto &field(this Self &&self) noexcept {          \
        return self.field##_;                                                  \
    }
