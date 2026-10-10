// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/features/csgo/esp.hpp>
#include <zephyrus/log.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace zephyrus::csgo::features {
namespace {
/// `PlayerInfo` from the engine interface (Linux layout).
struct player_info {
    std::uint64_t version;
    std::uint64_t xuid;
    char name[128];
    int user_id;
    char guid[33];
    std::uint32_t friends_id;
    char friends_name[128];
    bool fake_player;
    bool hltv;
    int custom_files[4];
    unsigned char files_downloaded;
};

constexpr float unit_to_meter{0.01905F};

[[nodiscard]] bool project(const float *matrix, const int width,
                           const int height, const Vector &world,
                           float &screen_x, float &screen_y) noexcept {
    const float w{matrix[12] * world.x + matrix[13] * world.y +
                  matrix[14] * world.z + matrix[15]};
    if (w < 0.001F) {
        return false;
    }
    const float x{(matrix[0] * world.x + matrix[1] * world.y +
                   matrix[2] * world.z + matrix[3]) /
                  w};
    const float y{(matrix[4] * world.x + matrix[5] * world.y +
                   matrix[6] * world.z + matrix[7]) /
                  w};
    screen_x = (static_cast<float>(width) * 0.5F) * (1.0F + x);
    screen_y = (static_cast<float>(height) * 0.5F) * (1.0F - y);
    return true;
}

[[nodiscard]] std::string weapon_label(const entity &player) noexcept {
    const entity weapon{player.active_weapon()};
    if (!weapon.valid()) {
        return {};
    }
    const weapon_info *data{weapon.weapon_data()};
    if (data == nullptr || data->name == nullptr) {
        return {};
    }
    std::string name{data->name};
    // `CCSWeaponBaseVData::m_szName` is a localization token for some
    // classes (e.g. "#SFUI_WPNHUD_HKP2000"); strip it down to the identifier.
    if (name.starts_with("#SFUI_WPNHUD_")) {
        name.erase(0, 13);
    } else if (name.starts_with("#SFUI_")) {
        name.erase(0, 6);
    } else if (name.starts_with('#')) {
        name.erase(0, 1);
    }
    if (constexpr std::string_view prefix{"weapon_"};
        name.starts_with(prefix)) {
        name.erase(0, prefix.size());
    }
    return name;
}
} // namespace

