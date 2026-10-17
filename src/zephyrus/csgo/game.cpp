// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński.

#include <zephyrus/csgo/address.hpp>
#include <zephyrus/csgo/constants.hpp>
#include <zephyrus/csgo/game.hpp>
#include <zephyrus/csgo/interfaces.hpp>
#include <zephyrus/csgo/netvars.hpp>
#include <zephyrus/csgo/usercmd.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>

#include <cstdint>

namespace zephyrus::csgo {
namespace {
// CGlobalVars layout (Linux, Osiris-validated).
constexpr std::ptrdiff_t globals_current_time{16};
constexpr std::ptrdiff_t globals_frametime{20};
constexpr std::ptrdiff_t globals_tick_count{28};
constexpr std::ptrdiff_t globals_interval_per_tick{32};
} // namespace

std::expected<game, std::string> game::create() noexcept {
    auto module{hook::find_module(client_module)};
    if (!module) {
        return std::unexpected{"client module not loaded yet: " +
                               module.error()};
    }
    auto engine_module_info{hook::find_module(engine_module)};
    if (!engine_module_info) {
        return std::unexpected{"engine module not loaded yet: " +
                               engine_module_info.error()};
    }

    auto factory{interface_factory(*module)};
    if (!factory) {
        return std::unexpected{factory.error()};
    }
    auto engine_factory{interface_factory(*engine_module_info)};
    if (!engine_factory) {
        return std::unexpected{engine_factory.error()};
    }

    game instance{};
    instance.client_ = (*factory)("VClient018", nullptr);
    instance.entity_list_ = (*factory)("VClientEntityList003", nullptr);
    instance.engine_ = (*engine_factory)("VEngineClient014", nullptr);
    instance.engine_trace_ = (*engine_factory)("EngineTraceClient004", nullptr);
    instance.physics_surface_props_ =
        (*factory)("VPhysicsSurfaceProps001", nullptr);
    if (instance.physics_surface_props_ == nullptr) {
        // The physics surface props interface can live in vphysics_client.so
        // (Osiris resolves it there).
        if (auto physics{hook::find_module("vphysics_client.so")}; physics) {
            if (auto physics_factory{interface_factory(*physics)};
                physics_factory) {
                instance.physics_surface_props_ =
                    (*physics_factory)("VPhysicsSurfaceProps001", nullptr);
            }
        }
    }
    if (instance.client_ == nullptr || instance.entity_list_ == nullptr ||
        instance.engine_ == nullptr || instance.engine_trace_ == nullptr) {
        std::string missing;
        const auto note_missing = [&missing](const char *name) {
            if (!missing.empty()) {
                missing += ", ";
            }
            missing += name;
        };
        if (instance.client_ == nullptr) {
            note_missing("VClient018");
        }
        if (instance.entity_list_ == nullptr) {
            note_missing("VClientEntityList003");
        }
        if (instance.engine_ == nullptr) {
            note_missing("VEngineClient014");
        }
        if (instance.engine_trace_ == nullptr) {
            note_missing("EngineTraceClient004");
        }
        return std::unexpected{"client/engine interfaces unavailable: " +
                               missing};
    }
    if (instance.physics_surface_props_ == nullptr) {
        ZEPHYRUS_WARN(
            "VPhysicsSurfaceProps001 unavailable (autowall degraded)");
    }

    // Globals live behind rip-relative references inside the client
    // interface's virtual functions (same chains as Osiris' Linux branch).
    auto **vtable{*reinterpret_cast<void ***>(instance.client_)};
    if (vtable == nullptr) {
        return std::unexpected{"client vtable is null"};
    }

    // `[11] + 2` in Osiris' Linux branch is pointer arithmetic on a
    // `uintptr_t*`, i.e. 16 bytes: the disp32 of the `mov rax, [rip+disp]`
    // that loads the global-vars pointer sits at function offset +16.
    instance.global_vars_ =
        address{vtable[11]}.add(16).abs().deref().as<void *>();
    instance.client_mode_ =
        address{vtable[10]}.add(12).abs().add(4).abs().deref().as<void *>();
    instance.input_ = address{vtable[16]}.add(3).abs().deref().as<void *>();

    if (instance.global_vars_ == nullptr || instance.client_mode_ == nullptr) {
        // The engine creates its ClientMode instance after the client module
        // is loaded; the caller retries until both are available.
        return std::unexpected{"client globals not created yet"};
    }
    if (instance.input_ == nullptr) {
        ZEPHYRUS_WARN("client input not resolved yet (optional)");
    }

    instance.patterns_.emplace(*module, *engine_module_info);
    netvars::init(instance.client_);

    return instance;
}

entity game::get_entity(const int index) const noexcept {
    if (entity_list_ == nullptr) {
        return {};
    }
    return entity{virtual_call<3, entity_pod *>(entity_list_, index)};
}

entity game::local_player() const noexcept {
    if (!patterns_.has_value() || patterns_->local_player_slot == nullptr) {
        return {};
    }
    auto **slot{reinterpret_cast<entity_pod **>(
        const_cast<void *>(patterns_->local_player_slot))};
    return entity{*slot};
}

int game::max_clients() const noexcept {
    if (engine_ == nullptr) {
        return 0;
    }
    return virtual_call<20, int>(engine_);
}

trace game::trace_ray(const Vector &from, const Vector &to,
                      const unsigned int mask,
                      entity_pod *skip) const noexcept {
    trace_filter_skip filter{skip};
    return trace_ray(from, to, mask, filter);
}

trace game::trace_ray(const Vector &from, const Vector &to,
                      const unsigned int mask,
                      trace_filter &filter) const noexcept {
    if (engine_trace_ == nullptr) {
        return {};
    }
    const ray shot{from, to};
    return engine_trace_ray(engine_trace_, shot, mask, &filter);
}

int game::point_contents(const Vector &point,
                         const unsigned int mask) const noexcept {
    if (engine_trace_ == nullptr) {
        return 0;
    }
    return engine_point_contents(engine_trace_, point, mask);
}

surface_data *game::surface_props(const int index) const noexcept {
    if (physics_surface_props_ == nullptr || index < 0) {
        return nullptr;
    }
    // `IPhysicsSurfaceProps` Linux vtable (verified in vphysics_client.so):
    //   0/1 dtor, 2 ParseSurfaceData, 3 SurfacePropCount, 4 GetSurfaceIndex,
    //   5 GetPhysicsProperties(index, 4 out pointers), 6 GetSurfaceData(index).
    // Calling slot 5 as `GetSurfaceData` writes the physics floats through
    // garbage output pointers (observed SIGSEGV).
    return virtual_call<6, surface_data *>(
        const_cast<void *>(physics_surface_props_), index);
}

float game::current_time() const noexcept {
    return *reinterpret_cast<float *>(static_cast<std::byte *>(global_vars_) +
                                      globals_current_time);
}

float game::interval_per_tick() const noexcept {
    return *reinterpret_cast<float *>(static_cast<std::byte *>(global_vars_) +
                                      globals_interval_per_tick);
}

float game::frametime() const noexcept {
    return *reinterpret_cast<float *>(static_cast<std::byte *>(global_vars_) +
                                      globals_frametime);
}

void game::set_current_time(const float value) const noexcept {
    *reinterpret_cast<float *>(static_cast<std::byte *>(global_vars_) +
                               globals_current_time) = value;
}

void game::set_frametime(const float value) const noexcept {
    *reinterpret_cast<float *>(static_cast<std::byte *>(global_vars_) +
                               globals_frametime) = value;
}

int game::tick_count() const noexcept {
    return *reinterpret_cast<int *>(static_cast<std::byte *>(global_vars_) +
                                    globals_tick_count);
}

float game::server_time(user_cmd *cmd) const noexcept {
    static int tick{};
    static user_cmd *last_cmd{};
    if (cmd != nullptr) {
        if (last_cmd == cmd) {
            // Several features query the time within one CreateMove; the
            // cached value must not advance between those calls.
            return static_cast<float>(tick) * interval_per_tick();
        }
        if (last_cmd == nullptr || last_cmd->has_been_predicted) {
            tick = local_player().tick_base();
        } else {
            ++tick;
        }
        last_cmd = cmd;
    }
    return static_cast<float>(tick) * interval_per_tick();
}
} // namespace zephyrus::csgo
