// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#include <zephyrus/csgo/hooks/client_mode_hooks.hpp>
#include <zephyrus/csgo/offsets.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>

namespace zephyrus::csgo::hooks {
namespace {
// Verified against `client_client.so` (1.38.8.1): the ClientModeCSNormal
// constructor at 0x00ff8b20 installs this vtable on the instance at
// 0x06e04200; slot 19 takes a view-setup pointer, slot 25 takes
// (float, CUserCmd*). See doc/csgo-re-abi.md §11.
constexpr std::size_t index_override_view{19};
constexpr std::size_t index_create_move{25};
// `CHLClient` vtable 0x021c12c8 slot 37 = FrameStageNotify (doc §5).
constexpr std::size_t index_frame_stage_notify{37};
} // namespace

hook::result_type client_mode_hooks::install(void *client_mode,
                                             void *client) noexcept {
    if (client_mode == nullptr) {
        return std::unexpected{std::string{"null client mode"}};
    }

    auto **vtable{*reinterpret_cast<void ***>(client_mode)};
    if (vtable == nullptr) {
        return std::unexpected{std::string{"null client mode vtable"}};
    }

    // Every target must live inside a loaded module: patching a bogus
    // pointer (stale vtable chain) would corrupt random code.
    for (const std::size_t index :
         {index_override_view, index_create_move}) {
        if (vtable[index] == nullptr || !hook::module_of(vtable[index])) {
            return std::unexpected{"client mode vtable entry " +
                                   std::to_string(index) +
                                   " is not a valid function pointer"};
        }
    }

    auto override_view_target{vtable[index_override_view]};
    auto override_view_result{
        hook::function_hook<void(void *, view_setup *)>::install(
            override_view_target, [this](void *self, view_setup *setup) {
                override_view_hook_.call_original(self, setup);
                if (setup != nullptr && override_view) {
                    override_view(*setup);
                }
            })};
    if (!override_view_result) {
        return std::unexpected{"overrideView hook failed: " +
                               override_view_result.error()};
    }
    override_view_hook_ = std::move(*override_view_result);

    auto create_move_target{vtable[index_create_move]};
    auto create_move_result{
        hook::function_hook<bool(void *, float, user_cmd *)>::install(
            create_move_target,
            [this](void *self, float sample_time, user_cmd *cmd) {
                const bool result{
                    create_move_hook_.call_original(self, sample_time, cmd)};
                if (cmd != nullptr && create_move) {
                    return create_move(*cmd, result);
                }
                return result;
            })};
    if (!create_move_result) {
        uninstall();
        return std::unexpected{"createMove hook failed: " +
                               create_move_result.error()};
    }
    create_move_hook_ = std::move(*create_move_result);

    if (client != nullptr) {
        auto **client_vtable{*reinterpret_cast<void ***>(client)};
        auto target{client_vtable[index_frame_stage_notify]};
        if (target == nullptr || !hook::module_of(target)) {
            uninstall();
            return std::unexpected{
                std::string{"client frameStageNotify is not a valid function "
                            "pointer"}};
        }
        auto frame_stage_result{hook::function_hook<void(void *, int)>::install(
            target, [this](void *self, int stage) {
                frame_stage_notify_hook_.call_original(self, stage);
                if (frame_stage_notify) {
                    frame_stage_notify(stage);
                }
            })};
        if (!frame_stage_result) {
            uninstall();
            return std::unexpected{"frameStageNotify hook failed: " +
                                   frame_stage_result.error()};
        }
        frame_stage_notify_hook_ = std::move(*frame_stage_result);
    }

    ZEPHYRUS_INFO("client hooks installed (overrideView/createMove/"
                  "frameStageNotify)");
    return {};
}

void client_mode_hooks::uninstall() noexcept {
    frame_stage_notify_hook_ = {};
    create_move_hook_ = {};
    override_view_hook_ = {};
}
} // namespace zephyrus::csgo::hooks
