// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/abstract/manager.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

namespace Rml {
class Context;
}

namespace zephyrus {
enum class backend_mode;
}

namespace zephyrus::rmlui {
class rmlui_manager : public abstract::manager<rmlui_manager> {
  public:
    rmlui_manager() noexcept : manager<rmlui_manager>{} {}
    explicit rmlui_manager(cmrc_file_interface *file_interface,
                           backend_mode mode) noexcept
        : manager<rmlui_manager>{}, file_interface_{file_interface},
          mode_{mode} {}

    result_type init_impl() noexcept;
    void shutdown_impl() noexcept;

    [[nodiscard]] Rml::Context *get_context() const noexcept;

  private:
    cmrc_file_interface *file_interface_{};
    backend_mode mode_{};
    Rml::Context *context_{};
};
} // namespace zephyrus::rmlui
