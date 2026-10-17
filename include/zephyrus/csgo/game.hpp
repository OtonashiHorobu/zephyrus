// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#pragma once

#include <zephyrus/csgo/entity.hpp>
#include <zephyrus/csgo/patterns.hpp>

#include <expected>
#include <optional>
#include <string>

namespace zephyrus::csgo {
struct user_cmd;

/// Resolved interfaces, global pointers and helpers of the running client.
///
/// `create()` may be called until the engine has loaded its modules; it
/// returns an error and can simply be retried (the overlay installer thread
/// polls it together with SDL2).
class game {
  public:
    game() noexcept = default;

    [[nodiscard]] static std::expected<game, std::string> create() noexcept;

    [[nodiscard]] bool valid() const noexcept { return client_ != nullptr; }
    [[nodiscard]] bool resolved_valid() const noexcept {
        return patterns_.has_value();
    }

    // --- interfaces ---
    /// `VClient018`.
    [[nodiscard]] void *client() const noexcept { return client_; }
    /// `VClientEntityList003`.
    [[nodiscard]] void *entity_list() const noexcept { return entity_list_; }
    /// `CCSClientMode`.
    [[nodiscard]] void *client_mode() const noexcept { return client_mode_; }
    /// `CInput`.
    [[nodiscard]] void *input() const noexcept { return input_; }
    /// `CGlobalVars`.
    [[nodiscard]] void *global_vars() const noexcept { return global_vars_; }
    /// `VEngineClient014`.
    [[nodiscard]] void *engine() const noexcept { return engine_; }
    /// `EngineTraceClient004`.
    [[nodiscard]] void *engine_trace() const noexcept { return engine_trace_; }
    /// `VPhysicsSurfaceProps001` (client module), if the game exposes it.
    [[nodiscard]] void *physics_surface_props() const noexcept {
        return physics_surface_props_;
    }

    [[nodiscard]] const patterns &resolved() const noexcept {
        return *patterns_;
    }

    // --- entities ---
    [[nodiscard]] entity get_entity(int index) const noexcept;
    [[nodiscard]] entity local_player() const noexcept;
    [[nodiscard]] int max_clients() const noexcept;

    /// `IEngineTrace::TraceRay` with an optional entity to skip (`nullptr`
    /// hits everything).
    [[nodiscard]] trace trace_ray(const Vector &from, const Vector &to,
                                  unsigned int mask,
                                  entity_pod *skip = nullptr) const noexcept;
    /// `IEngineTrace::TraceRay` with a caller-provided filter.
    [[nodiscard]] trace trace_ray(const Vector &from, const Vector &to,
                                  unsigned int mask,
                                  trace_filter &filter) const noexcept;
    /// `IEngineTrace::GetPointContents`.
    [[nodiscard]] int point_contents(const Vector &point,
                                     unsigned int mask) const noexcept;
    /// `IPhysicsSurfaceProps::GetSurfaceData` (`nullptr` when the interface
    /// did not resolve).
    [[nodiscard]] struct surface_data *surface_props(int index) const noexcept;

    // --- global vars ---
    [[nodiscard]] float current_time() const noexcept;
    [[nodiscard]] float interval_per_tick() const noexcept;
    [[nodiscard]] float frametime() const noexcept;
    [[nodiscard]] int tick_count() const noexcept;
    [[nodiscard]] float server_time(user_cmd *cmd = nullptr) const noexcept;
    void set_current_time(float value) const noexcept;
    void set_frametime(float value) const noexcept;

  private:
    void *client_{};
    void *entity_list_{};
    void *client_mode_{};
    void *input_{};
    void *global_vars_{};
    void *engine_{};
    void *engine_trace_{};
    void *physics_surface_props_{};
    std::optional<patterns> patterns_{};
};
} // namespace zephyrus::csgo
