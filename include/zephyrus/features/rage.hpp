// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/game.hpp>
#include <zephyrus/acr/weapons.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/input/keybind.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <expected>
#include <string>

namespace zephyrus::features {
enum class rage_priority : int {
    fov = 0,
    distance = 1,
    health = 2,
};

/// Aim target resolved against the server's three hit zones (head / torso /
/// legs), with `lethal` picking the zone that needs the fewest hits.
enum class rage_hitbox : int {
    head = 0,
    chest = 1,
    stomach = 2,
    legs = 3,
    lethal = 4,
};

/// One configurable ragebot profile. Every weapon owns a slot; until a slot
/// is customized it inherits the global ("all weapons") profile, mirroring
/// the OsirisAndExtra per-weapon layout.
struct rage_profile {
    bool customized{false};
    bool enabled{false};
    bool silent_aim{true};
    bool autofire{true};
    bool triggerbot{false};
    bool head_magnet{true};
    bool require_visible{true};
    bool team_check{true};
    bool no_recoil{true};
    bool auto_scope{true};
    bool auto_stop{false};
    bool between_shots{false};
    rage_priority priority{rage_priority::fov};
    rage_hitbox hitbox{rage_hitbox::head};
    int hitchance{50};
    float fov{45.0F};
    float max_distance{1024.0F};
};

/// A single fired / blocked shot, resolved with the reason it connected or
/// missed.
struct shot_record {
    std::uint64_t millis{};
    int weapon{-1};
    int target_clientnum{-1};
    int hitchance{};
    float distance{};
    float target_health{};
    acr::vec3 target_position{};
    bool fired{};
    bool resolved{};
    bool hit{};
    std::string reason;
};

/// Silent-aim / autofire / triggerbot driven entirely by game-level hooks.
class rage {
  public:
    static constexpr int all_weapons{-1};

    [[nodiscard]] std::expected<void, std::string>
    initialize(const acr::game &game) noexcept;
    void shutdown() noexcept;

    // Global keybinds gate their respective feature.
    keybind toggle_key{};
    keybind trigger_key{};

    [[nodiscard]] rage_profile &global_profile() noexcept { return global_; }
    [[nodiscard]] rage_profile &profile_slot(int gun) noexcept;
    [[nodiscard]] const rage_profile &profile_for(int gun) const noexcept;
    [[nodiscard]] const rage_profile &active_profile() const noexcept;

    /// Editor helpers: `load_profile` returns a copy to edit, `store_profile`
    /// writes it back (marking weapon slots as customized).
    [[nodiscard]] rage_profile load_profile(int index) const noexcept;
    void store_profile(int index, const rage_profile &profile) noexcept;

    [[nodiscard]] bool has_target() const noexcept { return target_.valid(); }
    [[nodiscard]] acr::player target() const noexcept { return target_; }
    [[nodiscard]] int last_hitchance() const noexcept {
        return last_hitchance_;
    }
    [[nodiscard]] const std::deque<shot_record> &shots() const noexcept {
        return shot_log_;
    }
    [[nodiscard]] std::string_view last_block_reason() const noexcept {
        return last_block_reason_;
    }

  private:
    void on_shoot(void *player, void *target) noexcept;
    void on_sendshoot(void *weapon, const void *to) noexcept;
    void select_target(const rage_profile &profile) noexcept;
    [[nodiscard]] acr::vec3 aim_point(const rage_profile &profile) const noexcept;
    [[nodiscard]] int estimate_hitchance(const rage_profile &profile) const noexcept;
    [[nodiscard]] int resolve_hitbox(const rage_profile &profile) const noexcept;
    void record_shot(int weapon, int hitchance, float distance) noexcept;
    void update_shot_log() noexcept;
    void set_block_reason(std::string reason) noexcept;

    const acr::game *game_{};
    hook::function_hook<void(void *, void *)> shoot_hook_;
    hook::function_hook<void(void *, const void *)> sendshoot_hook_;
    void (*scope_)(bool){};

    std::array<rage_profile, acr::gun_count> weapons_{};
    rage_profile global_{};

    acr::player target_{};
    acr::vec3 target_point_{};
    bool forced_attack_{};
    bool scoped_by_us_{};
    int last_hitchance_{};
    std::string last_block_reason_;
    std::deque<shot_record> shot_log_;
    std::uint64_t last_block_log_ms_{};
    std::uint64_t last_shot_time_ms_{};
};
} // namespace zephyrus::features
