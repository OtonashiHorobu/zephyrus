// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/csgo/netvars.hpp>
#include <zephyrus/features/csgo/visuals.hpp>

namespace zephyrus::csgo::features {
void visuals::override_view(view_setup &setup) noexcept {
    if (options_.fov_changer && options_.fov > 1.0F) {
        setup.fov = options_.fov;
    }
}

void visuals::apply_no_flash(const game &game) const noexcept {
    if (!options_.no_flash) {
        return;
    }
    const std::uint32_t offset{netvars::get("CCSPlayer->m_flFlashMaxAlpha"_nv)};
    if (offset == 0) {
        return;
    }
    const entity local{game.local_player()};
    if (!local.valid()) {
        return;
    }
    *reinterpret_cast<float *>(static_cast<std::byte *>(local.raw()) + offset) =
        0.0F;
}
} // namespace zephyrus::csgo::features
