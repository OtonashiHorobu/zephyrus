// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/log/logging_manager.hpp>
#include <zephyrus/rmlui/rmlui_manager.hpp>

namespace Rml {
class Context;
}

namespace zephyrus {
enum class backend_mode { standalone, injected };

class zephyrus : public abstract::manager<zephyrus> {
  public:
    explicit zephyrus(rmlui::rmlui_manager rmlui_manager,
                      log::logging_manager logging_manager,
                      [[maybe_unused]] backend_mode mode) noexcept
        : manager<zephyrus>{}, rmlui_manager_{std::move(rmlui_manager)},
          logging_manager_{std::move(logging_manager)} {}

    [[nodiscard]] result_type init_impl() noexcept;
    void shutdown_impl() noexcept;

    [[nodiscard]] Rml::Context *get_rmlui_context() const noexcept;

  private:
    rmlui::rmlui_manager rmlui_manager_;
    log::logging_manager logging_manager_;
};
} // namespace zephyrus
