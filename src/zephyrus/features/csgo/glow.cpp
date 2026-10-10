// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Behaviour modeled after OsirisAndExtra (rewritten).

#include <zephyrus/features/csgo/glow.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace zephyrus::csgo::features {
namespace {
/// `GlowObjectDefinition` (Linux layout, validated upstream).
struct glow_object_definition {
    int next_free_slot;
    void *entity;
    Vector glow_color;
    float glow_alpha;
    bool glow_alpha_capped_by_render_alpha;
    float glow_alpha_function_of_max_velocity;
    float glow_alpha_max;
    float glow_pulse_overdrive;
    bool render_when_occluded;
    bool render_when_unoccluded;
    bool full_bloom_render;
    int full_bloom_stencil_test_value;
    int glow_style;
    int split_screen_slot;
};
static_assert(sizeof(glow_object_definition) == 64,
              "glow object layout changed");

/// `CUtlVector<GlowObjectDefinition>` followed by the free-list head.
struct glow_object_manager {
    glow_object_definition *definitions;
    int allocation_count;
    int grow_size;
    int size;
    int pad;
    int first_free_slot;
};

constexpr int entry_in_use{-2};

void rainbow_color(const float time, float &red, float &green,
                   float &blue) noexcept {
    const float hue{std::fmod(time * 0.5F, 1.0F) * 360.0F};
    const float h{hue / 60.0F};
    const float x{1.0F - std::abs(std::fmod(h, 2.0F) - 1.0F)};
    if (h < 1.0F) {
        red = 1.0F;
        green = x;
        blue = 0.0F;
    } else if (h < 2.0F) {
        red = x;
        green = 1.0F;
        blue = 0.0F;
    } else if (h < 3.0F) {
        red = 0.0F;
        green = 1.0F;
        blue = x;
    } else if (h < 4.0F) {
        red = 0.0F;
        green = x;
        blue = 1.0F;
    } else if (h < 5.0F) {
        red = x;
        green = 0.0F;
        blue = 1.0F;
    } else {
        red = 1.0F;
        green = 0.0F;
        blue = x;
    }
}
} // namespace

void glow::update(const game &game) noexcept {
    if (!options_.enabled || !game.resolved().glow_object_manager) {
        return;
    }
    const entity local{game.local_player()};
    if (!local.valid() || !local.is_alive()) {
        return;
    }

    auto *manager{reinterpret_cast<glow_object_manager *>(
        game.resolved().glow_object_manager)};
    if (manager->definitions == nullptr || manager->size <= 0 ||
        manager->size > 4096 || manager->allocation_count < manager->size ||
        manager->allocation_count > 65536) {
        return;
    }

    // Collect the enemy pointers from the entity list first, then only touch
    // manager entries whose entity pointer equals one of them: no virtual
    // call is ever made through a manager-provided pointer, so a layout
    // mismatch can only skip an entry, never crash.
    std::array<const void *, 65> enemies{};
    std::size_t enemy_count{};
    for (int index{1};
         index <= game.max_clients() && enemy_count < enemies.size(); ++index) {
        const entity candidate{game.get_entity(index)};
        if (!candidate.valid() || !candidate.is_player() ||
            candidate.is_dormant() || !candidate.is_alive() ||
            candidate.raw() == local.raw() ||
            !game.is_other_enemy(candidate, local)) {
            continue;
        }
        enemies[enemy_count++] = candidate.raw();
    }
    if (enemy_count == 0) {
        return;
    }

    const float time{game.current_time()};
    for (int i{}; i < manager->size; ++i) {
        glow_object_definition &object{manager->definitions[i]};
        if (object.next_free_slot != entry_in_use || object.entity == nullptr) {
            continue;
        }
        if (std::find(enemies.begin(), enemies.begin() + enemy_count,
                      object.entity) == enemies.begin() + enemy_count) {
            continue;
        }

        const entity candidate{object.entity};
        object.render_when_occluded = true;
        object.render_when_unoccluded = false;
        object.glow_alpha = 1.0F;
        object.glow_alpha_max = 0.6F;
        object.glow_style = options_.style;

        if (options_.health_based) {
            const float health{std::clamp(
                static_cast<float>(candidate.health()) / 100.0F, 0.0F, 1.0F)};
            object.glow_color = {1.0F - health, health, 0.0F};
        } else if (options_.rainbow) {
            float red{};
            float green{};
            float blue{};
            rainbow_color(time, red, green, blue);
            object.glow_color = {red, green, blue};
        } else {
            object.glow_color = {0.85F, 0.35F, 1.0F};
        }
    }
}
} // namespace zephyrus::csgo::features
