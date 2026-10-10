// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/acr/game.hpp>
#include <zephyrus/hook/hook.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace zephyrus::features {
enum class skeleton_joint : int {
    head = 0,
    neck,
    chest,
    pelvis,
    hand_left,
    hand_right,
    knee_left,
    knee_right,
    foot_left,
    foot_right,
    count,
};

struct player_skeleton {
    std::array<acr::vec3, static_cast<std::size_t>(skeleton_joint::count)>
        joints{};
    bool valid{};
    std::uint32_t sequence{};
};

/// Captures the real animated pose of player models.
///
/// ACR player models are MD2 (vertex keyframes, no bones), so the pose is
/// derived from the model's interpolated vertices: the model renders
/// (`md2::render`/`md3::render`) are hooked and the dynamic vertex cache entry
/// belonging to the current animation position is read back right after the
/// model was drawn, then transformed into world space with the model's own
/// matrix stack.
///
/// Weapons are rendered as model attachments nested inside the player model's
/// render call, so only the outermost call (depth 0) may be captured; picking
/// the first finishing call would capture the weapon mesh instead of the body.
class skeleton_capture {
  public:
    using render_signature = void(void *, int, int, float, int, const void *,
                                  float, float, void *, const void *, float,
                                  float);
    using renderclient_signature = void(void *player);

    [[nodiscard]] std::expected<void, std::string>
    initialize(const acr::game &game) noexcept;
    void shutdown() noexcept;

    [[nodiscard]] const player_skeleton *
    find(const void *player) const noexcept;

    /// Sequence number of the most recent capture. Compare with
    /// `player_skeleton::sequence` to tell whether a pose was refreshed since
    /// the caller last looked (stale poses belong to an earlier frame).
    [[nodiscard]] std::uint32_t latest_sequence() const noexcept {
        return sequence_;
    }

    /// Invalidate cached poses (e.g. on map change).
    void clear() noexcept;

  private:
    void capture(void *self, void *dynent) noexcept;
    void on_render_done(void *self, void *dynent) noexcept;
    [[nodiscard]] bool is_player(const void *dynent) const noexcept;
    void build_skeleton(const float (&matrix)[16], const acr::vec3 *local,
                        int numverts, player_skeleton &out) noexcept;

    const acr::game *game_{};
    hook::function_hook<render_signature> render_hook_;
    hook::function_hook<render_signature> render_hook_md3_;
    hook::function_hook<renderclient_signature> renderclient_hook_;
    const float *matrixstack_{};
    const int *lastmillis_{};
    void *render_target_{};
    void *render_model_{};
    bool captured_this_render_{};
    int render_depth_{};
    std::unordered_map<const void *, player_skeleton> skeletons_;
    std::uint32_t sequence_{};
    std::vector<acr::vec3> scratch_;
};
} // namespace zephyrus::features
