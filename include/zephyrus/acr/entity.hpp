// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/offsets.hpp>

#include <cstddef>
#include <cstring>
#include <string_view>

namespace zephyrus::acr {
struct vec3 {
    float x{};
    float y{};
    float z{};
};

/// Raw memory access helpers for game objects.
class memory {
  public:
    template <typename T>
    [[nodiscard]] static T
    read(const void *base, std::size_t offset) noexcept {
        T value{};
        std::memcpy(&value, static_cast<const std::byte *>(base) + offset,
                    sizeof(T));
        return value;
    }

    template <typename T>
    static void write(void *base, std::size_t offset, T value) noexcept {
        std::memcpy(static_cast<std::byte *>(base) + offset, &value,
                    sizeof(T));
    }

    [[nodiscard]] static std::string_view
    string(const void *base, std::size_t offset,
           std::size_t capacity) noexcept {
        const auto *text{static_cast<const char *>(base) + offset};
        return std::string_view{text, strnlen(text, capacity)};
    }
};

/// Read-only view over a `playerent`.
class player {
  public:
    player() noexcept = default;
    explicit player(void *base) noexcept : base_{base} {}

    [[nodiscard]] bool valid() const noexcept { return base_ != nullptr; }
    [[nodiscard]] void *base() const noexcept { return base_; }

    [[nodiscard]] int clientnum() const noexcept {
        return memory::read<int>(base_, offsets::playerent_clientnum);
    }
    [[nodiscard]] int team() const noexcept {
        return memory::read<int>(base_, offsets::playerent_team);
    }
    [[nodiscard]] int state() const noexcept {
        return memory::read<int>(base_, offsets::physent_state);
    }
    [[nodiscard]] bool alive() const noexcept {
        return state() == offsets::cs_alive;
    }
    [[nodiscard]] bool spectating() const noexcept {
        return team() == offsets::team_spect ||
               (team() != offsets::team_cla && team() != offsets::team_rvsf);
    }
    [[nodiscard]] int ownernum() const noexcept {
        return memory::read<int>(base_, offsets::playerent_ownernum);
    }
    [[nodiscard]] bool is_bot() const noexcept { return ownernum() >= 0; }
    [[nodiscard]] int ping() const noexcept {
        return memory::read<int>(base_, offsets::playerent_ping);
    }

    [[nodiscard]] vec3 origin() const noexcept { // eye position
        return {memory::read<float>(base_, offsets::physent_o + offsets::vec_x),
                memory::read<float>(base_, offsets::physent_o + offsets::vec_y),
                memory::read<float>(base_, offsets::physent_o +
                                              offsets::vec_z)};
    }
    [[nodiscard]] float eyeheight() const noexcept {
        return memory::read<float>(base_, offsets::physent_eyeheight);
    }
    [[nodiscard]] float aboveeye() const noexcept {
        return memory::read<float>(base_, offsets::physent_aboveeye);
    }
    [[nodiscard]] float radius() const noexcept {
        return memory::read<float>(base_, offsets::physent_radius);
    }
    [[nodiscard]] vec3 velocity() const noexcept {
        return {memory::read<float>(base_, offsets::physent_vel + offsets::vec_x),
                memory::read<float>(base_, offsets::physent_vel + offsets::vec_y),
                memory::read<float>(base_,
                                    offsets::physent_vel + offsets::vec_z)};
    }
    [[nodiscard]] vec3 feet() const noexcept {
        vec3 result{origin()};
        result.z -= eyeheight();
        return result;
    }
    [[nodiscard]] vec3 head() const noexcept {
        return {memory::read<float>(base_,
                                    offsets::playerent_head + offsets::vec_x),
                memory::read<float>(base_,
                                    offsets::playerent_head + offsets::vec_y),
                memory::read<float>(base_,
                                    offsets::playerent_head + offsets::vec_z)};
    }
    [[nodiscard]] bool head_valid() const noexcept {
        const vec3 value{head()};
        return value.x > 0.0f && value.y > 0.0f && value.z > 0.0f;
    }
    void set_head(const vec3 &value) noexcept {
        memory::write(base_, offsets::playerent_head + offsets::vec_x, value.x);
        memory::write(base_, offsets::playerent_head + offsets::vec_y, value.y);
        memory::write(base_, offsets::playerent_head + offsets::vec_z, value.z);
    }

