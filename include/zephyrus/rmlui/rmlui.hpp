// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <memory>

#include <zephyrus/rmlui/cmrc_file_interface.hpp>

namespace zephyrus::rmlui {
class rmlui {
  public:
    void init();

  private:
    std::unique_ptr<cmrc_file_interface> file_interface_;
};
} // namespace zephyrus::rmlui
