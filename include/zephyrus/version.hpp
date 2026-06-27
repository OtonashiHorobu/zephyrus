// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <string_view>

#include <fmt/format.h>

namespace zephyrus::version {
extern const char *release_version;
extern const char *branch_name;
extern const char *commit_hash;
std::string_view full_version() noexcept;
} // namespace zephyrus::version
