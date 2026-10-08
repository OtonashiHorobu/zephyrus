// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/features/skeleton.hpp>

#include <zephyrus/acr/entity.hpp>
#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace zephyrus::features {
namespace {
// Layouts generated with `offsetof` against the matching game headers.
constexpr std::size_t vertmodel_parts = 64;    // vector<part*>
constexpr std::size_t part_meshes = 32;        // vector<mesh*>
constexpr std::size_t mesh_dyncache = 80;      // modelcachelist<dyncacheentry>
constexpr std::size_t mesh_numverts_off = 56;  // mesh->numverts
constexpr std::size_t cache_list_next = 8;     // list head pointer
constexpr std::size_t cache_entry_next = 8;    // entry->next
constexpr std::size_t cache_entry_size = 24;   // entry->size
constexpr std::size_t cache_entry_cur = 36;    // entry->cur (anpos)
constexpr std::size_t cache_entry_data = 64;   // sizeof(dyncacheentry)

constexpr std::size_t dynent_current = 196; // animstate current[2]
constexpr int anim_loop = 1 << 8;
constexpr int anim_reverse = 1 << 11;

struct animstate {
    int anim{};
    int frame{};
    int range{};
    int basetime{};
    float speed{};
};

struct anpos {
    int fr1{};
    int fr2{};
    float t{};
};

anpos frames_of(const animstate &state, int millis) {
    anpos result{};
    if (state.speed <= 0.0F || state.range <= 0) {
        return result;
    }
    const int time{millis - state.basetime};
    result.fr1 = static_cast<int>(static_cast<float>(time) / state.speed);
    result.t =
        (static_cast<float>(time) - static_cast<float>(result.fr1) *
                                        state.speed) /
        state.speed;
    if ((state.anim & anim_loop) != 0) {
        result.fr1 = result.fr1 % state.range + state.frame;
        result.fr2 = result.fr1 + 1;
        if (result.fr2 >= state.frame + state.range) {
            result.fr2 = state.frame;
        }
    } else {
        result.fr1 = std::min(result.fr1, state.range - 1) + state.frame;
        result.fr2 = std::min(result.fr1 + 1, state.frame + state.range - 1);
    }
    if ((state.anim & anim_reverse) != 0) {
        result.fr1 = (state.frame + state.range - 1) - (result.fr1 - state.frame);
        result.fr2 = (state.frame + state.range - 1) - (result.fr2 - state.frame);
    }
    return result;
}

acr::vec3 transform_point(const float (&m)[16], const acr::vec3 &v) {
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12],
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

acr::vec3 normalize(const acr::vec3 &v) {
    const float length{std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z)};
    if (length <= 1.0e-6F) {
        return {};
    }
    return {v.x / length, v.y / length, v.z / length};
}

struct cluster {
    acr::vec3 sum{};
    float count{};
};

void add(cluster &c, const acr::vec3 &p) {
    c.sum.x += p.x;
    c.sum.y += p.y;
    c.sum.z += p.z;
    c.count += 1.0F;
}

acr::vec3 mean(const cluster &c) {
    if (c.count <= 0.0F) {
        return {};
    }
    return {c.sum.x / c.count, c.sum.y / c.count, c.sum.z / c.count};
}
} // namespace

