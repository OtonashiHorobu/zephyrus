// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu
//
// Portions derived from Osiris (MIT), Copyright(c) 2018-2023 Daniel
// Krupiński. Layouts and vtable indices are the validated Linux ones
// (legacy CS:GO 1.38.8.1 linux64).

#pragma once

#include <zephyrus/csgo/math.hpp>
#include <zephyrus/csgo/netvars.hpp>
#include <zephyrus/csgo/offsets.hpp>

#include <cstddef>
#include <cstdint>

namespace zephyrus::csgo {
struct entity_pod;
struct recv_table;
struct weapon_info;

/// `matrix3x4_t` used for bone transforms.
struct matrix3x4 {
    float m[3][4];

    [[nodiscard]] Vector origin() const noexcept {
        return {m[0][3], m[1][3], m[2][3]};
    }
};

/// `CGameTrace` (subset the features read).
struct trace {
    Vector startpos;
    Vector endpos;
    std::byte pad0[20];
    float fraction;
    int contents;
    unsigned short disp_flags;
    bool all_solid;
    bool start_solid;
    std::byte pad1[4];
    struct {
        const char *name;
        short surface_props;
        unsigned short flags;
    } surface;
    int hitgroup;
    std::byte pad2[4];
    entity_pod *entity;
    int hitbox;
};

struct ray {
    ray(const Vector &source, const Vector &destination) noexcept
        : start{source}, delta{destination - source},
          is_swept{delta.x != 0.0F || delta.y != 0.0F || delta.z != 0.0F} {}

    Vector start{};
    float pad{};
    Vector delta{};
    std::byte pad2[44]{};
    bool is_ray{true};
    bool is_swept{};
};
static_assert(offsetof(ray, is_ray) == 72,
              "Ray_t layout changed (m_IsRay must sit at +0x48)");

/// `CGameTrace` is exactly 104 bytes on this build: the engine zeroes
/// 0x68 bytes before every trace, so the struct must not be smaller. The
/// 4-byte padding before `surface` is implicit (the pointer forces 8-byte
/// alignment); `all_solid`/`start_solid` land at +54/+55 as the engine reads
/// them.
static_assert(sizeof(trace) == 0x68, "CGameTrace size changed");
static_assert(offsetof(trace, all_solid) == 54 &&
                  offsetof(trace, start_solid) == 55,
              "CGameTrace flags moved");

/// `ITraceFilter` as the engine sees it: slot 0 = `ShouldHitEntity`,
/// slot 1 = `GetTraceType`. There is deliberately **no virtual destructor**;
/// adding one shifts the vtable and makes the engine call the destructor
/// through `GetTraceType`.
struct trace_filter {
    explicit trace_filter(entity_pod *skip) noexcept : skip{skip} {}
    virtual bool should_hit_entity(entity_pod *, int) noexcept { return true; }
    virtual int get_trace_type() const noexcept { return 0; }
    entity_pod *skip;
};

struct trace_filter_skip : trace_filter {
    explicit trace_filter_skip(entity_pod *skip) noexcept
        : trace_filter{skip} {}
    bool should_hit_entity(entity_pod *entity, int) noexcept override {
        return entity != skip;
    }
};

/// Typed `IEngineTrace` calls (Linux vtable, verified against
/// `engine_client.so` slot 5 / slot 0). The engine takes **pointers** for the
/// ray, the filter and the trace; passing the aggregates by value would place
/// them on the stack (SysV MEMORY class) and leave the argument registers
/// with garbage, which crashes inside `TraceRay`. Always go through these
/// helpers instead of `virtual_call`.
[[nodiscard]] inline trace
engine_trace_ray(void *engine_trace, const ray &shot, const unsigned int mask,
                 trace_filter *filter) noexcept {
    auto **vtable{*reinterpret_cast<void ***>(engine_trace)};
    using fn_type =
        void (*)(void *, const ray &, unsigned int, trace_filter *, trace *);
    trace result{};
    reinterpret_cast<fn_type>(vtable[5])(engine_trace, shot, mask, filter,
                                         &result);
    return result;
}

[[nodiscard]] inline int engine_point_contents(void *engine_trace,
                                               const vector &point,
                                               const unsigned int mask) noexcept {
    auto **vtable{*reinterpret_cast<void ***>(engine_trace)};
    using fn_type = int (*)(void *, const vector &, unsigned int, void **);
    return reinterpret_cast<fn_type>(vtable[0])(engine_trace, point, mask,
                                                nullptr);
}

/// `SurfaceData` from `IPhysicsSurfaceProps` (Linux layout validated by
/// Osiris: 80-byte prefix, then the physical parameters).
struct surface_data {
    std::byte pad0[80];
    float max_speed_factor;
    float jump_factor;
    float penetration_modifier;
    float damage_modifier;
    short material;
    bool climbable;
};

namespace hit_group {
enum : int {
    invalid = -1,
    generic,
    head,
    chest,
    stomach,
    left_arm,
    right_arm,
    left_leg,
    right_leg,
    gear = 10,
};
} // namespace hit_group

/// `CAnimationLayer` (client layout; the networked tail starts at `order`).
/// Field offsets follow the OsirisAndExtra Windows SDK and are used by the
/// resolver detectors and the animation recorder.
struct animation_layer {
    float animation_time{};
    float fade_out{};
    void *dispatched_studio_hdr{};
    int dispatched_src{};
    int dispatched_dst{};
    unsigned int order{};
    unsigned int sequence{};
    float prev_cycle{};
    float weight{};
    float weight_delta_rate{};
    float playback_rate{};
    float cycle{};
    void *owner{};
    int invalidate_physics_bits{};
};
static_assert(sizeof(animation_layer) == 72,
              "animation_layer Linux x64 layout changed");

/// `CCSPlayerAnimState` field view. The Linux offsets live in
/// `csgo/offsets.hpp`; until they are recovered every accessor returns a safe
/// default instead of dereferencing a guess.
class anim_state_view {
  public:
    anim_state_view() noexcept = default;
    explicit anim_state_view(void *state) noexcept : state_{state} {}

