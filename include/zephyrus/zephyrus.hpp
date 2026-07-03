// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/log/logging_manager.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>

namespace zephyrus {
class zephyrus {
  public:
    explicit zephyrus(rmlui::rmlui_manager rmlui_manager,
                      log::logging_manager logging_manager) noexcept
        : rmlui_manager_{std::move(rmlui_manager)},
          logging_manager_{std::move(logging_manager)} {}

    [[nodiscard]] int init() noexcept;
    void shutdown() noexcept;

  private:
    rmlui::rmlui_manager rmlui_manager_;
    log::logging_manager logging_manager_;
};

zephyrus &g_zephyrus() noexcept;
} // namespace zephyrus
