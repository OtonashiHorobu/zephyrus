// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace zephyrus::csgo {
/// Chainable raw address helper (mirrors Osiris' `SafeAddress`): pattern
/// matches are refined through pointer chains and rip-relative offsets.
class address {
  public:
    address() noexcept = default;
    explicit address(const void *value) noexcept : value_{value} {}

    address &add(std::ptrdiff_t offset) noexcept {
        if (value_ != nullptr) {
            value_ = static_cast<const std::byte *>(value_) + offset;
        }
        return *this;
    }

    template <std::size_t N = 1> address &deref() noexcept {
        if constexpr (N != 0) {
            if (value_ != nullptr) {
                value_ = deref_value<const void *>();
                return deref<N - 1>();
            }
        }
        return *this;
    }

    /// Interpret the current position as a 32-bit rip-relative displacement
    /// (the operand of a `call`/`lea`/`mov`) and resolve the target.
    address &abs() noexcept {
        if (value_ != nullptr) {
            using offset_type = std::int32_t;
            const auto next{static_cast<const std::byte *>(value_) +
                            sizeof(offset_type)};
            value_ = next + deref_value<offset_type>();
        }
        return *this;
    }

    template <typename T> [[nodiscard]] T as() const noexcept {
        return reinterpret_cast<T>(const_cast<void *>(value_));
    }

    [[nodiscard]] const void *raw() const noexcept { return value_; }
    [[nodiscard]] bool valid() const noexcept { return value_ != nullptr; }

  private:
    template <typename T> [[nodiscard]] T deref_value() const noexcept {
        T value;
        std::memcpy(&value, value_, sizeof(T));
        return value;
    }

    const void *value_{};
};
} // namespace zephyrus::csgo
