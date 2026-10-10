// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <zephyrus/csgo/usercmd.hpp>
#include <zephyrus/csgo/view_setup.hpp>
#include <zephyrus/hook/hook.hpp>

#include <functional>

namespace zephyrus::csgo::hooks {
/// Inline hooks on the `ClientMode` virtual methods.
///
/// The targets are read from the live vtable and patched in place, so every
/// call (from the engine or the game) reaches the callbacks. Linux indices
/// come from the Osiris csgo branch (its Windows counterparts in comments):
///   overrideView      19 (18)
///   createMove        25 (24)
///   getViewModelFov   36 (35)
/// and `Client::frameStageNotify` at 37 (36).
class client_mode_hooks {
  public:
    /// Called after the original `OverrideView` so it may adjust the setup.
    std::function<void(view_setup &)> override_view;
    /// Called after the original `CreateMove` so it may modify the command.
    /// The returned value is the send-packet state (fake lag suppresses it);
    /// the original state is passed in.
    std::function<bool(user_cmd &, bool original_send)> create_move;
    /// Return > 0 to override the view-model FOV.
    std::function<float()> view_model_fov;
    /// Called from `Client::FrameStageNotify` (Linux index 37).
    std::function<void(int stage)> frame_stage_notify;
    /// Return true to skip the original `DoPostScreenEffects` (Linux index
    /// 45), e.g. to remove the scope overlay.
    std::function<bool()> skip_post_screen_effects;
    /// Return true to skip drawing the world fog (`ShouldDrawFog`, Linux
    /// index 18).
    std::function<bool()> skip_fog;
    /// Called after the original `Client::DispatchUserMessage` (Linux index
    /// 38) with (type, size, data).
    std::function<void(int, int, const void *)> user_message;
    /// Called after the original `C_CSPlayer::UpdateClientSideAnimation`
    /// (entity vtable; only installed when the Linux index is known).
    std::function<void(void *)> update_client_side_animation;

    [[nodiscard]] hook::result_type install(void *client_mode,
                                            void *client = nullptr) noexcept;
    void uninstall() noexcept;

    /// Install the entity vtable hook (needs a live player entity to read the
    /// vtable from). Returns an empty result while the Linux vtable index is
    /// unknown.
    [[nodiscard]] hook::result_type
    install_client_animation(void *local_player) noexcept;
    void uninstall_client_animation() noexcept;

  private:
    hook::function_hook<void(void *, view_setup *)> override_view_hook_{};
    hook::function_hook<bool(void *, float, user_cmd *)> create_move_hook_{};
    hook::function_hook<float(void *)> view_model_fov_hook_{};
    hook::function_hook<void(void *, int)> frame_stage_notify_hook_{};
    hook::function_hook<void(void *, int)> post_screen_effects_hook_{};
    hook::function_hook<bool(void *)> should_draw_fog_hook_{};
    hook::function_hook<bool(void *, int, int, int, const void *)>
        dispatch_user_message_hook_{};
    hook::function_hook<void(void *)> client_animation_hook_{};
};
} // namespace zephyrus::csgo::hooks
