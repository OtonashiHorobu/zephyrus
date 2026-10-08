// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/features/rage.hpp>

#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/acr/symbols.hpp>
#include <zephyrus/acr/weapons.hpp>
#include <zephyrus/log.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace zephyrus::features {
namespace {
constexpr int hit_head{3};
constexpr int hit_torso{2};
constexpr int hit_leg{1};

constexpr float head_size{0.4F};
constexpr float player_radius{1.1F};
constexpr float player_height{4.5F};
constexpr float leg_part{0.65F};

constexpr int perk2_steady{7};

std::uint64_t now_ms() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start{clock::now()};
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
                                                              start)
            .count());
}

float length(const acr::vec3 &value) {
    return std::sqrt(value.x * value.x + value.y * value.y +
                     value.z * value.z);
}

acr::vec3 normalize(const acr::vec3 &value) {
    const float len{length(value)};
    if (len <= 1.0e-6F) {
        return {};
    }
    return {value.x / len, value.y / len, value.z / len};
}

float dot(const acr::vec3 &a, const acr::vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
} // namespace

std::expected<void, std::string>
rage::initialize(const acr::game &game) noexcept {
    game_ = &game;

    auto shoot{hook::function_hook<void(void *, void *)>::install(
        acr::symbols::shoot, [this](void *player, void *target) {
            on_shoot(player, target);
        })};
    if (!shoot) {
        return std::unexpected{"failed to hook shoot(): " + shoot.error()};
    }
    shoot_hook_ = std::move(*shoot);

    auto sendshoot{
        hook::function_hook<void(void *, const void *)>::install(
            acr::symbols::sendshoot, [this](void *weapon, const void *to) {
                on_sendshoot(weapon, to);
            })};
    if (!sendshoot) {
        return std::unexpected{"failed to hook sendshoot(): " +
                               sendshoot.error()};
    }
    sendshoot_hook_ = std::move(*sendshoot);

    if (auto scope{hook::resolve(acr::symbols::setscope)}; scope) {
        scope_ = reinterpret_cast<void (*)(bool)>(*scope);
    } else {
        ZEPHYRUS_WARN("rage: setscope unavailable: {}", scope.error());
    }

    ZEPHYRUS_INFO("rage hooks installed");
    return {};
}

void rage::shutdown() noexcept {
    shoot_hook_ = {};
    sendshoot_hook_ = {};
    target_ = {};
}

rage_profile &rage::profile_slot(int gun) noexcept {
    const int index{std::clamp(gun, 0, acr::gun_count - 1)};
    return weapons_[static_cast<std::size_t>(index)];
}

const rage_profile &rage::profile_for(int gun) const noexcept {
    const int index{std::clamp(gun, 0, acr::gun_count - 1)};
    const rage_profile &candidate{weapons_[static_cast<std::size_t>(index)]};
    return candidate.customized ? candidate : global_;
}

const rage_profile &rage::active_profile() const noexcept {
    if (game_ == nullptr || !game_->valid()) {
        return global_;
    }
    return profile_for(game_->local_player().weapon_type());
}

rage_profile rage::load_profile(int index) const noexcept {
    if (index < 0 || index >= acr::gun_count) {
        return global_;
    }
    const rage_profile &slot{weapons_[static_cast<std::size_t>(index)]};
    return slot.customized ? slot : global_;
}

void rage::store_profile(int index, const rage_profile &profile) noexcept {
    if (index < 0 || index >= acr::gun_count) {
        global_ = profile;
        global_.customized = false;
        return;
    }
    weapons_[static_cast<std::size_t>(index)] = profile;
    weapons_[static_cast<std::size_t>(index)].customized = true;
}

