// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/version.hpp>

namespace zephyrus::version {
std::string_view full_version() noexcept {
    static const std::string full_version_static{
        fmt::format("Zephyrus ACR {} [git {}:{}]"
#if !defined(NDEBUG) || defined(_DEBUG)
                    " [debug]"
#endif
                    ,
                    release_version, branch_name, commit_hash)};
    return full_version_static;
}
} // namespace zephyrus::version
