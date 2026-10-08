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

  private:
    using raycubelos_fn = bool (*)(const vec3 &, const vec3 &, float);
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

    raycubelos_fn visible_{};
    world_to_screen_fn world_to_screen_{};
    draw_text_fn draw_text_{};
    add_bot_fn add_bot_{};
    bot_count_fn bot_count_{};
};
} // namespace zephyrus::acr