void rage::select_target(const rage_profile &profile) noexcept {
    target_ = {};

    if (game_ == nullptr || !game_->valid()) {
        return;
    }

    const acr::player local{game_->local_player()};
    if (!local.valid() || !local.alive()) {
        return;
    }

    const acr::vec3 camera{game_->camera_position()};
    const acr::vec3 view{normalize(game_->camera_direction())};
    const float cos_fov{
        std::cos(profile.fov * 0.5F * 3.14159265358979323846F / 180.0F)};

    float best_score{-1.0e30F};
    acr::player best{};

    static std::size_t detail_counter{};
    const bool detail{(++detail_counter % 480) == 1};
    int detailed{};

    for (const acr::player &entity : game_->players()) {
        if (!entity.valid() || !entity.alive() ||
            entity.base() == local.base() || entity.spectating()) {
            continue;
        }
        const acr::vec3 point{entity.head_valid() ? entity.head()
                                                  : entity.origin()};
        const acr::vec3 offset{point.x - camera.x, point.y - camera.y,
                               point.z - camera.z};
        const float distance{length(offset)};
        const float alignment{
            distance > 0.01F ? dot(normalize(offset), view) : 0.0F};
        const bool clear{game_->visible(camera, point)};

        if (detail && detailed < 4) {
            ++detailed;
            ZEPHYRUS_TRACE(
                "rage detail: cn={} team={} local={} d={:.1f} align={:.2f} vis={} same_team={}",
                entity.clientnum(), entity.team(), local.team(),
                static_cast<double>(distance),
                static_cast<double>(alignment), clear,
                entity.team() == local.team());
        }

        if (profile.team_check && entity.team() == local.team() &&
            (local.team() == acr::offsets::team_cla ||
             local.team() == acr::offsets::team_rvsf)) {
            continue;
        }
        if (distance > profile.max_distance || distance <= 0.01F) {
            continue;
        }
        if (alignment < cos_fov) {
            continue;
        }
        if (profile.require_visible && !clear) {
            continue;
        }

        float score{};
        switch (profile.priority) {
        case rage_priority::distance:
            score = -distance;
            break;
        case rage_priority::health:
            score = -static_cast<float>(entity.health()) - distance * 1.0e-4F;
            break;
        case rage_priority::fov:
        default:
            score = alignment - distance * 1.0e-4F;
            break;
        }
        if (score > best_score) {
            best_score = score;
            best = entity;
        }
    }

    target_ = best;

    static std::size_t frame{};
    if ((++frame % 480) == 1) {
        const acr::vec3 cam{game_->camera_position()};
        ZEPHYRUS_TRACE(
            "rage: alive={} gun={} players={} target={} hp={} fov={:.0f} cam=({:.1f},{:.1f},{:.1f}) eye=({:.1f},{:.1f},{:.1f})",
            local.alive(), local.weapon_type(), game_->players().size(),
            best.valid(), local.health(), static_cast<double>(profile.fov),
            static_cast<double>(cam.x), static_cast<double>(cam.y),
            static_cast<double>(cam.z), static_cast<double>(local.origin().x),
            static_cast<double>(local.origin().y),
            static_cast<double>(local.origin().z));
    }
}

int rage::resolve_hitbox(const rage_profile &profile) const noexcept {
    switch (profile.hitbox) {
    case rage_hitbox::head:
        return hit_head;
    case rage_hitbox::chest:
    case rage_hitbox::stomach:
        return hit_torso;
    case rage_hitbox::legs:
        return hit_leg;
    case rage_hitbox::lethal:
        break;
    }

    // Lethal: pick the zone that needs the fewest hits, preferring the
    // larger hitboxes on ties.
    if (game_ == nullptr || !game_->valid() || !target_.valid()) {
        return hit_head;
    }
    const acr::player local{game_->local_player()};
    const int gun{local.weapon_type()};
    const float distance{
        length({target_.origin().x - game_->camera_position().x,
                target_.origin().y - game_->camera_position().y,
                target_.origin().z - game_->camera_position().z})};
    const float health{static_cast<float>(std::max(target_.health(), 1))};

    struct candidate {
        int zone;
        float preference;
    };
    const candidate candidates[]{{hit_torso, 0.0F},
                                 {hit_leg, 1.0F},
                                 {hit_head, 2.0F}};
    int best_zone{hit_head};
    float best_shots{1.0e9F};
    float best_preference{1.0e9F};
    for (const auto &[zone, preference] : candidates) {
        const float damage{game_->zone_damage(gun, distance, zone)};
        if (damage <= 0.0F) {
            continue;
        }
        const float shots{std::ceil(health / damage)};
        if (shots < best_shots ||
            (shots == best_shots && preference < best_preference)) {
            best_shots = shots;
            best_preference = preference;
            best_zone = zone;
        }
    }
    return best_zone;
}

