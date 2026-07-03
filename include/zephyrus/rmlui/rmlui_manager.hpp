// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/abstract/manager.hpp>
#include <zephyrus/rmlui/cmrc_file_interface.hpp>

namespace zephyrus::rmlui {
class rmlui_manager : public abstract::manager<rmlui_manager> {
  public:
    rmlui_manager() noexcept : manager<rmlui_manager>{} {}
    explicit rmlui_manager(cmrc_file_interface *const file_interface) noexcept
        : manager<rmlui_manager>{}, file_interface_{file_interface} {}

    result_type init_impl() noexcept;
    void shutdown_impl() noexcept;

  private:
    cmrc_file_interface *file_interface_;
};
} // namespace zephyrus::rmlui
