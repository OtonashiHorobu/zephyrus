// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/acr/game.hpp>
#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/acr/protocol.hpp>
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

    ok &= resolve_pointer(result.raycube_, symbols::raycube);
    ok &= resolve_pointer(result.entinmap_, symbols::entinmap);
    ok &= resolve_pointer(result.sendposition_, symbols::sendposition);
    ok &= resolve_pointer(result.addmsg_, symbols::addmsg);
    ok &= resolve_pointer(result.c2sinfo_, symbols::c2sinfo);
    ok &= resolve_pointer(result.vecfromyawpitch_, symbols::vecfromyawpitch);

    ok &= resolve_pointer(result.gamemode_, symbols::gamemode);
    ok &= resolve_pointer(result.mutators_, symbols::mutators);
    ok &= resolve_pointer(result.lastmillis_, symbols::lastmillis);
    ok &= resolve_pointer(result.worldhitpos_, symbols::worldhitpos);
    ok &= resolve_pointer(result.gametype_, symbols::gametype);

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

int game::now() const noexcept {
    return lastmillis_ != nullptr ? *lastmillis_ : 0;
}

int game::gamemode() const noexcept {
    return gamemode_ != nullptr ? *gamemode_ : protocol::g_dm;
}

int game::mutators() const noexcept {
    return mutators_ != nullptr ? *mutators_ : 0;
}

int game::mode_implied() const noexcept {
    if (gametype_ == nullptr) {
        return 0;
    }
    const int mode{gamemode()};
    if (mode < 0 || mode >= protocol::g_max) {
        return 0;
    }
    // struct gametypes { int type, implied, mutators[2]; const char *name,
    // *gsp[1]; }
    return memory::read<int>(
        gametype_, static_cast<std::size_t>(mode) *
                           sizeof(protocol::gametypes_entry) +
                       offsetof(protocol::gametypes_entry, implied));
}

int game::spawn_protect_ms() const noexcept {
    const int combined{mutators() | mode_implied()};
    if (combined & protocol::g_m_classic) {
        return protocol::spawn_protect_classic;
    }
    const int mode{gamemode()};
    if (mode >= 0 && mode < protocol::g_max &&
        ((1 << mode) & protocol::flag_modes_mask) != 0) {
        return protocol::spawn_protect_flags;
    }
    if (combined & protocol::g_m_team) {
        return protocol::spawn_protect_team;
    }
    return protocol::spawn_protect_default;
}

vec3 game::crosshair_position() const noexcept {
    if (worldhitpos_ == nullptr) {
        return {};
    }
    return {worldhitpos_[0], worldhitpos_[1], worldhitpos_[2]};
}

float game::raycube(const vec3 &from, const vec3 &direction,
                    vec3 *surface) const noexcept {
    if (raycube_ == nullptr) {
        return 0.0F;
    }
    vec3 scratch{};
    void *target{surface != nullptr ? static_cast<void *>(surface)
                                    : static_cast<void *>(&scratch)};
    return raycube_(from, direction, target);
}

void game::entinmap(void *physent) const noexcept {
    if (entinmap_ != nullptr && physent != nullptr) {
        entinmap_(physent);
    }
}

void game::send_position(void *physent) const noexcept {
    if (sendposition_ != nullptr && physent != nullptr) {
        sendposition_(physent);
    }
}

void game::flush_messages() const noexcept {
    if (c2sinfo_ != nullptr) {
        c2sinfo_(true);
    }
}

void game::message_weapchange(int cn, int gun) const noexcept {
    if (addmsg_ != nullptr) {
        addmsg_(protocol::sv_weapchange, "ri2", cn, gun);
    }
}

void game::message_quickswitch(int cn) const noexcept {
    if (addmsg_ != nullptr) {
        addmsg_(protocol::sv_quickswitch, "ri", cn);
    }
}

void game::message_shootc(int cn, int id, int gun) const noexcept {
    if (addmsg_ != nullptr) {
        addmsg_(protocol::sv_shootc, "ri3", cn, id, gun);
    }
}

void game::message_explode(int cn, int id, int gun, int flags,
                           const vec3 &o) const noexcept {
    if (addmsg_ != nullptr) {
        constexpr float dmf{16.0F};
        addmsg_(protocol::sv_explode, "ri7", cn, id, gun, flags,
                static_cast<int>(o.x * dmf), static_cast<int>(o.y * dmf),
                static_cast<int>(o.z * dmf));
    }
}

void game::vec_from_yaw_pitch(float yaw, int move, int strafe,
                              vec3 &out) const noexcept {
    if (vecfromyawpitch_ == nullptr) {
        out = {};
        return;
    }
    vecfromyawpitch_(yaw, 0.0F, move, strafe, static_cast<void *>(&out));
}
} // namespace zephyrus::acr
