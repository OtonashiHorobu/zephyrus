// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/log/logging_manager.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>
#include <zephyrus/util/getter.hpp>

namespace zephyrus {
enum class backend_mode { standalone, injected };

class zephyrus : public abstract::manager<zephyrus> {
  public:
    explicit zephyrus(rmlui::rmlui_manager rmlui_manager,
                      log::logging_manager logging_manager) noexcept
        : manager<zephyrus>{}, rmlui_manager_{std::move(rmlui_manager)},
          logging_manager_{std::move(logging_manager)} {}

    [[nodiscard]] result_type init_impl() noexcept;
    void shutdown_impl() noexcept;

    ZEPHYRUS_GETTER(rmlui_manager)
    ZEPHYRUS_GETTER(logging_manager)

  private:
    rmlui::rmlui_manager rmlui_manager_;
    log::logging_manager logging_manager_;
};
} // namespace zephyrus
