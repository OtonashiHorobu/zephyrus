// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <cstddef>

namespace zephyrus::csgo {
/// `CViewSetup` subset passed to `ClientMode::OverrideView`. The field offsets
/// are shared between platforms (only `fov` is used so far).
struct view_setup {
    std::byte pad0[172];
    void *csm{};
    float fov{};
    std::byte pad1[32];
    float far_z{};
};
} // namespace zephyrus::csgo