    [[nodiscard]] float yaw() const noexcept {
        return memory::read<float>(base_, offsets::physent_yaw);
    }
    [[nodiscard]] float pitch() const noexcept {
        return memory::read<float>(base_, offsets::physent_pitch);
    }

    [[nodiscard]] int health() const noexcept {
        return memory::read<int>(base_, offsets::playerent_health);
    }
    [[nodiscard]] int armour() const noexcept {
        return memory::read<int>(base_, offsets::playerent_armour);
    }
    [[nodiscard]] bool crouching() const noexcept {
        return memory::read<bool>(base_, offsets::physent_crouching);
    }
    [[nodiscard]] float zoomed() const noexcept {
        return memory::read<float>(base_, offsets::playerent_zoomed);
    }
    [[nodiscard]] float maxeyeheight() const noexcept {
        return memory::read<float>(base_, offsets::physent_maxeyeheight);
    }
    [[nodiscard]] int perk2() const noexcept {
        return memory::read<int>(base_, offsets::playerent_playerstate +
                                          offsets::playerstate_perk2);
    }
    [[nodiscard]] bool scoping() const noexcept {
        return memory::read<bool>(base_, offsets::playerent_playerstate +
                                            offsets::playerstate_scoping);
    }
    [[nodiscard]] int gunselect() const noexcept {
        return memory::read<int>(base_, offsets::playerent_playerstate +
                                           offsets::playerstate_gunselect);
    }

    /// Rounds currently in the selected weapon's magazine (-1 when unknown).
    /// Note: `weapon::mag` is an `int&` reference into the owner's state, so
    /// the stored value is dereferenced here.
    [[nodiscard]] int mag() const noexcept {
        void *weapon{memory::read<void *>(base_,
                                          offsets::playerent_weaponsel)};
        if (weapon == nullptr) {
            return -1;
        }
        const auto *mag{memory::read<int *>(weapon, offsets::weapon_mag)};
        return mag != nullptr ? *mag : -1;
    }

    /// Cancel the local recoil view kick (client-side only).
    void reset_recoil() noexcept {
        memory::write(base_, offsets::physent_pitchvel, 0.0F);
        memory::write(base_, offsets::physent_yawvel, 0.0F);
    }

    /// Stop movement input; optionally kill the residual velocity as well.
    void stop_movement(bool damp_velocity) noexcept {
        memory::write(base_, offsets::physent_move, static_cast<char>(0));
        memory::write(base_, offsets::physent_strafe, static_cast<char>(0));
        if (damp_velocity) {
            memory::write(base_, offsets::physent_vel + offsets::vec_x, 0.0F);
            memory::write(base_, offsets::physent_vel + offsets::vec_y, 0.0F);
        }
    }
    [[nodiscard]] std::string_view name() const noexcept {
        return memory::string(base_, offsets::playerent_name,
                              offsets::maxstrlen);
    }

    [[nodiscard]] bool attacking() const noexcept {
        return memory::read<bool>(base_, offsets::playerent_attacking);
    }
    void set_attacking(bool value) noexcept {
        memory::write(base_, offsets::playerent_attacking, value);
    }

    [[nodiscard]] int weapon_type() const noexcept {
        void *weapon{memory::read<void *>(base_,
                                          offsets::playerent_weaponsel)};
        if (weapon == nullptr) {
            return -1;
        }
        return memory::read<int>(weapon, offsets::weapon_type);
    }

  private:
    void *base_{};
};
} // namespace zephyrus::acr
