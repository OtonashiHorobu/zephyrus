// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński. Linux pattern table only (legacy CS:GO 1.38.8.1 linux64).

#include <zephyrus/csgo/address.hpp>
#include <zephyrus/csgo/patterns.hpp>
#include <zephyrus/log.hpp>

#include <string_view>

namespace zephyrus::csgo {
namespace {
void *find_in(const hook::module_info &module, const std::string_view pattern,
              int &missing) noexcept {
    auto match{hook::find_pattern(module, pattern)};
    if (!match) {
        ++missing;
        ZEPHYRUS_WARN("pattern not found: {}", pattern);
        return nullptr;
    }
    return const_cast<void *>(static_cast<const void *>(*match));
}
} // namespace

patterns::patterns(const hook::module_info &client,
                   const hook::module_info &engine) noexcept {
    (void)engine;
    auto find_client{[&](const std::string_view pattern) {
        return find_in(client, pattern, missing);
    }};

    // Verified in Ghidra: the pattern is unique in `client_client.so` and
    // lands in the `cmp edi,-1` prologue of `FUN_008f4e10(index)`, which
    // returns `(&DAT_024847e8)[index]`; the client mode's CreateMove slot
    // reads the local player through `FUN_008f4e10(-1)` as well.
    local_player_slot =
        address{find_client("83 FF FF 48 8B 05")}.add(6).abs().as<void *>();

    ZEPHYRUS_INFO("patterns: resolved ({} missing)", missing);
}
} // namespace zephyrus::csgo