    [[nodiscard]] bool valid() const noexcept {
        // A live AnimState is a heap pointer (above the 4 GiB line on x64);
        // a 32-bit-looking value means the entity field is stale/cleared
        // (e.g. dead players) and must not be dereferenced.
        return state_ != nullptr &&
               reinterpret_cast<std::uintptr_t>(state_) >= 0x100000000ULL &&
               offsets::anim_state.eye_yaw != 0;
    }
    [[nodiscard]] void *raw() const noexcept { return state_; }

    [[nodiscard]] float eye_yaw() const noexcept {
        return read<float>(offsets::anim_state.eye_yaw);
    }
    [[nodiscard]] float foot_yaw() const noexcept {
        return read<float>(offsets::anim_state.foot_yaw);
    }
    [[nodiscard]] float foot_yaw_last() const noexcept {
        return read<float>(offsets::anim_state.foot_yaw_last);
    }
    [[nodiscard]] float move_yaw() const noexcept {
        return read<float>(offsets::anim_state.move_yaw);
    }
    [[nodiscard]] float move_weight() const noexcept {
        return read<float>(offsets::anim_state.move_weight);
    }
    [[nodiscard]] float duck_amount() const noexcept {
        return read<float>(offsets::anim_state.duck_amount);
    }
    [[nodiscard]] float duck_additional() const noexcept {
        return read<float>(offsets::anim_state.duck_additional);
    }
    [[nodiscard]] float walk_to_run_transition() const noexcept {
        return read<float>(offsets::anim_state.walk_to_run_transition);
    }
    [[nodiscard]] bool on_ground() const noexcept {
        return read<bool>(offsets::anim_state.on_ground);
    }
    [[nodiscard]] float last_update_time() const noexcept {
        return read<float>(offsets::anim_state.last_update_time);
    }
    [[nodiscard]] float velocity_length_xy() const noexcept {
        return read<float>(offsets::anim_state.velocity_length_xy);
    }

    void set_eye_yaw(const float value) const noexcept {
        write<float>(offsets::anim_state.eye_yaw, value);
    }
    void set_foot_yaw(const float value) const noexcept {
        write<float>(offsets::anim_state.foot_yaw, value);
        write<float>(offsets::anim_state.foot_yaw_last, value);
    }
    void set_move_yaw(const float value) const noexcept {
        write<float>(offsets::anim_state.move_yaw, value);
    }
    void set_duck_amount(const float value) const noexcept {
        write<float>(offsets::anim_state.duck_amount, value);
    }

  private:
    template <typename T>
    [[nodiscard]] T read(const std::ptrdiff_t offset) const noexcept {
        if (state_ == nullptr || offset == 0) {
            return {};
        }
        return *reinterpret_cast<const T *>(
            static_cast<const std::byte *>(state_) + offset);
    }
    template <typename T>
    void write(const std::ptrdiff_t offset, const T value) const noexcept {
        if (state_ == nullptr || offset == 0) {
            return;
        }
        *reinterpret_cast<T *>(static_cast<std::byte *>(state_) + offset) =
            value;
    }