std::vector<esp_drawable> esp::collect(const game &game, const int screen_width,
                                       const int screen_height) const {
    std::vector<esp_drawable> drawables;
    if (!options_.enabled || game.engine() == nullptr) {
        return drawables;
    }

    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return drawables;
    }

    const auto *matrix{static_cast<const float *>(
        virtual_call<37, const void *>(game.engine()))};
    if (matrix == nullptr) {
        return drawables;
    }

    const Vector eye{local.eye_position()};
    const Vector local_origin{local.abs_origin()};
    const int local_team{local.team_number()};

    for (int index{1}; index <= game.max_clients(); ++index) {
        const entity candidate{game.get_entity(index)};
        if (!candidate.valid() || candidate.is_dormant() ||
            candidate.raw() == local.raw() || !candidate.is_alive() ||
            !candidate.is_player()) {
            continue;
        }

        const bool teammate{local_team > 1 &&
                            candidate.team_number() == local_team};

        // Absolute (interpolated) origin: the network `m_vecOrigin` snaps on
        // every update and is not what the renderer draws.
        const Vector origin{candidate.abs_origin()};
        Vector mins{-16.0F, -16.0F, 0.0F};
        Vector maxs{16.0F, 16.0F, 72.0F};
        // Prefer the networked `m_Collision` bounds (stable recv offsets,
        // verified at runtime: CBaseEntity->m_vecMins 0x388 / m_vecMaxs
        // 0x394); fall back to the collideable virtuals when they did not
        // resolve or return a non-vtable object.
        const std::uint32_t mins_offset{
            netvars::get("CBaseEntity->m_vecMins"_nv)};
        const std::uint32_t maxs_offset{
            netvars::get("CBaseEntity->m_vecMaxs"_nv)};
        if (mins_offset != 0 && maxs_offset != 0) {
            mins = *reinterpret_cast<const Vector *>(
                static_cast<const std::byte *>(candidate.raw()) + mins_offset);
            maxs = *reinterpret_cast<const Vector *>(
                static_cast<const std::byte *>(candidate.raw()) + maxs_offset);
        } else if (void *collideable{virtual_call<3, void *>(candidate.raw())};
                   collideable != nullptr) {
            void *collideable_vtable{*reinterpret_cast<void **>(collideable)};
            if (collideable_vtable != nullptr &&
                hook::module_of(collideable_vtable)) {
                mins = virtual_call<1, const Vector &>(collideable);
                maxs = virtual_call<2, const Vector &>(collideable);
            } else {
                static bool logged{};
                if (!logged) {
                    logged = true;
                    ZEPHYRUS_WARN("esp: collideable vtable invalid; using "
                                  "default bounds");
                }
            }
        }

        float left{}, top{}, right{}, bottom{};
        bool any{};
        int corners{};
        // Players behind the camera project with negative/mirrored w: their
        // corners land far below/off the screen and the resulting boxes jump
        // around ("flicker"). Require the origin to be in front of the view
        // plane first, then only accept a sane number of corners.
        float origin_screen_x{};
        float origin_screen_y{};
        if (!project(matrix, screen_width, screen_height, origin,
                     origin_screen_x, origin_screen_y)) {
            continue;
        }
        for (int corner{}; corner < 8; ++corner) {
            const Vector point{origin.x + ((corner & 1) != 0 ? maxs.x : mins.x),
                               origin.y + ((corner & 2) != 0 ? maxs.y : mins.y),
                               origin.z +
                                   ((corner & 4) != 0 ? maxs.z : mins.z)};
            float x{};
            float y{};
            if (!project(matrix, screen_width, screen_height, point, x, y)) {
                continue;
            }
            left = any ? std::min(left, x) : x;
            top = any ? std::min(top, y) : y;
            right = any ? std::max(right, x) : x;
            bottom = any ? std::max(bottom, y) : y;
            any = true;
            ++corners;
        }
        if (!any || corners < 4 || right - left < 1.0F ||
            bottom - top < 1.0F) {
            continue;
        }
        // Discard explosions of the projection near the view plane and boxes
        // that do not touch the screen at all.
        if (right - left > static_cast<float>(screen_width) * 3.0F ||
            bottom - top > static_cast<float>(screen_height) * 3.0F) {
            continue;
        }
        constexpr float margin{96.0F};
        if (right < -margin ||
            left > static_cast<float>(screen_width) + margin ||
            bottom < -margin ||
            top > static_cast<float>(screen_height) + margin) {
            continue;
        }

        esp_drawable drawable{};
        drawable.left = left;
        drawable.top = top;
        drawable.right = right;
        drawable.bottom = bottom;
        drawable.health = std::clamp(
            static_cast<float>(candidate.health()) / 100.0F, 0.0F, 1.0F);
        drawable.distance = (origin - local_origin).length() * unit_to_meter;
        drawable.teammate = teammate;
        drawable.visible = candidate.is_visible(
            game.engine_trace(), eye, origin + Vector{0.0F, 0.0F, 64.0F});

        if (options_.visible_only && !drawable.visible) {
            continue;
        }
        if (teammate && !options_.team) {
            continue;
        }

        if (player_info info{};
            virtual_call<8, bool>(game.engine(), index, &info)) {
            drawable.name = info.name;
        }
        drawable.weapon = weapon_label(candidate);

        drawables.push_back(std::move(drawable));
    }
    return drawables;
}
} // namespace zephyrus::csgo::features
