// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <atomic>
#include <expected>
#include <string>

namespace zephyrus::abstract {
template <typename Derived> class manager {
  public:
    using result_type = std::expected<void, std::string>;

    [[nodiscard]] result_type init() noexcept {
        if (initialized_) {
            return std::unexpected{"already initialized"};
        }
        const auto result{static_cast<Derived *>(this)->init_impl()};
        if (result) {
            initialized_ = true;
        }
        return result;
    }

    void shutdown() noexcept {
        if (!initialized_) {
            return;
        }
        static_cast<Derived *>(this)->shutdown_impl();
        initialized_ = false;
    }

    manager(manager &&other) noexcept
        : initialized_{other.initialized_.exchange(false)} {}

    ~manager() noexcept { shutdown(); }

  protected:
    manager() noexcept = default;

  private:
    std::atomic<bool> initialized_;
};
} // namespace zephyrus::abstract
