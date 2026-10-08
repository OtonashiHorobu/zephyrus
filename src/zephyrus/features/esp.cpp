// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/features/esp.hpp>

#include <zephyrus/acr/offsets.hpp>
#include <zephyrus/acr/weapons.hpp>
#include <zephyrus/log.hpp>

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementUtilities.h>

#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace zephyrus::features {
namespace {
constexpr float cubes_per_meter{4.0f};
/// The game keeps health at `HEALTHSCALE` (10x) precision internally and
/// divides by it for the HUD, so 1000 internal == the familiar "100hp".
constexpr int health_scale{10};
constexpr std::size_t max_labels{32};

struct rgba {
    unsigned char r{};
    unsigned char g{};
    unsigned char b{};
};

struct screen_point {
    float x{};
    float y{};
    bool valid{};
};

struct segment {
    float x1{};
    float y1{};
    float x2{};
    float y2{};
};

screen_point project(const acr::game &game, const acr::vec3 &world,
                     int width, int height) {
    float x{};
    float y{};
    const int flags{game.world_to_screen(world, x, y)};
    if ((flags & 1) != 0 || (flags & 4) != 0) {
        return {};
    }
    return {.x = x * static_cast<float>(width),
            .y = y * static_cast<float>(height),
            .valid = true};
}

/// Emit the segments as extruded quads so the glow does not depend on the
/// driver's (often clamped) line width.
void emit_extruded(const std::vector<segment> &segments, float half_width) {
    glBegin(GL_QUADS);
    for (const segment &line : segments) {
        const float dx{line.x2 - line.x1};
        const float dy{line.y2 - line.y1};
        const float length{std::sqrt(dx * dx + dy * dy)};
        if (length < 0.01F) {
            continue;
        }
        const float nx{-dy / length * half_width};
        const float ny{dx / length * half_width};
        glVertex2f(line.x1 + nx, line.y1 + ny);
        glVertex2f(line.x2 + nx, line.y2 + ny);
        glVertex2f(line.x2 - nx, line.y2 - ny);
        glVertex2f(line.x1 - nx, line.y1 - ny);
    }
    glEnd();
}

void draw_segments(const std::vector<segment> &segments, const rgba &colour,
                   bool glow, float half_width) {
    if (segments.empty()) {
        return;
    }
    if (glow) {
        glColor4ub(colour.r, colour.g, colour.b, 26);
        emit_extruded(segments, half_width * 3.4F);
        glColor4ub(colour.r, colour.g, colour.b, 60);
        emit_extruded(segments, half_width * 2.1F);
        glColor4ub(colour.r, colour.g, colour.b, 120);
        emit_extruded(segments, half_width * 1.3F);
    }
    glColor4ub(colour.r, colour.g, colour.b, 235);
    emit_extruded(segments, half_width);
}

void draw_filled(float left, float top, float right, float bottom) {
    glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
}

std::string escape_rml(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        switch (character) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

struct pending_label {
    float center_x{};
    float name_y{};
    float meta_y{};
    float font_px{};
    float distance{};
    std::string name;
    std::string meta;
    bool visible{};
    bool enemy{};
};

std::vector<pending_label> pending_labels;

acr::vec3 add(const acr::vec3 &a, const acr::vec3 &b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
acr::vec3 mul(const acr::vec3 &a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
} // namespace

std::expected<void, std::string>
esp::initialize(const acr::game &game) noexcept {
    game_ = &game;
    if (auto result{skeletons_.initialize(game)}; !result) {
        ZEPHYRUS_WARN("esp: {}", result.error());
    }

    // The game draws its own player nametags from `drawwaypoints()`. While our
    // labels are active the timers are cleared right before that draw so the
    // stock names do not overlap the ESP.
    auto hook{hook::function_hook<void()>::install(
        "_Z13drawwaypointsv", [this]() {
            if (options_.enabled && options_.toggle_key.active() &&
                game_ != nullptr && game_->valid()) {
                const acr::player local{game_->local_player()};
                for (const acr::player &entity : game_->players()) {
                    if (entity.base() == nullptr || !entity.valid()) {
                        continue;
                    }
                    const bool teammate{
                        local.valid() && entity.team() == local.team() &&
                        (local.team() == acr::offsets::team_cla ||
                         local.team() == acr::offsets::team_rvsf)};
                    // Without teammate ESP we leave their stock tags alone.
                    if (teammate && !options_.show_teammates) {
                        continue;
                    }
                    acr::memory::write(
                        entity.base(),
                        acr::offsets::playerent_nametagmillis, 0);
                }
            }
            nametag_hook_.call_original();
        })};
    if (hook) {
        nametag_hook_ = std::move(*hook);
    } else {
        ZEPHYRUS_WARN("esp: nametag hook failed: {}", hook.error());
    }
    return {};
}

void esp::shutdown() noexcept {
    nametag_hook_ = {};
    skeletons_.shutdown();
}

void esp::initialize_labels(Rml::ElementDocument &document) noexcept {
    label_document_ = &document;
    label_layer_ = document.GetElementById("esp-layer");
    ensure_labels(max_labels);
    for (label &entry : labels_) {
        hide_label(entry);
    }
    ZEPHYRUS_INFO("esp labels ready ({} slots)", labels_.size());
}

void esp::ensure_labels(std::size_t count) noexcept {
    if (label_document_ == nullptr || label_layer_ == nullptr) {
        return;
    }
    while (labels_.size() < count) {
        Rml::ElementPtr root{label_document_->CreateElement("div")};
        root->SetClass("esp-label", true);
        root->SetClass("hidden", true);

        Rml::ElementPtr name{label_document_->CreateElement("div")};
        name->SetClass("name", true);
        Rml::Element *name_ptr{name.get()};
        root->AppendChild(std::move(name));

        Rml::Element *root_ptr{root.get()};
        label_layer_->AppendChild(std::move(root));

        Rml::ElementPtr meta_root{label_document_->CreateElement("div")};
        meta_root->SetClass("esp-label", true);
        meta_root->SetClass("hidden", true);

        Rml::ElementPtr meta{label_document_->CreateElement("div")};
        meta->SetClass("meta", true);
        Rml::Element *meta_ptr{meta.get()};
        meta_root->AppendChild(std::move(meta));

        Rml::Element *meta_root_ptr{meta_root.get()};
        label_layer_->AppendChild(std::move(meta_root));

        labels_.push_back({.root = root_ptr,
                           .name = name_ptr,
                           .meta_root = meta_root_ptr,
                           .meta = meta_ptr});
    }
}

void esp::hide_label(label &entry) noexcept {
    if (entry.root != nullptr) {
        entry.root->SetClass("hidden", true);
    }
    if (entry.meta_root != nullptr) {
        entry.meta_root->SetClass("hidden", true);
    }
}

void esp::update_labels() noexcept {
    if (label_document_ == nullptr) {
        pending_labels.clear();
        return;
    }
    ensure_labels(std::max(max_labels, pending_labels.size()));

    std::size_t index{};
    // Labels are positioned with window pixel coordinates; with a density
    // independent pixel ratio != 1 those must be converted to dp.
    const float ratio{labels_.empty() || labels_[0].root == nullptr
                          ? 1.0F
                          : Rml::ElementUtilities::GetDensityIndependentPixelRatio(
                                labels_[0].root)};
    const float inv_ratio{ratio > 0.01F ? 1.0F / ratio : 1.0F};
    for (const pending_label &data : pending_labels) {
        if (index >= labels_.size()) {
            break;
        }
        label &entry{labels_[index++]};
        for (Rml::Element *root : {entry.root, entry.meta_root}) {
            if (root == nullptr) {
                continue;
            }
            root->SetClass("hidden", false);
            root->SetClass("visible", data.visible);
            root->SetClass("occluded", !data.visible);
            root->SetClass("enemy", data.enemy);
            root->SetClass("team", !data.enemy);
        }
        // The font tracks the projected player box (see render): the label
        // stays proportional to the box at every distance. Fractional dp
        // values keep the scaling stepless instead of snapping to whole
        // density independent pixels.
        char value[32]{};
        std::snprintf(value, sizeof(value), "%.2fdp",
                      static_cast<double>(data.font_px * inv_ratio));
        entry.root->SetProperty("font-size", value);
        if (entry.meta_root != nullptr) {
            entry.meta_root->SetProperty("font-size", value);
        }
        // Centre the 280dp wide label on the player: the 140dp half width is
        // in dp, so it is subtracted after the pixel to dp conversion.
        std::snprintf(value, sizeof(value), "%.2fdp",
                      static_cast<double>(data.center_x * inv_ratio - 140.0F));
        entry.root->SetProperty("left", value);
        if (entry.meta_root != nullptr) {
            entry.meta_root->SetProperty("left", value);
        }
        std::snprintf(value, sizeof(value), "%.2fdp",
                      static_cast<double>(data.name_y * inv_ratio));
        entry.root->SetProperty("top", value);
        if (entry.meta_root != nullptr) {
            std::snprintf(value, sizeof(value), "%.2fdp",
                          static_cast<double>(data.meta_y * inv_ratio));
            entry.meta_root->SetProperty("top", value);
        }
        if (entry.name != nullptr) {
            entry.name->SetInnerRML(escape_rml(data.name));
        }
        if (entry.meta != nullptr) {
            entry.meta->SetInnerRML(escape_rml(data.meta));
        }
    }
    for (; index < labels_.size(); ++index) {
        hide_label(labels_[index]);
    }
    pending_labels.clear();
}

void esp::render(int width, int height) noexcept {
    pending_labels.clear();
    last_drawn_ = 0;

    if (!options_.enabled || !options_.toggle_key.active() || game_ == nullptr ||
        !game_->valid() || width <= 0 || height <= 0) {
        return;
    }

    const acr::player local{game_->local_player()};
    if (!local.valid()) {
        return;
    }

    const acr::vec3 camera{game_->camera_position()};

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0,
            -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    static const auto start{std::chrono::steady_clock::now()};
    const float time_seconds{
        std::chrono::duration<float>(std::chrono::steady_clock::now() - start)
            .count()};

    for (const acr::player &entity : game_->players()) {
        if (!entity.valid() || !entity.alive() ||
            entity.base() == local.base() || entity.spectating()) {
            continue;
        }

        const bool enemy{entity.team() != local.team()};
        if (options_.team_check && !enemy && !options_.show_teammates) {
            continue;
        }
        if (!enemy && !options_.show_teammates &&
            (local.team() == acr::offsets::team_cla ||
             local.team() == acr::offsets::team_rvsf)) {
            continue;
        }

        const acr::vec3 origin{entity.origin()};
        const acr::vec3 feet{entity.feet()};
        const float eye_height{entity.eyeheight()};
        const float above_eye{entity.aboveeye()};
        const float radius{std::max(entity.radius(), 0.1F)};
        acr::vec3 head{entity.head()};
        if (!entity.head_valid()) {
            head = {origin.x, origin.y, origin.z + above_eye * 0.35F};
        }

        const screen_point head_point{project(*game_, head, width, height)};
        const screen_point feet_point{project(*game_, feet, width, height)};
        if (!head_point.valid || !feet_point.valid) {
            continue;
        }

        const bool visible{game_->visible(camera, head)};
        if (options_.visible_only && !visible) {
            continue;
        }

        const rgba colour{enemy ? (visible ? rgba{80, 220, 130}
                                           : rgba{255, 105, 105})
                                : rgba{60, 157, 255}};

        float top{std::min(head_point.y, feet_point.y)};
        float bottom{std::max(head_point.y, feet_point.y)};
        float left{std::min(head_point.x, feet_point.x)};
        float right{std::max(head_point.x, feet_point.x)};

        if (options_.box == esp_box::box_2d ||
            options_.box == esp_box::corners_2d) {
            const float box_height{std::max(bottom - top, 6.0F)};
            const float half_width{box_height * 0.32F};
            left = head_point.x - half_width;
            right = head_point.x + half_width;
        }

        std::vector<segment> box_segments;
        if (options_.box == esp_box::box_2d ||
            options_.box == esp_box::corners_2d) {
            if (options_.box == esp_box::box_2d) {
                box_segments.push_back({left, top, right, top});
                box_segments.push_back({right, top, right, bottom});
                box_segments.push_back({right, bottom, left, bottom});
                box_segments.push_back({left, bottom, left, top});
            } else {
                const float corner_width{(right - left) * 0.28F};
                const float corner_height{(bottom - top) * 0.28F};
                box_segments.push_back({left, top, left + corner_width, top});
                box_segments.push_back({left, top, left, top + corner_height});
                box_segments.push_back(
                    {right, top, right - corner_width, top});
                box_segments.push_back(
                    {right, top, right, top + corner_height});
                box_segments.push_back(
                    {left, bottom, left + corner_width, bottom});
                box_segments.push_back(
                    {left, bottom, left, bottom - corner_height});
                box_segments.push_back(
                    {right, bottom, right - corner_width, bottom});
                box_segments.push_back(
                    {right, bottom, right, bottom - corner_height});
            }
        } else if (options_.box == esp_box::box_3d ||
                   options_.box == esp_box::corners_3d) {
            const float box_top{origin.z + above_eye};
            const float box_bottom{origin.z - eye_height};
            std::array<screen_point, 8> corners{};
            std::size_t corner_index{};
            float minimum_x{1.0e9F};
            float minimum_y{1.0e9F};
            float maximum_x{-1.0e9F};
            float maximum_y{-1.0e9F};
            bool corners_valid{true};
            for (const float dx : {-radius, radius}) {
                for (const float dy : {-radius, radius}) {
                    for (const float z : {box_bottom, box_top}) {
                        corners[corner_index] = project(
                            *game_, {origin.x + dx, origin.y + dy, z}, width,
                            height);
                        if (!corners[corner_index].valid) {
                            corners_valid = false;
                            break;
                        }
                        minimum_x = std::min(minimum_x, corners[corner_index].x);
                        minimum_y = std::min(minimum_y, corners[corner_index].y);
                        maximum_x = std::max(maximum_x, corners[corner_index].x);
                        maximum_y = std::max(maximum_y, corners[corner_index].y);
                        ++corner_index;
                    }
                }
            }
            if (corners_valid) {
                left = minimum_x;
                top = minimum_y;
                right = maximum_x;
                bottom = maximum_y;

                static constexpr std::array<std::array<int, 2>, 12> edges{{
                    {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7},
                    {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7},
                }};
                for (const auto &[a, b] : edges) {
                    const screen_point &p{corners[a]};
                    const screen_point &q{corners[b]};
                    if (options_.box == esp_box::corners_3d) {
                        const float trim{0.22F};
                        box_segments.push_back(
                            {p.x + (q.x - p.x) * trim, p.y + (q.y - p.y) * trim,
                             p.x + (q.x - p.x) * (1.0F - trim),
                             p.y + (q.y - p.y) * (1.0F - trim)});
                    } else {
                        box_segments.push_back({p.x, p.y, q.x, q.y});
                    }
                }
            }
        }

        const float center_x{(left + right) * 0.5F};

        // Real animated skeleton when the model was rendered this frame,
        // otherwise a state driven approximation.
        std::vector<segment> skeleton_segments;
        if (options_.draw_skeleton) {
            const player_skeleton *captured{
                skeletons_.find(entity.base())};
            // Poses older than the previous overlay frame belong to a player
            // whose model is not rendered (occluded); drawing them would
            // freeze the skeleton in place. Fall back to the live position.
            if (captured != nullptr && captured->valid &&
                captured->sequence > last_skeleton_sequence_) {
                const auto line = [&](skeleton_joint from, skeleton_joint to) {
                    const acr::vec3 &a{
                        captured->joints[static_cast<std::size_t>(from)]};
                    const acr::vec3 &b{
                        captured->joints[static_cast<std::size_t>(to)]};
                    const screen_point pa{project(*game_, a, width, height)};
                    const screen_point pb{project(*game_, b, width, height)};
                    if (pa.valid && pb.valid) {
                        skeleton_segments.push_back(
                            {pa.x, pa.y, pb.x, pb.y});
                    }
                };
                line(skeleton_joint::head, skeleton_joint::neck);
                line(skeleton_joint::neck, skeleton_joint::chest);
                line(skeleton_joint::chest, skeleton_joint::pelvis);
                line(skeleton_joint::neck, skeleton_joint::hand_left);
                line(skeleton_joint::neck, skeleton_joint::hand_right);
                line(skeleton_joint::pelvis, skeleton_joint::knee_left);
                line(skeleton_joint::knee_left, skeleton_joint::foot_left);
                line(skeleton_joint::pelvis, skeleton_joint::knee_right);
                line(skeleton_joint::knee_right, skeleton_joint::foot_right);
            } else {
                const float total_height{
                    std::max(eye_height + above_eye, 0.5F)};
                const float yaw{entity.yaw() * 3.14159265358979323846F /
                                180.0F};
                const acr::vec3 forward{std::cos(yaw), std::sin(yaw), 0.0F};
                const acr::vec3 side{-forward.y, forward.x, 0.0F};

                const acr::vec3 pelvis{
                    feet.x, feet.y, feet.z + total_height * 0.50F};
                const acr::vec3 neck{origin.x, origin.y,
                                     feet.z + total_height * 0.84F};

                const acr::vec3 velocity{entity.velocity()};
                const float speed{std::sqrt(velocity.x * velocity.x +
                                            velocity.y * velocity.y)};
                const float swing{
                    std::sin(time_seconds * (2.2F + speed * 0.15F) *
                             3.14159265358979323846F) *
                    std::min(speed, 3.0F) * 0.06F};

                const acr::vec3 hand_left{add(
                    add(pelvis, mul(side, -total_height * 0.12F)),
                    add(mul(forward, swing),
                        {0.0F, 0.0F, -total_height * 0.06F}))};
                const acr::vec3 hand_right{add(
                    add(pelvis, mul(side, total_height * 0.12F)),
                    add(mul(forward, -swing),
                        {0.0F, 0.0F, -total_height * 0.06F}))};
                const acr::vec3 foot_left{
                    add(add(feet, mul(side, -total_height * 0.07F)),
                        mul(forward, swing))};
                const acr::vec3 foot_right{
                    add(add(feet, mul(side, total_height * 0.07F)),
                        mul(forward, -swing))};

                const auto line = [&](const acr::vec3 &a, const acr::vec3 &b) {
                    const screen_point pa{project(*game_, a, width, height)};
                    const screen_point pb{project(*game_, b, width, height)};
                    if (pa.valid && pb.valid) {
                        skeleton_segments.push_back(
                            {pa.x, pa.y, pb.x, pb.y});
                    }
                };
                line({head.x, head.y, head.z + total_height * 0.06F}, neck);
                line(neck, pelvis);
                line(neck, hand_left);
                line(neck, hand_right);
                line(pelvis, foot_left);
                line(pelvis, foot_right);
            }
        }

        if (!box_segments.empty()) {
            draw_segments(box_segments, colour, options_.glow, 1.1F);
        }
        if (!skeleton_segments.empty()) {
            draw_segments(skeleton_segments, {228, 235, 245}, options_.glow,
                          0.9F);
        }

        if (options_.draw_health && entity.health() > 0) {
            const float fraction{
                std::clamp(static_cast<float>(entity.health()) /
                               static_cast<float>(100 * health_scale),
                           0.0F, 1.0F)};
            const float bar_left{left - 7.0F};
            const float bar_right{left - 2.0F};
            glColor4ub(0, 0, 0, 150);
            draw_filled(bar_left - 1.0F, top - 1.0F, bar_right + 1.0F,
                        bottom + 1.0F);
            glColor4ub(static_cast<GLubyte>(255.0F * (1.0F - fraction)),
                       static_cast<GLubyte>(220.0F * fraction), 60, 235);
            draw_filled(bar_left, bottom - (bottom - top) * fraction,
                        bar_right, bottom);
        }

        if (options_.snapline != esp_snapline::none) {
            const float origin_x{static_cast<float>(width) * 0.5F};
            float origin_y{static_cast<float>(height)};
            if (options_.snapline == esp_snapline::top) {
                origin_y = 0.0F;
            } else if (options_.snapline == esp_snapline::crosshair) {
                origin_y = static_cast<float>(height) * 0.5F;
            }
            const std::vector<segment> snap{
                {origin_x, origin_y, center_x, top}};
            draw_segments(snap, colour, options_.glow, 0.8F);
        }

        // ---- text labels (rendered by the RmlUi overlay) ----
        {
            const acr::vec3 offset{origin.x - camera.x, origin.y - camera.y,
                                   origin.z - camera.z};
            const float meters{
                std::sqrt(offset.x * offset.x + offset.y * offset.y +
                          offset.z * offset.z) /
                cubes_per_meter};

            std::string meta;
            if (options_.draw_weapon) {
                meta += acr::weapon_name(entity.weapon_type());
            }
            if (options_.draw_health && entity.health() > 0) {
                if (!meta.empty()) {
                    meta += "  ";
                }
                meta += std::to_string(entity.health() / health_scale) + "hp";
            }
            if (options_.draw_distance) {
                if (!meta.empty()) {
                    meta += "  ";
                }
                char distance_text[32]{};
                std::snprintf(distance_text, sizeof(distance_text), "%.0fm",
                              static_cast<double>(meters));
                meta += distance_text;
            }

            // The label size tracks the projected player box so the text stays
            // proportional to it at every distance (the stock nametags grow
            // with the model as well). The name hugs the top of the box, the
            // weapon/health/distance line the bottom.
            const float box_height{std::max(bottom - top, 6.0F)};
            const float font_px{std::clamp(box_height * 0.20F, 8.0F, 22.0F)};
            pending_labels.push_back({.center_x = center_x,
                                      .name_y = top - font_px - 6.0F,
                                      .meta_y = bottom + 6.0F,
                                      .font_px = font_px,
                                      .distance = meters,
                                      .name = std::string{entity.name()},
                                      .meta = std::move(meta),
                                      .visible = visible,
                                      .enemy = enemy});
        }

        ++last_drawn_;
    }

    last_skeleton_sequence_ = skeletons_.latest_sequence();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    static std::size_t frame{};
    if ((++frame % 240) == 1) {
        ZEPHYRUS_TRACE("esp: {} drawn ({} players)", last_drawn_,
                       game_->players().size());
    }
}
} // namespace zephyrus::features