    void *state_{};
};

/// `WeaponType` from `CCSWeaponBaseVData` (Linux layout).
enum class weapon_type : int {
    knife = 0,
    pistol,
    submachinegun,
    rifle,
    shotgun,
    sniper_rifle,
    machinegun,
    c4,
    placeholder,
    grenade,
    unknown,
    stackable_item,
    fists,
    breach_charge,
    bump_mine,
    tablet,
    melee,
};

/// `CCSWeaponBaseVData` fields the features read.
struct weapon_info {
    std::byte pad0[32];
    int max_clip;
    std::byte pad1[204];
    const char *name;
    std::byte pad2[72];
    weapon_type type;
    std::byte pad3[4];
    int price;
    std::byte pad4[12];
    float cycletime;
    std::byte pad5[12];
    bool full_auto;
    std::byte pad6[3];
    int damage;
    float headshot_multiplier;
    float armor_ratio;
    int bullets;
    float penetration;
    std::byte pad7[8];
    float range;
    float range_modifier;
    std::byte pad8[16];
    bool silencer;
    std::byte pad9[23];
    float max_speed;
    float max_speed_alt;
    std::byte pad10[100];
    float recoil_magnitude;
    float recoil_magnitude_alt;
    std::byte pad11[16];
    float recovery_time_stand;
};

/// Entity vtable dispatch (SysV: the implicit object pointer is the first
/// argument, exactly like a plain C function).
template <std::size_t Index, typename Ret, typename... Args>
[[nodiscard]] Ret virtual_call(void *self, Args... args) noexcept {
    auto **vtable{*reinterpret_cast<void ***>(self)};
    using fn_type = Ret (*)(void *, Args...);
    return reinterpret_cast<fn_type>(vtable[Index])(self, args...);
}

class entity {
  public:
    entity() noexcept = default;
    explicit entity(void *self) noexcept : self_{self} {}

    template <typename T>
    [[nodiscard]] T netvar(std::uint32_t hash) const noexcept {
        return *reinterpret_cast<T *>(static_cast<std::byte *>(self_) +
                                      netvars::get(hash));
    }

    template <typename T>
    void set_netvar(std::uint32_t hash, T value) const noexcept {
        const std::uint32_t offset{netvars::get(hash)};
        if (offset != 0) {
            *reinterpret_cast<T *>(static_cast<std::byte *>(self_) + offset) =
                value;
        }
    }

    [[nodiscard]] void *raw() const noexcept { return self_; }
    [[nodiscard]] entity_pod *pod() const noexcept {
        return reinterpret_cast<entity_pod *>(self_);
    }
    [[nodiscard]] bool valid() const noexcept { return self_ != nullptr; }

    // --- entity vtable ---
    [[nodiscard]] bool is_player() const noexcept {
        return virtual_call<210, bool>(self_);
    }
    [[nodiscard]] bool is_alive() const noexcept {
        return virtual_call<208, bool>(self_);
    }
    [[nodiscard]] bool is_weapon() const noexcept {
        return virtual_call<218, bool>(self_);
    }
    [[nodiscard]] int health() const noexcept {
        return virtual_call<167, int>(self_);
    }
    [[nodiscard]] int team_number() const noexcept {
        return virtual_call<128, int>(self_);
    }
    [[nodiscard]] const Vector &abs_origin() const noexcept {
        return virtual_call<12, const Vector &>(self_);
    }
    [[nodiscard]] Vector eye_position() const noexcept {
        return virtual_call<348, Vector>(self_);
    }
    [[nodiscard]] Vector aim_punch() const noexcept {
        return virtual_call<409, Vector>(self_);
    }
    [[nodiscard]] entity active_weapon() const noexcept {
        return entity{virtual_call<331, entity_pod *>(self_)};
    }
    [[nodiscard]] weapon_type get_weapon_type() const noexcept {
        return virtual_call<523, weapon_type>(self_);
    }
    [[nodiscard]] weapon_info *weapon_data() const noexcept {
        return virtual_call<529, weapon_info *>(self_);
    }
    [[nodiscard]] float inaccuracy() const noexcept {
        return virtual_call<551, float>(self_);
    }
    [[nodiscard]] float spread() const noexcept {
        return virtual_call<552, float>(self_);
    }

