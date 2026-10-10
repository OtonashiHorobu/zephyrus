// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Engine prediction: runs the game's own movement code on the modified user
// command so features see the resulting velocity/flags, then restores the
// entity to the predicted frame (the later engine prediction must be the only
// one that sticks). Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/csgo/offsets.hpp>
#include <zephyrus/features/csgo/prediction.hpp>
#include <zephyrus/log.hpp>

namespace zephyrus::csgo::features {
namespace {
/// IPrediction virtuals (Linux-validated by the Osiris csgo branch).
constexpr std::size_t prediction_update{3};
constexpr std::size_t prediction_check_moving_ground{18};
constexpr std::size_t prediction_setup_move{20};
constexpr std::size_t prediction_finish_move{21};
/// CGameMovement virtuals.
constexpr std::size_t movement_process{1};
constexpr std::size_t movement_reset{2};
constexpr std::size_t movement_start_track{3};
constexpr std::size_t movement_finish_track{4};
/// IMoveHelper virtuals.
constexpr std::size_t move_helper_set_host{1};
constexpr std::size_t move_helper_process_impacts{4};

[[nodiscard]] int client_state_int(const game &game,
                                   const std::ptrdiff_t offset) noexcept {
    if (!game.resolved_valid() || offset == 0 ||
        game.resolved().client_state == nullptr) {
        return 0;
    }
    return *reinterpret_cast<int *>(
        static_cast<std::byte *>(game.resolved().client_state) + offset);
}
} // namespace

bool prediction::available(const game &game) noexcept {
    return game.prediction() != nullptr && game.game_movement() != nullptr &&
           game.resolved_valid() &&
           game.resolved().restore_entity_to_predicted_frame != nullptr &&
           game.resolved().move_data != nullptr &&
           game.resolved().move_helper != nullptr;
}

void prediction::update(const game &game) noexcept {
    if (!available(game)) {
        return;
    }
    const int delta_tick{
        client_state_int(game, offsets::client_state_delta_tick)};
    const int start{
        client_state_int(game, offsets::client_state_last_command_ack)};
    const int stop{
        client_state_int(game, offsets::client_state_last_outgoing_command) +
        client_state_int(game, offsets::client_state_choked_commands)};
    virtual_call<prediction_update, void>(game.prediction(), delta_tick,
                                          delta_tick > 0, start, stop);
}

void prediction::run(const game &game, user_cmd &cmd) noexcept {
    if (running_ || !available(game)) {
        return;
    }

    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return;
    }

    running_ = true;
    old_current_time_ = game.current_time();
    old_frametime_ = game.frametime();

    game.set_current_time(game.server_time(&cmd));
    game.set_frametime(game.interval_per_tick());

    if (int *seed{game.resolved().prediction_random_seed}; seed != nullptr) {
        *seed = 0;
    }
    if (int *player{game.resolved().prediction_player}; player != nullptr) {
        *player =
            static_cast<int>(reinterpret_cast<std::uintptr_t>(local.raw()));
    }

    void *move_data{game.resolved().move_data};
    void *move_helper{game.resolved().move_helper};

    virtual_call<movement_start_track, void>(game.game_movement(), local.raw());
    virtual_call<prediction_check_moving_ground, void>(
        game.prediction(), local.raw(), static_cast<double>(game.frametime()));
    virtual_call<move_helper_set_host, void>(move_helper, local.raw());
    virtual_call<prediction_setup_move, void>(game.prediction(), local.raw(),
                                              &cmd, move_helper, move_data);
    virtual_call<movement_process, void>(game.game_movement(), local.raw(),
                                         move_data);
    virtual_call<prediction_finish_move, void>(game.prediction(), local.raw(),
                                               &cmd, move_data);
    virtual_call<move_helper_process_impacts, void>(move_helper);
    virtual_call<movement_finish_track, void>(game.game_movement(),
                                              local.raw());
    virtual_call<movement_reset, void>(game.game_movement());
    virtual_call<move_helper_set_host, void>(move_helper, nullptr);

    if (int *seed{game.resolved().prediction_random_seed}; seed != nullptr) {
        *seed = -1;
    }
    if (int *player{game.resolved().prediction_player}; player != nullptr) {
        *player = 0;
    }

    velocity_ = local.velocity();
    flags_ = local.flags();
}

void prediction::restore(const game &game) noexcept {
    if (!running_) {
        return;
    }
    running_ = false;

    // Undo the manual simulation: the engine predicts the command again after
    // CreateMove returns, and only that result may persist.
    if (game.resolved_valid() &&
        game.resolved().restore_entity_to_predicted_frame != nullptr) {
        using restore_fn = void (*)(int, int);
        reinterpret_cast<restore_fn>(
            game.resolved().restore_entity_to_predicted_frame)(
            0, game.tick_count());
    }

    game.set_current_time(old_current_time_);
    game.set_frametime(old_frametime_);
}
} // namespace zephyrus::csgo::features
