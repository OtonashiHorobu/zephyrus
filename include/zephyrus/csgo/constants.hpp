// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <string_view>

namespace zephyrus::csgo {
/// Linux module file names of the legacy CS:GO client (build 1.38.8.1).
/// The launcher `csgo_linux64` only bootstraps; the game lives in these
/// shared objects, resolved at runtime from the already-loaded instance.
inline constexpr std::string_view client_module{"client_client.so"};
inline constexpr std::string_view engine_module{"engine_client.so"};
inline constexpr std::string_view sdl_module{"libSDL2-2.0.so.0"};

/// The build this port targets (`csgo/steam.inf`).
inline constexpr std::string_view target_patch_version{"1.38.8.1"};
inline constexpr std::string_view target_client_version{"1575"};
} // namespace zephyrus::csgo
