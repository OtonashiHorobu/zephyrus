// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/abstract/manager.hpp>

namespace zephyrus::log {
class logging_manager : public abstract::manager<logging_manager> {
  public:
    logging_manager() : manager<logging_manager>{} {}

    result_type init_impl() noexcept;
    void shutdown_impl() noexcept;
};
} // namespace zephyrus::log
