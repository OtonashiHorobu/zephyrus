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
constexpr std::size_t index_should_draw_fog{18};
constexpr std::size_t index_override_view{19};
constexpr std::size_t index_create_move{25};
constexpr std::size_t index_view_model_fov{36};
constexpr std::size_t index_frame_stage_notify{37};
constexpr std::size_t index_dispatch_user_message{38};
constexpr std::size_t index_post_screen_effects{45};
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
    for (const std::size_t index : {index_should_draw_fog, index_override_view,
                                    index_create_move, index_view_model_fov}) {
        if (vtable[index] == nullptr || !hook::module_of(vtable[index])) {
            return std::unexpected{"client mode vtable entry " +
                                   std::to_string(index) +
                                   " is not a valid function pointer"};
        }
    }

    auto should_draw_fog_target{vtable[index_should_draw_fog]};
    auto should_draw_fog_result{hook::function_hook<bool(void *)>::install(
        should_draw_fog_target, [this](void *self) {
            if (skip_fog && skip_fog()) {
                return false;
            }
            return should_draw_fog_hook_.call_original(self);
        })};
    if (!should_draw_fog_result) {
        return std::unexpected{"shouldDrawFog hook failed: " +
                               should_draw_fog_result.error()};
    }
    should_draw_fog_hook_ = std::move(*should_draw_fog_result);

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

    auto view_model_fov_target{vtable[index_view_model_fov]};
    auto view_model_fov_result{hook::function_hook<float(void *)>::install(
        view_model_fov_target, [this](void *self) {
            const float original{view_model_fov_hook_.call_original(self)};
            if (view_model_fov) {
                const float override_value{view_model_fov()};
                if (override_value > 0.0F) {
                    return override_value;
                }
            }
            return original;
        })};
    if (!view_model_fov_result) {
        uninstall();
        return std::unexpected{"getViewModelFov hook failed: " +
                               view_model_fov_result.error()};
    }
    view_model_fov_hook_ = std::move(*view_model_fov_result);

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

        auto message_target{client_vtable[index_dispatch_user_message]};
        if (message_target == nullptr || !hook::module_of(message_target)) {
            uninstall();
            return std::unexpected{
                std::string{"client dispatchUserMessage is not a valid "
                            "function pointer"}};
        }
        auto message_result{
            hook::function_hook<bool(void *, int, int, int, const void *)>::
                install(message_target, [this](void *self, int type,
                                               int passthrough_flags, int size,
                                               const void *data) {
                    const bool result{dispatch_user_message_hook_.call_original(
                        self, type, passthrough_flags, size, data)};
                    if (user_message) {
                        user_message(type, size, data);
                    }
                    return result;
                })};
        if (!message_result) {
            uninstall();
            return std::unexpected{"dispatchUserMessage hook failed: " +
                                   message_result.error()};
        }
        dispatch_user_message_hook_ = std::move(*message_result);
    }

    {
        auto target{vtable[index_post_screen_effects]};
        if (target == nullptr || !hook::module_of(target)) {
            uninstall();
            return std::unexpected{
                std::string{"doPostScreenEffects is not a valid function "
                            "pointer"}};
        }
        auto post_screen_result{hook::function_hook<void(void *, int)>::install(
            target, [this](void *self, int param) {
                if (skip_post_screen_effects && skip_post_screen_effects()) {
                    return;
                }
                post_screen_effects_hook_.call_original(self, param);
            })};
        if (!post_screen_result) {
            uninstall();
            return std::unexpected{"doPostScreenEffects hook failed: " +
                                   post_screen_result.error()};
        }
        post_screen_effects_hook_ = std::move(*post_screen_result);
    }

    ZEPHYRUS_INFO("client mode hooks installed (overrideView/createMove/"
                  "getViewModelFov/frameStageNotify/doPostScreenEffects/"
                  "shouldDrawFog/dispatchUserMessage)");
    return {};
}

void client_mode_hooks::uninstall() noexcept {
    dispatch_user_message_hook_ = {};
    should_draw_fog_hook_ = {};
    post_screen_effects_hook_ = {};
    frame_stage_notify_hook_ = {};
    view_model_fov_hook_ = {};
    create_move_hook_ = {};
    override_view_hook_ = {};
}

hook::result_type
client_mode_hooks::install_client_animation(void *local_player) noexcept {
    if (client_animation_hook_.valid() || local_player == nullptr) {
        return {};
    }
    if (!offset_valid(offsets::update_client_side_animation_vtable)) {
        ZEPHYRUS_INFO(
            "UpdateClientSideAnimation index unknown; animation hook off");
        return {};
    }

    auto **vtable{*reinterpret_cast<void ***>(local_player)};
    const auto index{
        static_cast<std::size_t>(offsets::update_client_side_animation_vtable)};
    void *target{vtable[index]};
    if (target == nullptr || !hook::module_of(target)) {
        return std::unexpected{
            std::string{"UpdateClientSideAnimation is not a valid function "
                        "pointer"}};
    }

    auto result{
        hook::function_hook<void(void *)>::install(target, [this](void *self) {
            client_animation_hook_.call_original(self);
            if (update_client_side_animation) {
                update_client_side_animation(self);
            }
        })};
    if (!result) {
        return std::unexpected{"UpdateClientSideAnimation hook failed: " +
                               result.error()};
    }
    client_animation_hook_ = std::move(*result);
    ZEPHYRUS_INFO("UpdateClientSideAnimation hook installed (index {})", index);
    return {};
}

void client_mode_hooks::uninstall_client_animation() noexcept {
    client_animation_hook_ = {};
}
} // namespace zephyrus::csgo::hooks