    // --- renderable / networkable subobjects ---
    [[nodiscard]] void *renderable() const noexcept {
        return static_cast<std::byte *>(self_) + sizeof(void *);
    }
    [[nodiscard]] void *networkable() const noexcept {
        return static_cast<std::byte *>(self_) + sizeof(void *) * 2;
    }
    [[nodiscard]] bool is_dormant() const noexcept {
        return virtual_call<9, bool>(networkable());
    }
    [[nodiscard]] int index() const noexcept {
        return virtual_call<10, int>(networkable());
    }
    [[nodiscard]] bool setup_bones(matrix3x4 *out, int max_bones, int bone_mask,
                                   float current_time) const noexcept {
        return virtual_call<13, bool>(renderable(), out, max_bones, bone_mask,
                                      current_time);
    }
    [[nodiscard]] Vector bone_position(int bone) const noexcept {
        matrix3x4 bones[256]{};
        if (setup_bones(bones, 256, 256, 0.0F)) {
            return bones[bone].origin();
        }
        return {};
    }

    [[nodiscard]] bool is_visible(const void *engine_trace, const Vector &from,
                                  const Vector &to) const noexcept {
        if (engine_trace == nullptr) {
            return false;
        }
        trace_filter_skip filter{pod()};
        const ray shot{from, to};
        const trace result{engine_trace_ray(const_cast<void *>(engine_trace),
                                            shot, 0x46004009U, &filter)};
        return result.entity == pod() || result.fraction > 0.97F;
    }

    // --- netvars ---
    [[nodiscard]] Vector origin() const noexcept {
        return netvar<Vector>("CBaseEntity->m_vecOrigin"_nv);
    }
    [[nodiscard]] bool spotted() const noexcept {
        return netvar<bool>("CBaseEntity->m_bSpotted"_nv);
    }
    [[nodiscard]] float simulation_time() const noexcept {
        return netvar<float>("CBaseEntity->m_flSimulationTime"_nv);
    }
    void set_simulation_time(float value) const noexcept {
        set_netvar<float>("CBaseEntity->m_flSimulationTime"_nv, value);
    }
    [[nodiscard]] Vector velocity() const noexcept {
        return netvar<Vector>("CBasePlayer->m_vecVelocity[0]"_nv);
    }
    [[nodiscard]] int flags() const noexcept {
        return netvar<int>("CBasePlayer->m_fFlags"_nv);
    }
    [[nodiscard]] bool is_on_ground() const noexcept {
        return (flags() & 1) != 0;
    }
    [[nodiscard]] int tick_base() const noexcept {
        return netvar<int>("CBasePlayer->m_nTickBase"_nv);
    }
    [[nodiscard]] Vector aim_punch_angle() const noexcept {
        return netvar<Vector>("CBasePlayer->m_aimPunchAngle"_nv);
    }
    [[nodiscard]] Vector eye_angles() const noexcept {
        return netvar<Vector>("CCSPlayer->m_angEyeAngles"_nv);
    }
    [[nodiscard]] int armor() const noexcept {
        return netvar<int>("CCSPlayer->m_ArmorValue"_nv);
    }
    [[nodiscard]] bool has_helmet() const noexcept {
        return netvar<bool>("CCSPlayer->m_bHasHelmet"_nv);
    }
    [[nodiscard]] bool is_scoped() const noexcept {
        return netvar<bool>("CCSPlayer->m_bIsScoped"_nv);
    }
    [[nodiscard]] bool is_defusing() const noexcept {
        return netvar<bool>("CCSPlayer->m_bIsDefusing"_nv);
    }
    [[nodiscard]] bool wait_for_no_attack() const noexcept {
        return netvar<bool>("CCSPlayer->m_bWaitForNoAttack"_nv);
    }
    [[nodiscard]] bool gun_game_immunity() const noexcept {
        return netvar<bool>("CCSPlayer->m_bGunGameImmunity"_nv);
    }
    [[nodiscard]] int shots_fired() const noexcept {
        return netvar<int>("CCSPlayer->m_iShotsFired"_nv);
    }
    /// `m_flFlashDuration` sits right before `m_flFlashMaxAlpha` on this
    /// build (Osiris derives it the same way).
    [[nodiscard]] float flash_duration() const noexcept {
        const std::uint32_t offset{
            netvars::get("CCSPlayer->m_flFlashMaxAlpha"_nv)};
        if (offset < 8) {
            return 0.0F;
        }
        return *reinterpret_cast<float *>(static_cast<std::byte *>(self_) +
                                          offset - 8);
    }
    [[nodiscard]] bool is_flashed() const noexcept {
        return flash_duration() > 75.0F;
    }
    [[nodiscard]] float lower_body_yaw() const noexcept {
        return netvar<float>("CCSPlayer->m_flLowerBodyYawTarget"_nv);
    }
    [[nodiscard]] float duck_amount() const noexcept {
        return netvar<float>("CCSPlayer->m_flDuckAmount"_nv);
    }
    [[nodiscard]] bool has_heavy_armor() const noexcept {
        const std::uint32_t offset{
            netvars::get("CCSPlayer->m_bHasHeavyArmor"_nv)};
        return offset != 0 && *reinterpret_cast<bool *>(
                                  static_cast<std::byte *>(self_) + offset);
    }
    [[nodiscard]] bool is_armored() const noexcept { return armor() > 0; }
    /// True when the given hit group is protected by armor (helmet gates the
    /// head; chest/stomach/arms are always covered).
    [[nodiscard]] bool is_hitgroup_armored(const int hitgroup) const noexcept {
        switch (hitgroup) {
        case hit_group::head:
            return has_helmet();
        case hit_group::chest:
        case hit_group::stomach:
        case hit_group::left_arm:
        case hit_group::right_arm:
            return is_armored();
        default:
            return false;
        }
    }

