// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński. Linux pattern table only (legacy CS:GO 1.38.8.1 linux64).

#pragma once

#include <zephyrus/hook/hook.hpp>

#include <cstdint>

namespace zephyrus::csgo {
/// Runtime-resolved addresses for the client module.
///
/// Only finders verified against this exact build are kept (see
/// `doc/csgo-re-abi.md` §11): unverified Osiris patterns and the features
/// built on them were removed. Every field is null when the pattern did not
/// match and consumers must degrade gracefully.
struct patterns {
    patterns(const hook::module_info &client,
             const hook::module_info &engine) noexcept;

    /// `C_CSPlayer**` local-player slot (`DAT_024847e8`): the unique
    /// `83 FF FF 48 8B 05` match in `client_client.so` (Ghidra 0x008f4e11)
    /// resolves to the global read by the `FUN_008f4e10(index)` accessor;
    /// `ClientModeCSNormal::CreateMove` itself fetches the local player
    /// through `FUN_008f4e10(-1)`.
    void *local_player_slot{};

    int missing{};
};
} // namespace zephyrus::csgo
