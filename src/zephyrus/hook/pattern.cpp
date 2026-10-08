// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/hook/hook.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace zephyrus::hook {
namespace {
struct pattern_byte {
    std::uint8_t value{};
    bool wildcard{};
};

[[nodiscard]] int hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] std::expected<std::vector<pattern_byte>, std::string>
parse_pattern(std::string_view pattern) {
    std::vector<pattern_byte> parsed;
    std::size_t i{};

    while (i < pattern.size()) {
        const char c{pattern[i]};
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++i;
            continue;
        }

        if (c == '?') {
            ++i;
            if (i < pattern.size() && pattern[i] == '?') {
                ++i;
            }
            parsed.push_back(pattern_byte{.value = 0, .wildcard = true});
            continue;
        }

        if (i + 1 >= pattern.size()) {
            return std::unexpected{"truncated hex byte in pattern"};
        }
        const int high{hex_digit(pattern[i])};
        const int low{hex_digit(pattern[i + 1])};
        if (high < 0 || low < 0) {
            return std::unexpected{"invalid hex byte in pattern"};
        }
        parsed.push_back(pattern_byte{.value = static_cast<std::uint8_t>(
                                          (high << 4) | low),
                                      .wildcard = false});
        i += 2;
    }

    if (parsed.empty()) {
        return std::unexpected{"empty pattern"};
    }
    return parsed;
}
} // namespace

std::expected<const std::byte *, std::string>
find_pattern(std::span<const std::byte> range,
             std::string_view pattern) noexcept {
    auto parsed{parse_pattern(pattern)};
    if (!parsed) {
        return std::unexpected{parsed.error()};
    }

    const auto &bytes{*parsed};
    if (bytes.size() > range.size()) {
        return std::unexpected{"pattern longer than the scanned range"};
    }

    const std::size_t last{range.size() - bytes.size()};
    for (std::size_t offset{}; offset <= last; ++offset) {
        bool matches{true};
        for (std::size_t i{}; i < bytes.size(); ++i) {
            if (bytes[i].wildcard) {
                continue;
            }
            if (std::to_integer<std::uint8_t>(range[offset + i]) !=
                bytes[i].value) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return range.data() + offset;
        }
    }

    return std::unexpected{"pattern not found"};
}

std::expected<const std::byte *, std::string>
find_pattern(const module_info &module, std::string_view pattern) noexcept {
    if (module.base == nullptr || module.size == 0) {
        return std::unexpected{"invalid module"};
    }
    return find_pattern(
        std::span{static_cast<const std::byte *>(module.base), module.size},
        pattern);
}
} // namespace zephyrus::hook