acr::vec3 rage::aim_point(const rage_profile &profile) const noexcept {
    if (!target_.valid()) {
        return {};
    }
    const int zone{resolve_hitbox(profile)};
    const acr::vec3 eye{target_.origin()};
    const float eye_height{target_.eyeheight()};
    const float above_eye{std::max(target_.aboveeye(), 0.1F)};

    switch (zone) {
    case hit_head:
        if (target_.head_valid()) {
            return target_.head();
        }
        return {eye.x, eye.y, eye.z + above_eye * 0.5F};
    case hit_torso:
        return {eye.x, eye.y, eye.z - eye_height * 0.18F};
    case hit_leg:
    default: {
        const acr::vec3 feet{target_.feet()};
        return {feet.x, feet.y, feet.z + (eye_height + above_eye) * 0.28F};
    }
    }
}

int rage::estimate_hitchance(const rage_profile &profile) const noexcept {
    if (game_ == nullptr || !game_->valid() || !target_.valid()) {
        return 0;
    }
    const acr::player local{game_->local_player()};
    if (!local.valid()) {
        return 0;
    }
    const int gun{local.weapon_type()};
    const int base_spread{game_->gun_spread(gun)};
    if (base_spread <= 1) {
        return 100;
    }

    const acr::vec3 velocity{local.velocity()};
    const float speed{
        std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y)};
    const float crouch_factor{local.crouching() ? 0.75F : 1.0F};
    const float spread{static_cast<float>(base_spread) *
                       (speed / 3.0F + 0.4F) * 1.2F * crouch_factor};
    const float reduction{
        static_cast<float>(game_->gun_spreadrem(gun)) / 100.0F};
    const float ads{local.scoping() ? (1.0F - reduction) : 1.0F};
    const float steady{local.perk2() == perk2_steady ? 0.75F : 1.0F};

    // The server adds a uniform world-space offset of spread/2 * factor to
    // `to`, with factor = distance/1000 * perk * ads. The resulting angular
    // half-angle per axis is therefore spread * perk * ads / 2000.
    const float theta{spread * steady * ads / 2000.0F};
    if (theta <= 1.0e-5F) {
        return 100;
    }

    const acr::vec3 camera{game_->camera_position()};
    const acr::vec3 point{aim_point(profile)};
    const float distance{length({point.x - camera.x, point.y - camera.y,
                                 point.z - camera.z})};
    if (distance <= 0.1F) {
        return 100;
    }
    const int zone{resolve_hitbox(profile)};
    const float radius{zone == hit_head ? head_size : player_radius};
    const float angular_radius{radius / distance};

    // Fraction of the uniform spread square that falls inside the target
    // disc (17x17 grid).
    constexpr int grid{8};
    int inside{};
    int total{};
    for (int i{-grid}; i <= grid; ++i) {
        for (int j{-grid}; j <= grid; ++j) {
            const float dx{theta * static_cast<float>(i) / grid};
            const float dy{theta * static_cast<float>(j) / grid};
            ++total;
            if (dx * dx + dy * dy <= angular_radius * angular_radius) {
                ++inside;
            }
        }
    }
    static std::size_t hc_log{};
    if ((++hc_log % 480) == 1) {
        ZEPHYRUS_TRACE(
            "hc: gun={} base={} spread={:.1f} theta={:.5f} r_ang={:.5f} dist={:.1f} {}/{}",
            gun, base_spread, static_cast<double>(spread),
            static_cast<double>(theta), static_cast<double>(angular_radius),
            static_cast<double>(distance), inside, total);
    }
    return total > 0 ? inside * 100 / total : 0;
}

void rage::set_block_reason(std::string reason) noexcept {
    last_block_reason_ = std::move(reason);
}

