// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <fmt/format.h>
#include <string>

#include <zephyrus/platform/extern.hpp>

namespace zephyrus {
ZEPHYRUS_EXTERN extern const std::string release_version;
ZEPHYRUS_EXTERN extern const std::string branch_name;
ZEPHYRUS_EXTERN extern const std::string commit_hash;
const std::string full_version() noexcept;
} // namespace zephyrus
