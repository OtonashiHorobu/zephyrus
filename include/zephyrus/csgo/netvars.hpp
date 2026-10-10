// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace zephyrus::csgo {
/// FNV-1a (32 bit) over a property path such as `CBaseEntity->m_iHealth`.
[[nodiscard]] constexpr std::uint32_t fnv1a(const std::string_view text) noexcept {
    std::uint32_t hash{2166136261U};
    for (const char character : text) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 16777619U;
    }
    return hash;
}

constexpr std::uint32_t operator""_nv(const char *text,
                                      const std::size_t length) noexcept {
    return fnv1a({text, length});
}

/// Networked property offsets, collected by walking the client's recv tables.
/// `m_dwBoneMatrix`-style accessors in `entity` look their offset up here.
class netvars final {
  public:
    /// Walk every recv table reachable from the client's class list.
    static void init(void *client) noexcept;
    static void shutdown() noexcept;

    [[nodiscard]] static std::uint32_t get(std::uint32_t hash) noexcept;
};
} // namespace zephyrus::csgo