void rage::record_shot(int weapon, int hitchance, float distance) noexcept {
    shot_record record{};
    record.millis = now_ms();
    record.weapon = weapon;
    record.hitchance = hitchance;
    record.distance = distance;
    record.fired = true;
    if (target_.valid()) {
        record.target_clientnum = target_.clientnum();
        record.target_health = static_cast<float>(target_.health());
        record.target_position = target_.origin();
    }
    shot_log_.push_back(std::move(record));
    while (shot_log_.size() > 32) {
        shot_log_.pop_front();
    }
}

void rage::update_shot_log() noexcept {
    const std::uint64_t now{now_ms()};
    for (shot_record &record : shot_log_) {
        if (record.resolved || !record.fired) {
            continue;
        }
        if (now - record.millis < 200) {
            continue;
        }
        record.resolved = true;
        if (record.target_clientnum < 0) {
            record.reason = "no target";
            continue;
        }
        acr::player victim{};
        for (const acr::player &entity : game_->players()) {
            if (entity.valid() &&
                entity.clientnum() == record.target_clientnum) {
                victim = entity;
                break;
            }
        }
        if (!victim.valid()) {
            record.reason = "target gone";
            continue;
        }
        if (!victim.alive() ||
            static_cast<float>(victim.health()) < record.target_health) {
            record.hit = true;
            record.reason = "hit";
            ZEPHYRUS_INFO("[shot] {} -> #{} {:.0f}m hc={}% HIT (hp {:.0f} -> {})",
                          acr::weapon_name(record.weapon),
                          record.target_clientnum,
                          static_cast<double>(record.distance),
                          record.hitchance,
                          static_cast<double>(record.target_health),
                          victim.health());
            continue;
        }
        const acr::vec3 moved{victim.origin().x - record.target_position.x,
                              victim.origin().y - record.target_position.y,
                              victim.origin().z - record.target_position.z};
        record.reason = length(moved) > 1.0F ? "target moved" : "spread";
        ZEPHYRUS_INFO("[shot] {} -> #{} {:.0f}m hc={}% MISS ({}) hp={} moved={:.2f}",
                      acr::weapon_name(record.weapon),
                      record.target_clientnum,
                      static_cast<double>(record.distance), record.hitchance,
                      record.reason, victim.health(),
                      static_cast<double>(length(moved)));
    }
}

