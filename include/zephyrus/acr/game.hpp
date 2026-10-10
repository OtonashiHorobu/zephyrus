// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/entity.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace zephyrus::acr {
/// Read/write access to the running ACR client.
///
/// All addresses are resolved once via `dlsym(RTLD_DEFAULT, ...)`; the client
/// is linked with `-rdynamic`, so every game global and function is available.
class game {
  public:
    game() noexcept = default;

    [[nodiscard]] static std::expected<game, std::string> create() noexcept;
    [[nodiscard]] bool valid() const noexcept { return player1_ != nullptr; }

    [[nodiscard]] player local_player() const noexcept;
    [[nodiscard]] std::vector<player> players() const noexcept;

    [[nodiscard]] vec3 camera_position() const noexcept;
    [[nodiscard]] vec3 camera_direction() const noexcept;

    /// True when the straight line between the two points is not blocked by
    /// world geometry (wraps the game's `raycubelos`).
    [[nodiscard]] bool visible(const vec3 &from, const vec3 &to) const noexcept;

    /// Wraps `worldtoscreen`: normalized screen coordinates (0..1). Returns
    /// the game's flags, bit 0 = behind the camera, bit 2 = invalid.
    [[nodiscard]] int world_to_screen(const vec3 &world, float &x,
                                      float &y) const noexcept;

    void draw_text(int x, int y, int r, int g, int b, int a,
                   const char *text) const noexcept;

    [[nodiscard]] player crosshair_player() const noexcept;
    [[nodiscard]] int crosshair_zone() const noexcept;

    [[nodiscard]] bool add_bot() const noexcept;
    [[nodiscard]] int bot_count() const noexcept;

    // Weapon statistics from the game's `guns` table.
    [[nodiscard]] int gun_spread(int type) const noexcept;
    [[nodiscard]] int gun_spreadrem(int type) const noexcept;
    [[nodiscard]] int gun_attackdelay(int type) const noexcept;
    [[nodiscard]] int gun_damage(int type) const noexcept;
    [[nodiscard]] int gun_mulset(int type) const noexcept;

    /// Server damage model for one hitzone: 3 = head, 2 = torso, 1 = legs.
    [[nodiscard]] float zone_damage(int gun, float distance,
                                    int hitzone) const noexcept;

    // ------------------------------------------------------------------
    // Server-side mirror helpers (spawn protection, protocol, movement).
    // ------------------------------------------------------------------

    /// Current server wall clock (`lastmillis`).
    [[nodiscard]] int now() const noexcept;

    [[nodiscard]] int gamemode() const noexcept;
    [[nodiscard]] int mutators() const noexcept;
    /// `gametype[gamemode].implied` from the game's mode table.
    [[nodiscard]] int mode_implied() const noexcept;
    /// Exact mirror of the `SPAWNPROTECT` macro (1000/1500/2000/1750 ms).
    [[nodiscard]] int spawn_protect_ms() const noexcept;

    /// Crosshair hit position (`worldhitpos`).
    [[nodiscard]] vec3 crosshair_position() const noexcept;

    /// `raycube(o, ray, surface)`: distance to the first world hit.
    [[nodiscard]] float raycube(const vec3 &from, const vec3 &direction,
                                vec3 *surface = nullptr) const noexcept;

    /// `entinmap(physent*)`: push the entity out of solid geometry.
    void entinmap(void *physent) const noexcept;

    /// Send a position update for one player right now.
    void send_position(void *physent) const noexcept;

    /// Flush the queued client messages (`c2sinfo(true)`).
    void flush_messages() const noexcept;

    /// Raw `addmsg` entry point (`void addmsg(int, const char*, ...)`). The
    /// variadic call cannot be forwarded through a wrapper, so callers that
    /// need message types beyond the helpers below invoke it directly.
    using addmsg_fn = void (*)(int, const char *, ...);
    [[nodiscard]] addmsg_fn message_sender() const noexcept {
        return addmsg_;
    }

    // Typed client->server messages used by the exploit actions.
    void message_weapchange(int cn, int gun) const noexcept;
    void message_quickswitch(int cn) const noexcept;
    void message_shootc(int cn, int id, int gun) const noexcept;
    void message_explode(int cn, int id, int gun, int flags,
                         const vec3 &o) const noexcept;

    /// `vecfromyawpitch(yaw, 0, move, strafe, out)`.
    void vec_from_yaw_pitch(float yaw, int move, int strafe,
                            vec3 &out) const noexcept;

  private:
    using raycubelos_fn = bool (*)(const vec3 &, const vec3 &, float);
    using raycube_fn = float (*)(const vec3 &, const vec3 &, void *);
    using entinmap_fn = void (*)(void *);
    using sendposition_fn = void (*)(void *);
    using c2sinfo_fn = void (*)(bool);
    using vecfromyawpitch_fn = void (*)(float, float, int, int, void *);
    using world_to_screen_fn = int (*)(const vec3 &, void *, int);
    using draw_text_fn = void (*)(const char *, int, int, int, int, int, int,
                                  int, int);
    using add_bot_fn = bool (*)();
    using bot_count_fn = int (*)();

    void **player1_{};
    void *players_{};
    void **camera1_{};
    void **worldhit_{};
    int *worldhitzone_{};
    float *camdir_{};
    const std::byte *guns_{};
    const float *muls_{};
    float (*effective_damage_)(int, float, bool, bool){};
    int *gamemode_{};
    int *mutators_{};
    int *lastmillis_{};
    float *worldhitpos_{};
    const std::byte *gametype_{};

    raycubelos_fn visible_{};
    raycube_fn raycube_{};
    entinmap_fn entinmap_{};
    sendposition_fn sendposition_{};
    addmsg_fn addmsg_{};
    c2sinfo_fn c2sinfo_{};
    vecfromyawpitch_fn vecfromyawpitch_{};
    world_to_screen_fn world_to_screen_{};
    draw_text_fn draw_text_{};
    add_bot_fn add_bot_{};
    bot_count_fn bot_count_{};
};
} // namespace zephyrus::acr