std::expected<void, std::string>
skeleton_capture::initialize(const acr::game &game) noexcept {
    game_ = &game;

    auto matrixstack{hook::resolve("_ZN9vertmodel11matrixstackE")};
    if (!matrixstack) {
        return std::unexpected{"missing vertmodel::matrixstack: " +
                               matrixstack.error()};
    }
    matrixstack_ = reinterpret_cast<const float *>(*matrixstack);

    auto millis{hook::resolve("lastmillis")};
    if (!millis) {
        return std::unexpected{"missing lastmillis: " + millis.error()};
    }
    lastmillis_ = reinterpret_cast<const int *>(*millis);

    auto hook{hook::function_hook<render_signature>::install(
        "_ZN3md26renderEiifiRK3vecffP6dynentP11modelattachff",
        [this](void *self, int anim, int varseed, float speed, int basetime,
               const void *origin, float yaw, float pitch, void *dynent,
               const void *attach, float scale, float zoomed) {
            ++render_depth_;
            render_hook_.call_original(self, anim, varseed, speed, basetime,
                                       origin, yaw, pitch, dynent, attach,
                                       scale, zoomed);
            --render_depth_;
            on_render_done(self, dynent);
        })};
    if (!hook) {
        return std::unexpected{"failed to hook md2::render(): " +
                               hook.error()};
    }
    render_hook_ = std::move(*hook);

    // Player models are normally MD2, but the loader falls back to MD3; both
    // go through the same vertmodel mesh/dyncache pipeline.
    auto md3_hook{hook::function_hook<render_signature>::install(
        "_ZN3md36renderEiifiRK3vecffP6dynentP11modelattachff",
        [this](void *self, int anim, int varseed, float speed, int basetime,
               const void *origin, float yaw, float pitch, void *dynent,
               const void *attach, float scale, float zoomed) {
            ++render_depth_;
            render_hook_md3_.call_original(self, anim, varseed, speed,
                                           basetime, origin, yaw, pitch, dynent,
                                           attach, scale, zoomed);
            --render_depth_;
            on_render_done(self, dynent);
        })};
    if (md3_hook) {
        render_hook_md3_ = std::move(*md3_hook);
    } else {
        ZEPHYRUS_WARN("skeleton: md3::render hook failed: {}",
                      md3_hook.error());
    }

    auto renderclient{
        hook::function_hook<renderclient_signature>::install(
            "_Z12renderclientP9playerent", [this](void *player) {
                render_target_ = player;
                captured_this_render_ = false;
                renderclient_hook_.call_original(player);
                render_target_ = nullptr;
            })};
    if (!renderclient) {
        return std::unexpected{"failed to hook renderclient(): " +
                               renderclient.error()};
    }
    renderclient_hook_ = std::move(*renderclient);

    if (auto address{hook::resolve("_Z12renderclientP9playerent")};
        address) {
        ZEPHYRUS_TRACE("skeleton: renderclient resolved at {:#x}",
                       reinterpret_cast<std::uintptr_t>(*address));
    }

    ZEPHYRUS_INFO("skeleton capture installed (md2 vertex poses)");
    return {};
}

void skeleton_capture::shutdown() noexcept {
    render_hook_ = {};
    render_hook_md3_ = {};
    renderclient_hook_ = {};
    skeletons_.clear();
}

void skeleton_capture::clear() noexcept { skeletons_.clear(); }

void skeleton_capture::on_render_done(void *self, void *dynent) noexcept {
    if (render_depth_ == 0 && dynent != nullptr && dynent == render_target_ &&
        !captured_this_render_) {
        captured_this_render_ = true;
        capture(self, dynent);
    }
}

bool skeleton_capture::is_player(const void *dynent) const noexcept {
    if (game_ == nullptr || !game_->valid() || dynent == nullptr) {
        return false;
    }
    for (const acr::player &player : game_->players()) {
        if (player.base() == dynent) {
            return true;
        }
    }
    return false;
}

const player_skeleton *
skeleton_capture::find(const void *player) const noexcept {
    const auto it{skeletons_.find(player)};
    return it != skeletons_.end() ? &it->second : nullptr;
}