void rage::on_shoot(void *player, void *target) noexcept {
    if (game_ == nullptr || !game_->valid()) {
        shoot_hook_.call_original(player, target);
        return;
    }

    acr::player local{game_->local_player()};
    const bool is_local{local.valid() && player == local.base()};
    const rage_profile &profile{active_profile()};
    const bool aim_active{profile.enabled && toggle_key.active()};
    const bool was_attacking{local.valid() && local.attacking()};

    if (!aim_active || !is_local) {
        if (forced_attack_ && local.valid()) {
            local.set_attacking(false);
            forced_attack_ = false;
        }
        if (scoped_by_us_ && local.valid() && scope_ != nullptr) {
            scope_(false);
            scoped_by_us_ = false;
        }
        target_ = {};
        shoot_hook_.call_original(player, target);
        return;
    }

    select_target(profile);

    const int gun{local.weapon_type()};
    const std::uint64_t now{now_ms()};
    last_hitchance_ = target_.valid() ? estimate_hitchance(profile) : 0;
    const bool chance_ok{target_.valid() &&
                         last_hitchance_ >= profile.hitchance};

    // Auto scope while a target is tracked.
    if (profile.auto_scope && scope_ != nullptr &&
        acr::weapon_scoped(gun)) {
        if (target_.valid() && !local.scoping() && !scoped_by_us_) {
            scope_(true);
            scoped_by_us_ = true;
        } else if (!target_.valid() && scoped_by_us_) {
            scope_(false);
            scoped_by_us_ = false;
        }
    }

    // Auto stop: hold fire and brake the player until the estimated hit
    // chance meets the configured threshold.
    if (target_.valid() && profile.auto_stop && !chance_ok) {
        local.stop_movement(true);
    }

    const bool between_shots_blocked{
        profile.between_shots && last_shot_time_ms_ != 0 &&
        now - last_shot_time_ms_ <
            static_cast<std::uint64_t>(
                std::max(game_->gun_attackdelay(gun), 50))};

    if (target_.valid() && chance_ok && !between_shots_blocked) {
        target_point_ = aim_point(profile);

        if (profile.silent_aim && target != nullptr) {
            auto *aim{static_cast<float *>(target)};
            aim[0] = target_point_.x;
            aim[1] = target_point_.y;
            aim[2] = target_point_.z;
        }

        bool fire{profile.autofire};
        if (profile.triggerbot && trigger_key.active()) {
            const acr::player crosshair{game_->crosshair_player()};
            if (crosshair.valid() && crosshair.base() == target_.base()) {
                fire = true;
            }
        }
        if (fire) {
            local.set_attacking(true);
            forced_attack_ = true;
            static std::size_t fire_log{};
            if ((++fire_log % 240) == 1) {
                void *weapon{acr::memory::read<void *>(
                    local.base(), acr::offsets::playerent_weaponsel)};
                const int ammo{
                    weapon != nullptr
                        ? (acr::memory::read<int *>(
                               weapon, acr::offsets::weapon_ammo) != nullptr
                               ? *acr::memory::read<int *>(
                                     weapon, acr::offsets::weapon_ammo)
                               : -1)
                        : -1};
                const int type{weapon != nullptr
                                   ? acr::memory::read<int>(
                                         weapon, acr::offsets::weapon_type)
                                   : -1};
                ZEPHYRUS_TRACE(
                    "rage: fire gun={} weapon={} type={} mag={} ammo={} changing={} hc={}",
                    gun, weapon, type, local.mag(), ammo,
                    acr::memory::read<bool>(
                        local.base(), acr::offsets::playerent_weaponchanging),
                    last_hitchance_);
            }
        }
        last_block_reason_.clear();
    } else {
        if (forced_attack_) {
            local.set_attacking(false);
            forced_attack_ = false;
        }
        if (now - last_block_log_ms_ > 500) {
            last_block_log_ms_ = now;
            if (!target_.valid()) {
                // Only report "no target" while the trigger is actually held;
                // otherwise this would spam constantly.
                if (was_attacking) {
                    set_block_reason("no target");
                } else {
                    last_block_reason_.clear();
                }
            } else if (!chance_ok) {
                set_block_reason("hitchance " +
                                 std::to_string(last_hitchance_) + "% < " +
                                 std::to_string(profile.hitchance) + "%");
            } else if (between_shots_blocked) {
                set_block_reason("between shots");
            }
            if (!last_block_reason_.empty()) {
                ZEPHYRUS_INFO("[shot] blocked: {}", last_block_reason_);
            }
        }
    }

    const int mag_before{local.mag()};
    const float distance{
        target_.valid() ? length({target_.origin().x - local.origin().x,
                                  target_.origin().y - local.origin().y,
                                  target_.origin().z - local.origin().z})
                        : 0.0F};

    shoot_hook_.call_original(player, target);

    if (profile.no_recoil) {
        local.reset_recoil();
    }

    const int mag_after{local.mag()};
    if (mag_before >= 0 && mag_after >= 0 && mag_after < mag_before) {
        last_shot_time_ms_ = now;
        record_shot(gun, last_hitchance_, distance);
    }

    update_shot_log();
}

void rage::on_sendshoot(void *weapon, const void *to) noexcept {
    const rage_profile &profile{active_profile()};
    const bool magnet{profile.enabled && toggle_key.active() &&
                      profile.head_magnet && target_.valid() &&
                      weapon != nullptr && to != nullptr &&
                      game_ != nullptr && game_->valid() &&
                      acr::memory::read<void *>(
                          weapon, acr::offsets::weapon_owner) ==
                          game_->local_player().base()};
    if (!magnet) {
        sendshoot_hook_.call_original(weapon, to);
        return;
    }

    const acr::vec3 saved{target_.head()};
    const auto *aim{static_cast<const float *>(to)};
    target_.set_head({aim[0], aim[1], aim[2]});

    sendshoot_hook_.call_original(weapon, to);

    target_.set_head(saved);
}
} // namespace zephyrus::features
