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
class visuals {
  public:
    struct visuals_options {
        /// Override the world FOV (`OverrideView`).
        bool fov_changer{false};
        float fov{90.0F};
        /// Override the view-model FOV (`GetViewModelFov`).
        bool view_model_fov{false};
        float view_model_fov_value{68.0F};
        /// Keep the flash overlay fully transparent.
        bool no_flash{false};
        /// Skip the scope overlay while zoomed (`DoPostScreenEffects`).
        bool remove_scope{false};
        /// Skip the world fog (`ShouldDrawFog`).
        bool remove_fog{false};
    };

    [[nodiscard]] visuals_options &options() noexcept { return options_; }
    [[nodiscard]] const visuals_options &options() const noexcept {
        return options_;
    }

    void override_view(view_setup &setup) noexcept;

    /// Zero the local player's flash alpha (called every command).
    void apply_no_flash(const game &game) const noexcept;

    /// > 0 replaces the engine's view-model FOV; 0 keeps the original.
    [[nodiscard]] float view_model_fov() const noexcept;

  private:
    visuals_options options_{};
};
} // namespace zephyrus::csgo::features
