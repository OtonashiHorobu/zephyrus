// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/rmlui/rmlui.hpp>

namespace zephyrus {
class zephyrus {
  public:
    int init();

  private:
    rmlui::rmlui rmlui_;
};

zephyrus &global_zephyrus() noexcept;
} // namespace zephyrus