void skeleton_capture::capture(void *self, void *dynent) noexcept {
    if (self == nullptr || matrixstack_ == nullptr || lastmillis_ == nullptr ||
        !is_player(dynent)) {
        return;
    }

    // Find the part/mesh and its most recent dynamic vertex cache entry.
    auto **parts{acr::memory::read<void **>(self, vertmodel_parts)};
    const int part_count{
        acr::memory::read<int>(self, vertmodel_parts + 12)};
    if (parts == nullptr || part_count <= 0 || parts[0] == nullptr) {
        return;
    }
    void *part{parts[0]};

    auto **meshes{acr::memory::read<void **>(part, part_meshes)};
    const int mesh_count{acr::memory::read<int>(part, part_meshes + 12)};
    if (meshes == nullptr || mesh_count <= 0 || meshes[0] == nullptr) {
        return;
    }
    void *mesh{meshes[0]};

    const animstate state{
        acr::memory::read<animstate>(dynent, dynent_current)};
    const anpos wanted{frames_of(state, *lastmillis_)};

    const char *list{static_cast<const char *>(mesh) + mesh_dyncache};
    const void *list_end{list};
    const void *head{acr::memory::read<void *>(list, cache_list_next)};
    const void *chosen{nullptr};
    for (const void *entry{head}; entry != nullptr && entry != list_end;) {
        const anpos current{
            acr::memory::read<anpos>(entry, cache_entry_cur)};
        if (current.fr1 == wanted.fr1 && current.fr2 == wanted.fr2) {
            chosen = entry;
            break;
        }
        entry = acr::memory::read<void *>(entry, cache_entry_next);
    }
    if (chosen == nullptr) {
        chosen = head;
    }
    if (chosen == nullptr) {
        return;
    }

    const std::size_t entry_size{
        acr::memory::read<std::size_t>(chosen, cache_entry_size)};
    if (entry_size <= cache_entry_data + sizeof(acr::vec3)) {
        return;
    }
    // `gendynverts` allocates `numverts + 1` vertices; the extra slot only
    // holds the stencil shadow extrusion direction. Recycled cache memory can
    // leave a stale `shadowdir` (magnitude ~shadowdist) in it, so it must not
    // be treated as pose data.
    int numverts{
        static_cast<int>((entry_size - cache_entry_data) / sizeof(acr::vec3)) -
        1};
    const int mesh_verts{acr::memory::read<int>(mesh, mesh_numverts_off)};
    if (mesh_verts > 0) {
        numverts = std::min(numverts, mesh_verts);
    }
    if (numverts <= 0) {
        return;
    }

    static std::size_t frame_log{};
    if ((++frame_log % 240) == 1) {
        ZEPHYRUS_TRACE("skeleton: state anim={:#x} frames {}/{} verts {}",
                       state.anim, wanted.fr1, wanted.fr2, numverts);
    }

    float matrix[16]{};
    std::memcpy(matrix, matrixstack_, sizeof(matrix));

    const auto *local{reinterpret_cast<const acr::vec3 *>(
        static_cast<const char *>(chosen) + cache_entry_data)};

    player_skeleton &skeleton{skeletons_[dynent]};
    build_skeleton(matrix, local, numverts, skeleton);
    skeleton.valid = true;
    skeleton.sequence = ++sequence_;

    static bool logged{};
    if (!logged && skeleton.valid) {
        logged = true;
        const auto &j{skeleton.joints};
        ZEPHYRUS_INFO(
            "skeleton sample: verts={} head=({:.1f},{:.1f},{:.1f}) chest=({:.1f},{:.1f},{:.1f}) pelvis=({:.1f},{:.1f},{:.1f}) handL=({:.1f},{:.1f},{:.1f}) footL=({:.1f},{:.1f},{:.1f})",
            numverts, j[0].x, j[0].y, j[0].z, j[2].x, j[2].y, j[2].z, j[3].x,
            j[3].y, j[3].z, j[4].x, j[4].y, j[4].z, j[8].x, j[8].y, j[8].z);
    }
}