    // --- animation state ---
    /// Pointer to the player's `CCSPlayerAnimState` (null until the Linux
    /// offset is recovered in `csgo/offsets.hpp`).
    [[nodiscard]] void *anim_state_ptr() const noexcept {
        if (!offset_valid(offsets::player_anim_state)) {
            return nullptr;
        }
        return *reinterpret_cast<void **>(static_cast<std::byte *>(self_) +
                                          offsets::player_anim_state);
    }
    [[nodiscard]] anim_state_view anim_state() const noexcept {
        return anim_state_view{anim_state_ptr()};
    }
    /// `m_flPoseParameter` (24 floats). Returns null when the netvar did not
    /// resolve.
    [[nodiscard]] float *pose_parameters() const noexcept {
        const std::uint32_t offset{
            netvars::get("CBaseAnimating->m_flPoseParameter"_nv)};
        if (offset == 0) {
            return nullptr;
        }
        return reinterpret_cast<float *>(static_cast<std::byte *>(self_) +
                                         offset);
    }
    /// Animation layers; empty when neither the Linux offset nor the netvar
    /// resolved.
    [[nodiscard]] const animation_layer *animation_layers() const noexcept {
        std::ptrdiff_t offset{offsets::anim_overlay_vector};
        if (offset == 0) {
            offset = netvars::get("CBaseAnimating->m_AnimOverlay"_nv);
        }
        if (offset == 0) {
            return nullptr;
        }
        auto *elements{*reinterpret_cast<animation_layer **>(
            static_cast<std::byte *>(self_) + offset)};
        return elements;
    }
    [[nodiscard]] int animation_layer_count() const noexcept {
        std::ptrdiff_t offset{offsets::anim_overlay_count};
        if (offset == 0) {
            const std::uint32_t netvar{
                netvars::get("CBaseAnimating->m_AnimOverlay"_nv)};
            if (netvar == 0) {
                return 0;
            }
            // CUtlVector/CNetworkUtlVectorBase: size follows the element
            // pointer (Linux delta verified at +0x10).
            offset = static_cast<std::ptrdiff_t>(netvar) + 0x10;
        }
        return *reinterpret_cast<int *>(static_cast<std::byte *>(self_) +
                                        offset);
    }
    [[nodiscard]] int item_definition_index() const noexcept {
        return netvar<short>(
            "CBaseAttributableItem->m_iItemDefinitionIndex"_nv);
    }
    [[nodiscard]] int clip() const noexcept {
        return netvar<int>("CBaseCombatWeapon->m_iClip1"_nv);
    }
    [[nodiscard]] float next_primary_attack() const noexcept {
        return netvar<float>("CBaseCombatWeapon->m_flNextPrimaryAttack"_nv);
    }
    [[nodiscard]] float next_attack() const noexcept {
        return netvar<float>("CBaseCombatCharacter->m_flNextAttack"_nv);
    }

    [[nodiscard]] bool is_full_auto() const noexcept {
        const weapon_info *data{weapon_data()};
        return data != nullptr && data->full_auto;
    }
    [[nodiscard]] bool requires_recoil_control() const noexcept {
        const weapon_info *data{weapon_data()};
        return data != nullptr && data->recoil_magnitude < 35.0F &&
               data->recovery_time_stand > data->cycletime;
    }
    [[nodiscard]] weapon_type type() const noexcept {
        return get_weapon_type();
    }
    [[nodiscard]] bool is_sniper_rifle() const noexcept {
        return type() == weapon_type::sniper_rifle;
    }

  private:
    void *self_{};
};
} // namespace zephyrus::csgo
