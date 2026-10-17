// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/version.hpp>

#include <string>

namespace zephyrus::version {
namespace {
[[nodiscard]] std::string build(const std::string_view product) {
    return fmt::format("{} {} [git {}:{}]"
#if !defined(NDEBUG) || defined(_DEBUG)
                       " [debug]"
#endif
                       ,
                       product, release_version, branch_name, commit_hash);
}
} // namespace

std::string_view full_version() noexcept {
    static const std::string value{build("Zephyrus")};
    return value;
}

std::string_view csgo_version() noexcept {
    static const std::string value{build("Zephyrus CS:GO")};
    return value;
}
} // namespace zephyrus::version
