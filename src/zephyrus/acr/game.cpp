// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/acr/game.hpp>
#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/acr/symbols.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>

#include <cstring>
#include <string>

namespace zephyrus::acr {
namespace {
template <typename T> struct resolved {
    T value{};
    std::string name;
};

std::expected<void *, std::string> require(std::string_view symbol) {
    auto address{hook::resolve(symbol)};
    if (!address) {
        return std::unexpected{"missing game symbol " +
                               std::string{symbol} + ": " + address.error()};
    }
    return *address;
}

template <typename T>
[[nodiscard]] bool resolve_pointer(T &target, std::string_view symbol) {
    auto address{require(symbol)};
    if (!address) {
        ZEPHYRUS_ERROR("{}", address.error());
        return false;
    }
    target = reinterpret_cast<T>(*address);
    return true;
}
} // namespace

std::expected<game, std::string> game::create() noexcept {
    game result{};
    bool ok{true};

    ok &= resolve_pointer(result.player1_, symbols::player1);
    ok &= resolve_pointer(result.players_, symbols::players);
    ok &= resolve_pointer(result.camera1_, symbols::camera1);
    ok &= resolve_pointer(result.worldhit_, symbols::worldhit);
    ok &= resolve_pointer(result.worldhitzone_, symbols::worldhitzone);
    ok &= resolve_pointer(result.camdir_, symbols::camdir);
    ok &= resolve_pointer(result.guns_, symbols::guns);
    ok &= resolve_pointer(result.muls_, symbols::muls);
    ok &= resolve_pointer(result.effective_damage_, symbols::effective_damage);

    ok &= resolve_pointer(result.visible_, symbols::raycubelos);
    ok &= resolve_pointer(result.world_to_screen_, symbols::world_to_screen);
    ok &= resolve_pointer(result.draw_text_, symbols::draw_text);
    ok &= resolve_pointer(result.add_bot_, symbols::add_bot);
    ok &= resolve_pointer(result.bot_count_, symbols::bot_count);

    if (!ok) {
        return std::unexpected{"failed to resolve one or more ACR symbols"};
    }

    ZEPHYRUS_INFO("ACR bindings ready (player1={:#x}, players={:#x})",
                  reinterpret_cast<std::uintptr_t>(result.player1_),
                  reinterpret_cast<std::uintptr_t>(result.players_));
    return result;
}

player game::local_player() const noexcept {
    return player{player1_ != nullptr ? *player1_ : nullptr};
}

std::vector<player> game::players() const noexcept {
    std::vector<player> result;
    if (players_ == nullptr) {
        return result;
    }

    const auto capacity{
        memory::read<int>(players_, offsets::vector_alen)};
    const auto length{memory::read<int>(players_, offsets::vector_ulen)};
    if (capacity <= 0 || length <= 0 || length > capacity ||
        length > 256) {
        return result;
    }

    auto **buffer{memory::read<void **>(players_, offsets::vector_buf)};
    if (buffer == nullptr) {
        return result;
    }

    result.reserve(static_cast<std::size_t>(length));
    for (int i{}; i < length; ++i) {
        result.emplace_back(buffer[i]);
    }
    return result;
}

vec3 game::camera_position() const noexcept {
    const void *camera{camera1_ != nullptr ? *camera1_ : nullptr};
    if (camera == nullptr) {
        return {};
    }
    return {memory::read<float>(camera, offsets::physent_o + offsets::vec_x),
            memory::read<float>(camera, offsets::physent_o + offsets::vec_y),
            memory::read<float>(camera,
                                offsets::physent_o + offsets::vec_z)};
}

vec3 game::camera_direction() const noexcept {
    if (camdir_ == nullptr) {
        return {};
    }
    return {camdir_[0], camdir_[1], camdir_[2]};
}

bool game::visible(const vec3 &from, const vec3 &to) const noexcept {
    if (visible_ == nullptr) {
        return false;
    }
    return visible_(from, to, 0.0f);
}

int game::world_to_screen(const vec3 &world, float &x,
                          float &y) const noexcept {
    if (world_to_screen_ == nullptr) {
        return 4;
    }
    struct {
        float x;
        float y;
    } screen{};
    const int flags{world_to_screen_(world, &screen, 0)};
    x = screen.x;
    y = screen.y;
    return flags;
}

void game::draw_text(int x, int y, int r, int g, int b, int a,
                     const char *text) const noexcept {
    if (draw_text_ != nullptr && text != nullptr) {
        draw_text_(text, x, y, r, g, b, a, -1, -1);
    }
}

player game::crosshair_player() const noexcept {
    return player{worldhit_ != nullptr ? *worldhit_ : nullptr};
}

int game::crosshair_zone() const noexcept {
    return worldhitzone_ != nullptr ? *worldhitzone_ : -1;
}

bool game::add_bot() const noexcept {
    return add_bot_ != nullptr && add_bot_();
}

int game::bot_count() const noexcept {
    return bot_count_ != nullptr ? bot_count_() : 0;
}

namespace {
constexpr std::size_t guninfo_attackdelay = 266;
constexpr std::size_t guninfo_damage = 268;
constexpr std::size_t guninfo_spread = 278;
constexpr std::size_t guninfo_spreadrem = 280;
constexpr std::size_t guninfo_mulset = 304;
} // namespace

int game::gun_spread(int type) const noexcept {
    if (guns_ == nullptr || type < 0 || type >= 19) {
        return 0;
    }
    return memory::read<short>(guns_ + type * 308, guninfo_spread);
}

int game::gun_spreadrem(int type) const noexcept {
    if (guns_ == nullptr || type < 0 || type >= 19) {
        return 0;
    }
    return memory::read<short>(guns_ + type * 308, guninfo_spreadrem);
}

int game::gun_attackdelay(int type) const noexcept {
    if (guns_ == nullptr || type < 0 || type >= 19) {
        return 0;
    }
    return memory::read<short>(guns_ + type * 308, guninfo_attackdelay);
}

int game::gun_damage(int type) const noexcept {
    if (guns_ == nullptr || type < 0 || type >= 19) {
        return 0;
    }
    return memory::read<short>(guns_ + type * 308, guninfo_damage);
}

int game::gun_mulset(int type) const noexcept {
    if (guns_ == nullptr || type < 0 || type >= 19) {
        return 0;
    }
    return memory::read<int>(guns_ + type * 308, guninfo_mulset);
}

float game::zone_damage(int gun, float distance, int hitzone) const noexcept {
    if (effective_damage_ == nullptr) {
        return static_cast<float>(gun_damage(gun));
    }
    float damage{effective_damage_(gun, distance, false, true)};
    if (muls_ == nullptr) {
        return damage;
    }
    const int mulset{gun_mulset(gun)};
    // struct mul { float torso, head; }
    const float torso{muls_[mulset * 2]};
    const float head{muls_[mulset * 2 + 1]};
    damage *= hitzone == 3 ? head : torso;
    return damage;
}
} // namespace zephyrus::acr
