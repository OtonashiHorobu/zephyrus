// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/view_setup.hpp>

namespace zephyrus::csgo::features {
/// Visual helpers driven from the `ClientMode` hooks.
///
/// The no-scope/no-fog hooks were removed: their vtable slots could not be
/// verified against this build (slot 18 is a plain `return true` predicate and
/// skipping `DoPostScreenEffects` does not hide the scope overlay), and both
/// mutations were unsafe. Only the verified `OverrideView` hook (world FOV)
/// and the netvar-based no-flash remain.
class visuals {
  public:
    struct visuals_options {
        /// Override the world FOV (`OverrideView`, verified slot 19).
        bool fov_changer{false};
        float fov{90.0F};
        /// Keep the flash overlay fully transparent (`m_flFlashMaxAlpha`).
        bool no_flash{false};
    };

    [[nodiscard]] visuals_options &options() noexcept { return options_; }
    [[nodiscard]] const visuals_options &options() const noexcept {
        return options_;
    }

    void override_view(view_setup &setup) noexcept;

    /// Zero the local player's flash alpha (called every command).
    void apply_no_flash(const game &game) const noexcept;

  private:
    visuals_options options_{};
};
} // namespace zephyrus::csgo::features
