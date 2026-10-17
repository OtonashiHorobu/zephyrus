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
/// Inline hooks on the `ClientMode` / `CHLClient` virtual methods.
///
/// Only slots that were verified against this exact build
/// (`client_client.so`, 1.38.8.1 linux64) are hooked. Ghidra evidence:
///   * `ClientModeCSNormal` constructor at `0x00ff8b20` installs the vtable
///     `0x022d14b0` on the static instance at `0x06e04200` (see
///     `doc/csgo-re-abi.md` §11).
///   * slot 19 = `OverrideView(CViewSetup*)`: takes a single pointer and
///     forwards it to the view subsystem.
///   * slot 25 = `CreateMove(float, CUserCmd*)`: saves the xmm0 float and the
///     rsi pointer, then forwards to the input subsystem.
///   * `CHLClient` vtable `0x021c12c8`, slot 37 (`0x0098bf80`) =
///     `FrameStageNotify(int)` (`switch(stage)` 0..8).
class client_mode_hooks {
  public:
    /// Called after the original `OverrideView` so it may adjust the setup.
    std::function<void(view_setup &)> override_view;
    /// Called after the original `CreateMove` so it may modify the command.
    /// The returned value is the send-packet state (fake lag suppresses it);
    /// the original state is passed in.
    std::function<bool(user_cmd &, bool original_send)> create_move;
    /// Called from `Client::FrameStageNotify` (verified slot 37).
    std::function<void(int stage)> frame_stage_notify;

    [[nodiscard]] hook::result_type install(void *client_mode,
                                            void *client = nullptr) noexcept;
    void uninstall() noexcept;

  private:
    hook::function_hook<void(void *, view_setup *)> override_view_hook_{};
    hook::function_hook<bool(void *, float, user_cmd *)> create_move_hook_{};
    hook::function_hook<void(void *, int)> frame_stage_notify_hook_{};
};
} // namespace zephyrus::csgo::hooks