void skeleton_capture::build_skeleton(const float (&matrix)[16],
                                      const acr::vec3 *local, int numverts,
                                      player_skeleton &out) noexcept {
    if (numverts <= 0) {
        return;
    }
    static std::size_t log_counter{};
    if ((++log_counter % 240) == 1) {
        ZEPHYRUS_TRACE("skeleton: captured {} vertices", numverts);
    }

    scratch_.resize(static_cast<std::size_t>(numverts));
    float minimum_z{1.0e9F};
    float maximum_z{-1.0e9F};
    float center_x{};
    float center_y{};
    for (int i{}; i < numverts; ++i) {
        const acr::vec3 world{transform_point(matrix, local[i])};
        scratch_[static_cast<std::size_t>(i)] = world;
        minimum_z = std::min(minimum_z, world.z);
        maximum_z = std::max(maximum_z, world.z);
        center_x += world.x;
        center_y += world.y;
    }
    center_x /= static_cast<float>(numverts);
    center_y /= static_cast<float>(numverts);

    const float height{std::max(maximum_z - minimum_z, 0.01F)};
    const acr::vec3 right{normalize({matrix[0], matrix[1], matrix[2]})};

    const auto fraction{[&](const acr::vec3 &p) {
        return (p.z - minimum_z) / height;
    }};
    const auto lateral{[&](const acr::vec3 &p) {
        return (p.x - center_x) * right.x + (p.y - center_y) * right.y;
    }};

    cluster head_cluster{};
    cluster neck_cluster{};
    cluster chest_cluster{};
    cluster pelvis_cluster{};
    cluster knee_left_cluster{};
    cluster knee_right_cluster{};
    cluster foot_left_cluster{};
    cluster foot_right_cluster{};
    acr::vec3 hand_left{};
    acr::vec3 hand_right{};
    float hand_left_lateral{0.0F};
    float hand_right_lateral{0.0F};
    bool hand_left_found{false};
    bool hand_right_found{false};

    for (int i{}; i < numverts; ++i) {
        const acr::vec3 &p{scratch_[static_cast<std::size_t>(i)]};
        const float f{fraction(p)};
        if (f >= 0.90F) {
            add(head_cluster, p);
        }
        if (f >= 0.82F && f < 0.90F) {
            add(neck_cluster, p);
        }
        if (f >= 0.68F && f < 0.82F) {
            add(chest_cluster, p);
        }
        if (f >= 0.45F && f < 0.55F) {
            add(pelvis_cluster, p);
        }
        if (f >= 0.45F && f < 0.78F) {
            const float side{lateral(p)};
            if (side < hand_left_lateral) {
                hand_left_lateral = side;
                hand_left = p;
                hand_left_found = true;
            }
            if (side > hand_right_lateral) {
                hand_right_lateral = side;
                hand_right = p;
                hand_right_found = true;
            }
        }
        if (f >= 0.18F && f < 0.30F) {
            if (lateral(p) < 0.0F) {
                add(knee_left_cluster, p);
            } else {
                add(knee_right_cluster, p);
            }
        }
        if (f <= 0.06F) {
            if (lateral(p) < 0.0F) {
                add(foot_left_cluster, p);
            } else {
                add(foot_right_cluster, p);
            }
        }
    }

    using joint = skeleton_joint;
    const auto set{[&](joint j, const acr::vec3 &p) {
        out.joints[static_cast<std::size_t>(j)] = p;
    }};

    acr::vec3 head{mean(head_cluster)};
    acr::vec3 neck{mean(neck_cluster)};
    acr::vec3 chest{mean(chest_cluster)};
    acr::vec3 pelvis{mean(pelvis_cluster)};
    const acr::vec3 feet_mid{
        (mean(foot_left_cluster).x + mean(foot_right_cluster).x) * 0.5F,
        (mean(foot_left_cluster).y + mean(foot_right_cluster).y) * 0.5F,
        minimum_z};

    // Fill in gaps from neighbours so a partially occluded model still yields
    // a usable skeleton.
    const auto mix{[&](const acr::vec3 &fallback) {
        return fallback;
    }};
    if (head_cluster.count <= 0.0F) {
        head = {center_x, center_y, minimum_z + height};
    }
    if (neck_cluster.count <= 0.0F) {
        neck = {(head.x * 3.0F + pelvis.x) * 0.25F,
                (head.y * 3.0F + pelvis.y) * 0.25F,
                head.z - height * 0.10F};
    }
    if (chest_cluster.count <= 0.0F) {
        chest = {(neck.x + pelvis.x) * 0.5F, (neck.y + pelvis.y) * 0.5F,
                 (neck.z + pelvis.z) * 0.5F};
    }
    if (pelvis_cluster.count <= 0.0F) {
        pelvis = {center_x, center_y, minimum_z + height * 0.5F};
    }
    // Only synthesise a hand when no vertex of the arm band qualified;
    // overwriting a found vertex would pin the hands to a fixed height and
    // kill all arm animation.
    if (!hand_left_found) {
        hand_left = {center_x + right.x * hand_left_lateral,
                     center_y + right.y * hand_left_lateral,
                     pelvis.z + height * 0.05F};
    }
    if (!hand_right_found) {
        hand_right = {center_x + right.x * hand_right_lateral,
                      center_y + right.y * hand_right_lateral,
                      pelvis.z + height * 0.05F};
    }

    const acr::vec3 knee_left{
        knee_left_cluster.count > 0.0F ? mean(knee_left_cluster)
                                       : mix(acr::vec3{
                                             pelvis.x - right.x * height * 0.08F,
                                             pelvis.y - right.y * height * 0.08F,
                                             minimum_z + height * 0.24F})};
    const acr::vec3 knee_right{
        knee_right_cluster.count > 0.0F ? mean(knee_right_cluster)
                                        : mix(acr::vec3{
                                              pelvis.x + right.x * height * 0.08F,
                                              pelvis.y + right.y * height * 0.08F,
                                              minimum_z + height * 0.24F})};
    const acr::vec3 foot_left{
        foot_left_cluster.count > 0.0F
            ? mean(foot_left_cluster)
            : acr::vec3{feet_mid.x - right.x * height * 0.07F,
                        feet_mid.y - right.y * height * 0.07F, minimum_z}};
    const acr::vec3 foot_right{
        foot_right_cluster.count > 0.0F
            ? mean(foot_right_cluster)
            : acr::vec3{feet_mid.x + right.x * height * 0.07F,
                        feet_mid.y + right.y * height * 0.07F, minimum_z}};

    set(joint::head, head);
    set(joint::neck, neck);
    set(joint::chest, chest);
    set(joint::pelvis, pelvis);
    set(joint::hand_left, hand_left);
    set(joint::hand_right, hand_right);
    set(joint::knee_left, knee_left);
    set(joint::knee_right, knee_right);
    set(joint::foot_left, foot_left);
    set(joint::foot_right, foot_right);
}
} // namespace zephyrus::features
