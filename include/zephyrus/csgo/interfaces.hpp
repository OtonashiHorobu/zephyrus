// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/hook/hook.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace zephyrus::csgo {
/// `CreateInterface` as exported by every Source engine module.
using create_interface_fn = void *(*)(const char *name, int *return_code);

/// Resolve `CreateInterface` from a module located by `hook::find_module`.
/// The module must already be loaded by the host (RTLD_NOLOAD).
[[nodiscard]] std::expected<create_interface_fn, std::string>
interface_factory(const hook::module_info &module) noexcept;

/// Fetch an interface by versioned name, e.g. "VEngineClient014".
[[nodiscard]] std::expected<void *, std::string>
create_interface(const hook::module_info &module,
                 std::string_view name) noexcept;
} // namespace zephyrus::csgo
