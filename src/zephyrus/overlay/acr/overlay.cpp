// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/overlay/acr/overlay.hpp>

#include <zephyrus/acr/symbols.hpp>
#include <zephyrus/log.hpp>
#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/rmlui/backend/system_interface_sdl12.hpp>
#include <zephyrus/sdl/sdl.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Input.h>

#include <GL/gl.h>
#include <GL/glext.h>
#include <SDL/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace zephyrus::overlay {
namespace {
using active_texture_fn = void (*)(GLenum);

constexpr std::size_t max_texture_units{8};

[[nodiscard]] active_texture_fn active_texture() noexcept {
    static const active_texture_fn function{[]() -> active_texture_fn {
        const sdl::api *api{sdl::get()};
        if (api == nullptr) {
            return nullptr;
        }
        if (void *symbol{api->gl_get_proc_address("glActiveTextureARB")};
            symbol != nullptr) {
            return reinterpret_cast<active_texture_fn>(symbol);
        }
        if (void *symbol{api->gl_get_proc_address("glActiveTexture")};
            symbol != nullptr) {
            return reinterpret_cast<active_texture_fn>(symbol);
        }
        return nullptr;
    }()};
    return function;
}

[[nodiscard]] bool select_texture_unit(GLint unit) noexcept {
    const active_texture_fn function{active_texture()};
    if (unit == 0) {
        if (function != nullptr) {
            function(GL_TEXTURE0_ARB);
        }
        return true;
    }
    if (function == nullptr) {
        return false;
    }
    function(static_cast<GLenum>(GL_TEXTURE0_ARB + unit));
    return true;
}

struct texture_unit_state {
    GLint mode{GL_MODULATE};
    GLint combine_rgb{GL_MODULATE};
    GLint combine_alpha{GL_MODULATE};
    GLint source[6]{};
    GLint operand[6]{};
    GLint rgb_scale{1};
    GLint alpha_scale{1};
    GLfloat color[4]{};
    GLint binding{};
};

/// Saves and restores every piece of OpenGL state the overlay touches.
///
/// The game caches texture-unit state (`tmu tmus[]`) and only re-commits on
/// changes, so the state must be restored bit-exactly instead of relying on
/// the game to fix it up.
class gl_state_guard {
  public:
    gl_state_guard() noexcept { save(); }
    ~gl_state_guard() noexcept { restore(); }

    gl_state_guard(const gl_state_guard &) = delete;
    gl_state_guard &operator=(const gl_state_guard &) = delete;
    gl_state_guard(gl_state_guard &&) = delete;
    gl_state_guard &operator=(gl_state_guard &&) = delete;

    /// Configure fixed-function state for premultiplied-alpha UI drawing.
    void apply_overlay_state() noexcept {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_FOG);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_FALSE);

        // The game (and RmlUi) may leave vertex arrays enabled; immediate-mode
        // drawing must not race with stale array pointers.
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        for (GLint unit{1}; unit < unit_count_; ++unit) {
            if (!select_texture_unit(unit)) {
                break;
            }
            glDisable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }

        (void)select_texture_unit(0);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

  private:
    void save() noexcept {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

        glGetIntegerv(GL_MATRIX_MODE, &matrix_mode_);
        for (const GLenum matrix : {GL_PROJECTION, GL_MODELVIEW, GL_TEXTURE}) {
            glMatrixMode(matrix);
            glPushMatrix();
        }

        glGetIntegerv(GL_VIEWPORT, viewport_);
        glGetIntegerv(GL_SCISSOR_BOX, scissor_);
        glGetFloatv(GL_CURRENT_COLOR, color_);
        glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &active_unit_);
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &unit_count_);
        unit_count_ = std::clamp<GLint>(unit_count_, 1,
                                        static_cast<GLint>(max_texture_units));

        for (GLint unit{}; unit < unit_count_; ++unit) {
            if (!select_texture_unit(unit)) {
                unit_count_ = unit;
                break;
            }
            auto &state{units_[static_cast<std::size_t>(unit)]};
            glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &state.mode);
            glGetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,
                          &state.combine_rgb);
            glGetTexEnviv(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB,
                          &state.combine_alpha);
            for (GLint i{}; i < 3; ++i) {
                glGetTexEnviv(GL_TEXTURE_ENV,
                              GL_SOURCE0_RGB_ARB + i * 2, &state.source[i]);
                glGetTexEnviv(GL_TEXTURE_ENV,
                              GL_OPERAND0_RGB_ARB + i * 2,
                              &state.operand[i]);
                glGetTexEnviv(GL_TEXTURE_ENV,
                              GL_SOURCE0_ALPHA_ARB + i * 2,
                              &state.source[3 + i]);
                glGetTexEnviv(GL_TEXTURE_ENV,
                              GL_OPERAND0_ALPHA_ARB + i * 2,
                              &state.operand[3 + i]);
            }
            glGetTexEnviv(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, &state.rgb_scale);
            glGetTexEnviv(GL_TEXTURE_ENV, GL_ALPHA_SCALE, &state.alpha_scale);
            glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, state.color);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.binding);
        }

        (void)select_texture_unit(active_unit_);
    }

    void restore() noexcept {
        for (GLint unit{}; unit < unit_count_; ++unit) {
            if (!select_texture_unit(unit)) {
                break;
            }
            const auto &state{units_[static_cast<std::size_t>(unit)]};
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, state.mode);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB,
                      state.combine_rgb);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB,
                      state.combine_alpha);
            for (GLint i{}; i < 3; ++i) {
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB + i * 2,
                          state.source[i]);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB + i * 2,
                          state.operand[i]);
                glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB + i * 2,
                          state.source[3 + i]);
                glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB + i * 2,
                          state.operand[3 + i]);
            }
            glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, state.rgb_scale);
            glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, state.alpha_scale);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, state.color);
            glBindTexture(GL_TEXTURE_2D,
                          static_cast<GLuint>(state.binding));
        }

        (void)select_texture_unit(active_unit_);
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glScissor(scissor_[0], scissor_[1], scissor_[2], scissor_[3]);
        glColor4fv(color_);
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(static_cast<GLenum>(matrix_mode_));
        glPopClientAttrib();
        glPopAttrib();
    }

    GLint matrix_mode_{GL_MODELVIEW};
    GLint viewport_[4]{};
    GLint scissor_[4]{};
    GLfloat color_[4]{};
    GLint active_unit_{};
    GLint unit_count_{};
    std::array<texture_unit_state, max_texture_units> units_{};
};

// ---------------------------------------------------------------------------
// TOML (de)serialization of every feature option.
// ---------------------------------------------------------------------------

/// Restrict a config name to a safe filename stem.
std::string sanitize_config_name(std::string_view name) {
    std::string result;
    result.reserve(std::min<std::size_t>(name.size(), 64));
    for (const char character : name) {
        const bool allowed{(character >= 'a' && character <= 'z') ||
                           (character >= 'A' && character <= 'Z') ||
                           (character >= '0' && character <= '9') ||
                           character == '-' || character == '_' ||
                           character == '.'};
        if (allowed) {
            result += character;
        }
        if (result.size() >= 64) {
            break;
        }
    }
    while (!result.empty() && result.front() == '.') {
        result.erase(result.begin());
    }
    return result;
}

toml::table &ensure_keys(toml::table &table) {
    toml::node *node{table.get("keys")};
    toml::table *keys{node != nullptr ? node->as_table() : nullptr};
    if (keys == nullptr) {
        auto inserted{table.insert_or_assign("keys", toml::table{})};
        keys = inserted.first->second.as_table();
    }
    return *keys;
}

toml::table rage_profile_to_table(const features::rage_profile &profile) {
    toml::table table{};
    table.insert("customized", profile.customized);
    table.insert("enabled", profile.enabled);
    table.insert("silent_aim", profile.silent_aim);
    table.insert("autofire", profile.autofire);
    table.insert("triggerbot", profile.triggerbot);
    table.insert("head_magnet", profile.head_magnet);
    table.insert("head_magnet_all", profile.head_magnet_all);
    table.insert("head_magnet_strength",
                 static_cast<double>(profile.head_magnet_strength));
    table.insert("skip_protected", profile.skip_protected);
    table.insert("require_visible", profile.require_visible);
    table.insert("team_check", profile.team_check);
    table.insert("no_recoil", profile.no_recoil);
    table.insert("auto_scope", profile.auto_scope);
    table.insert("auto_stop", profile.auto_stop);
    table.insert("between_shots", profile.between_shots);
    table.insert("priority", static_cast<int64_t>(profile.priority));
    table.insert("hitbox", static_cast<int64_t>(profile.hitbox));
    table.insert("hitchance", static_cast<int64_t>(profile.hitchance));
    table.insert("fov", static_cast<double>(profile.fov));
    table.insert("max_distance", static_cast<double>(profile.max_distance));
    return table;
}

void rage_profile_from_table(const toml::table &table,
                             features::rage_profile &profile) {
    using config::read_bool;
    using config::read_float;
    using config::read_int;
    profile.customized = read_bool(table, "customized", profile.customized);
    profile.enabled = read_bool(table, "enabled", profile.enabled);
    profile.silent_aim = read_bool(table, "silent_aim", profile.silent_aim);
    profile.autofire = read_bool(table, "autofire", profile.autofire);
    profile.triggerbot = read_bool(table, "triggerbot", profile.triggerbot);
    profile.head_magnet = read_bool(table, "head_magnet", profile.head_magnet);
    profile.head_magnet_all =
        read_bool(table, "head_magnet_all", profile.head_magnet_all);
    profile.head_magnet_strength = static_cast<float>(read_float(
        table, "head_magnet_strength", profile.head_magnet_strength));
    profile.skip_protected =
        read_bool(table, "skip_protected", profile.skip_protected);
    profile.require_visible =
        read_bool(table, "require_visible", profile.require_visible);
    profile.team_check = read_bool(table, "team_check", profile.team_check);
    profile.no_recoil = read_bool(table, "no_recoil", profile.no_recoil);
    profile.auto_scope = read_bool(table, "auto_scope", profile.auto_scope);
    profile.auto_stop = read_bool(table, "auto_stop", profile.auto_stop);
    profile.between_shots =
        read_bool(table, "between_shots", profile.between_shots);
    profile.priority = static_cast<features::rage_priority>(read_int(
        table, "priority", static_cast<int64_t>(profile.priority)));
    profile.hitbox = static_cast<features::rage_hitbox>(
        read_int(table, "hitbox", static_cast<int64_t>(profile.hitbox)));
    profile.hitchance = static_cast<int>(
        read_int(table, "hitchance", profile.hitchance));
    profile.fov =
        static_cast<float>(read_float(table, "fov", profile.fov));
    profile.max_distance = static_cast<float>(
        read_float(table, "max_distance", profile.max_distance));
}

void write_config(toml::table &root, const features::esp &esp,
                  const features::rage &rage,
                  const features::thirdperson &thirdperson,
                  const features::movement &movement,
                  const features::exploits &exploits, const keybind &menu_key,
                  const menu::geometry &geometry) {
    root.insert("version", static_cast<int64_t>(1));

    const features::esp_options &esp_options{esp.options()};
    {
        toml::table table{};
        table.insert("enabled", esp_options.enabled);
        table.insert("box", static_cast<int64_t>(esp_options.box));
        table.insert("draw_name", esp_options.draw_name);
        table.insert("draw_distance", esp_options.draw_distance);
        table.insert("draw_health", esp_options.draw_health);
        table.insert("draw_weapon", esp_options.draw_weapon);
        table.insert("draw_skeleton", esp_options.draw_skeleton);
        table.insert("glow", esp_options.glow);
        table.insert("snapline", static_cast<int64_t>(esp_options.snapline));
        table.insert("visible_only", esp_options.visible_only);
        table.insert("team_check", esp_options.team_check);
        table.insert("show_teammates", esp_options.show_teammates);
        table.insert("show_spawn_protection",
                     esp_options.show_spawn_protection);
        config::write_keybind(ensure_keys(table), "toggle",
                              esp_options.toggle_key);
        root.insert("esp", std::move(table));
    }

    {
        toml::table table{};
        table.insert("global",
                     rage_profile_to_table(rage.global_profile()));
        toml::array weapons{};
        for (int gun{}; gun < acr::gun_count; ++gun) {
            const features::rage_profile &profile{rage.profile_slot(gun)};
            if (!profile.customized) {
                continue;
            }
            toml::table entry{rage_profile_to_table(profile)};
            entry.insert("index", static_cast<int64_t>(gun));
            weapons.push_back(std::move(entry));
        }
        table.insert("weapon", std::move(weapons));
        config::write_keybind(ensure_keys(table), "toggle", rage.toggle_key);
        config::write_keybind(ensure_keys(table), "trigger",
                              rage.trigger_key);
        root.insert("rage", std::move(table));
    }

    {
        toml::table table{};
        table.insert("enabled", thirdperson.enabled());
        table.insert("distance", static_cast<double>(thirdperson.distance()));
        config::write_keybind(ensure_keys(table), "toggle",
                              thirdperson.toggle_key);
        root.insert("thirdperson", std::move(table));
    }

    {
        const features::movement_options &options{movement.options()};
        toml::table table{};
        table.insert("speedhack", options.speedhack);
        table.insert("speedhack_multiplier",
                     static_cast<double>(options.speedhack_multiplier));
        table.insert("bhop", options.bhop);
        table.insert("bhop_hold_jump", options.bhop_hold_jump);
        table.insert("air_control", options.air_control);
        table.insert("air_control_stop", options.air_control_stop);
        table.insert("air_control_boost", options.air_control_boost);
        config::write_keybind(ensure_keys(table), "toggle",
                              movement.toggle_key);
        root.insert("movement", std::move(table));
    }

    {
        const features::exploits_options &options{exploits.options()};
        toml::table table{};
        table.insert("force_scope", options.force_scope);
        table.insert("force_crouch", options.force_crouch);
        table.insert("zero_velocity", options.zero_velocity);
        table.insert("force_onfloor", options.force_onfloor);
        table.insert("knife_spacing_ms",
                     static_cast<int64_t>(options.knife_spacing_ms));
        table.insert("knife_recycle", options.knife_recycle);
        table.insert("nade_spacing_ms",
                     static_cast<int64_t>(options.nade_spacing_ms));
        table.insert("nade_at_crosshair", options.nade_at_crosshair);
        table.insert("blink_distance_m",
                     static_cast<double>(options.blink_distance_m));
        table.insert("blink_fade", options.blink_fade);
        table.insert("blink_keep_velocity", options.blink_keep_velocity);
        table.insert("tracers", options.tracers);
        table.insert("tracer_seconds",
                     static_cast<double>(options.tracer_seconds));
        table.insert("tracer_enemy", options.tracer_enemy);
        table.insert("tracer_own", options.tracer_own);
        toml::table &keys{ensure_keys(table)};
        config::write_keybind(keys, "knife", exploits.knife_key);
        config::write_keybind(keys, "nade", exploits.nade_key);
        config::write_keybind(keys, "blink", exploits.blink_key);
        config::write_keybind(keys, "blink_crosshair",
                              exploits.blink_crosshair_key);
        config::write_keybind(keys, "blink_target",
                              exploits.blink_target_key);
        root.insert("exploits", std::move(table));
    }

    {
        toml::table table{};
        config::write_keybind(ensure_keys(table), "menu", menu_key);
        if (geometry.valid) {
            table.insert("x", static_cast<int64_t>(std::lround(geometry.x)));
            table.insert("y", static_cast<int64_t>(std::lround(geometry.y)));
            table.insert("width",
                         static_cast<int64_t>(std::lround(geometry.width)));
            table.insert("height",
                         static_cast<int64_t>(std::lround(geometry.height)));
        }
        root.insert("menu", std::move(table));
    }
}

void read_config(const toml::table &root, features::esp &esp,
                 features::rage &rage, features::thirdperson &thirdperson,
                 features::movement &movement, features::exploits &exploits,
                 keybind &menu_key, menu::geometry &geometry) {
    using config::read_bool;
    using config::read_float;
    using config::read_int;

    if (const toml::table *table{config::table_at(root, "esp")};
        table != nullptr) {
        features::esp_options &options{esp.options()};
        options.enabled = read_bool(*table, "enabled", options.enabled);
        options.box = static_cast<features::esp_box>(
            read_int(*table, "box", static_cast<int64_t>(options.box)));
        options.draw_name =
            read_bool(*table, "draw_name", options.draw_name);
        options.draw_distance =
            read_bool(*table, "draw_distance", options.draw_distance);
        options.draw_health =
            read_bool(*table, "draw_health", options.draw_health);
        options.draw_weapon =
            read_bool(*table, "draw_weapon", options.draw_weapon);
        options.draw_skeleton =
            read_bool(*table, "draw_skeleton", options.draw_skeleton);
        options.glow = read_bool(*table, "glow", options.glow);
        options.snapline = static_cast<features::esp_snapline>(
            read_int(*table, "snapline",
                     static_cast<int64_t>(options.snapline)));
        options.visible_only =
            read_bool(*table, "visible_only", options.visible_only);
        options.team_check =
            read_bool(*table, "team_check", options.team_check);
        options.show_teammates =
            read_bool(*table, "show_teammates", options.show_teammates);
        options.show_spawn_protection = read_bool(
            *table, "show_spawn_protection", options.show_spawn_protection);
        if (const toml::table *keys{config::table_at(*table, "keys")};
            keys != nullptr) {
            config::read_keybind(*keys, "toggle", options.toggle_key);
        }
    }

    if (const toml::table *table{config::table_at(root, "rage")};
        table != nullptr) {
        if (const toml::table *global{config::table_at(*table, "global")};
            global != nullptr) {
            features::rage_profile profile{rage.global_profile()};
            rage_profile_from_table(*global, profile);
            profile.customized = false;
            rage.store_profile(features::rage::all_weapons, profile);
        }
        if (const toml::node *weapons{table->get("weapon")};
            weapons != nullptr) {
            if (const toml::array *array{weapons->as_array()};
                array != nullptr) {
                for (const toml::node &node : *array) {
                    const toml::table *entry{node.as_table()};
                    if (entry == nullptr) {
                        continue;
                    }
                    const int index{static_cast<int>(
                        read_int(*entry, "index", -1))};
                    if (index < 0 || index >= acr::gun_count) {
                        continue;
                    }
                    features::rage_profile profile{rage.load_profile(index)};
                    rage_profile_from_table(*entry, profile);
                    profile.customized = true;
                    rage.store_profile(index, profile);
                }
            }
        }
        if (const toml::table *keys{config::table_at(*table, "keys")};
            keys != nullptr) {
            config::read_keybind(*keys, "toggle", rage.toggle_key);
            config::read_keybind(*keys, "trigger", rage.trigger_key);
        }
    }

    if (const toml::table *table{config::table_at(root, "thirdperson")};
        table != nullptr) {
        thirdperson.set_enabled(
            read_bool(*table, "enabled", thirdperson.enabled()));
        thirdperson.set_distance(static_cast<float>(
            read_float(*table, "distance", thirdperson.distance())));
        if (const toml::table *keys{config::table_at(*table, "keys")};
            keys != nullptr) {
            config::read_keybind(*keys, "toggle", thirdperson.toggle_key);
        }
    }

    if (const toml::table *table{config::table_at(root, "movement")};
        table != nullptr) {
        features::movement_options &options{movement.options()};
        options.speedhack =
            read_bool(*table, "speedhack", options.speedhack);
        options.speedhack_multiplier = static_cast<float>(read_float(
            *table, "speedhack_multiplier", options.speedhack_multiplier));
        options.bhop = read_bool(*table, "bhop", options.bhop);
        options.bhop_hold_jump =
            read_bool(*table, "bhop_hold_jump", options.bhop_hold_jump);
        options.air_control =
            read_bool(*table, "air_control", options.air_control);
        options.air_control_stop = read_bool(*table, "air_control_stop",
                                             options.air_control_stop);
        options.air_control_boost = read_bool(*table, "air_control_boost",
                                              options.air_control_boost);
        if (const toml::table *keys{config::table_at(*table, "keys")};
            keys != nullptr) {
            config::read_keybind(*keys, "toggle", movement.toggle_key);
        }
    }

    if (const toml::table *table{config::table_at(root, "exploits")};
        table != nullptr) {
        features::exploits_options &options{exploits.options()};
        options.force_scope =
            read_bool(*table, "force_scope", options.force_scope);
        options.force_crouch =
            read_bool(*table, "force_crouch", options.force_crouch);
        options.zero_velocity =
            read_bool(*table, "zero_velocity", options.zero_velocity);
        options.force_onfloor =
            read_bool(*table, "force_onfloor", options.force_onfloor);
        options.knife_spacing_ms = static_cast<int>(read_int(
            *table, "knife_spacing_ms", options.knife_spacing_ms));
        options.knife_recycle =
            read_bool(*table, "knife_recycle", options.knife_recycle);
        options.nade_spacing_ms = static_cast<int>(read_int(
            *table, "nade_spacing_ms", options.nade_spacing_ms));
        options.nade_at_crosshair = read_bool(*table, "nade_at_crosshair",
                                              options.nade_at_crosshair);
        options.blink_distance_m = static_cast<float>(read_float(
            *table, "blink_distance_m", options.blink_distance_m));
        options.blink_fade =
            read_bool(*table, "blink_fade", options.blink_fade);
        options.blink_keep_velocity = read_bool(*table, "blink_keep_velocity",
                                                options.blink_keep_velocity);
        options.tracers = read_bool(*table, "tracers", options.tracers);
        options.tracer_seconds = static_cast<float>(
            read_float(*table, "tracer_seconds", options.tracer_seconds));
        options.tracer_enemy =
            read_bool(*table, "tracer_enemy", options.tracer_enemy);
        options.tracer_own =
            read_bool(*table, "tracer_own", options.tracer_own);
        if (const toml::table *keys{config::table_at(*table, "keys")};
            keys != nullptr) {
            config::read_keybind(*keys, "knife", exploits.knife_key);
            config::read_keybind(*keys, "nade", exploits.nade_key);
            config::read_keybind(*keys, "blink", exploits.blink_key);
            config::read_keybind(*keys, "blink_crosshair",
                                 exploits.blink_crosshair_key);
            config::read_keybind(*keys, "blink_target",
                                 exploits.blink_target_key);
        }
    }

    if (const toml::table *table{config::table_at(root, "menu")};
        table != nullptr) {
        if (const toml::table *keys{config::table_at(*table, "keys")};
            keys != nullptr) {
            config::read_keybind(*keys, "menu", menu_key);
        }
        const int64_t width{read_int(*table, "width", 0)};
        const int64_t height{read_int(*table, "height", 0)};
        if (width > 0 && height > 0) {
            geometry.x = static_cast<float>(read_float(*table, "x", 0.0));
            geometry.y = static_cast<float>(read_float(*table, "y", 0.0));
            geometry.width = static_cast<float>(width);
            geometry.height = static_cast<float>(height);
            geometry.valid = true;
        }
    }
}
} // namespace

overlay::~overlay() noexcept { shutdown(); }

std::expected<void, std::string>
overlay::initialize(rmlui::rmlui_manager &rmlui) noexcept {
    if (present_hook_.valid()) {
        return std::unexpected{"overlay is already initialized"};
    }

    menu_key_.key = SDLK_INSERT;
    menu_key_.mode = keybind_mode::toggle;

    // Action binds stay unbound by default; the movement master switch is
    // off so nothing happens until the user enables it.
    movement_.toggle_key.mode = keybind_mode::off;
    exploits_.knife_key.mode = keybind_mode::hold;
    exploits_.nade_key.mode = keybind_mode::hold;
    exploits_.blink_key.mode = keybind_mode::hold;
    exploits_.blink_crosshair_key.mode = keybind_mode::hold;
    exploits_.blink_target_key.mode = keybind_mode::hold;

    // The legacy top-level config/profiles belong to ACR: migrate them once
    // into the per-game subdirectory so CS:GO cannot pick them up.
    config::document::migrate_legacy("acr");
    config_.set_path(config::document::default_path("acr"));

    if (const char *command{std::getenv("ZEPHYRUS_ACR_CMD")};
        command != nullptr && command[0] != '\0') {
        command_path_ = command;
    } else if (const char *runtime{std::getenv("XDG_RUNTIME_DIR")};
               runtime != nullptr && runtime[0] != '\0') {
        command_path_ = std::string{runtime} + "/zephyrus-cmd";
    } else {
        command_path_ = "/tmp/zephyrus-cmd";
    }

    const sdl::api *api{sdl::get()};
    if (api == nullptr) {
        return std::unexpected{"SDL function table is not available"};
    }

    rmlui_ = &rmlui;

    auto present{hook::function_hook<void()>::install(
        reinterpret_cast<void *>(api->gl_swap_buffers), [this]() {
            on_present();
            present_hook_.call_original();
        })};
    if (!present) {
        return std::unexpected{"failed to hook SDL_GL_SwapBuffers: " +
                               present.error()};
    }
    present_hook_ = std::move(*present);

    return {};
}

void overlay::shutdown() noexcept {
    if (ready_) {
        save_config();
    }

    // Uninstall in reverse order of installation; the present hook goes last
    // so no frame can observe a half-torn-down overlay.
    warp_hook_ = {};
    grab_hook_ = {};
    mouse_hook_ = {};
    key_hook_ = {};
    present_hook_ = {};

    // Restore game state owned by the features and drop their hooks while the
    // game is still alive (the third person camera distance in particular
    // must not be left non-zero).
    exploits_.shutdown();
    movement_.shutdown();
    thirdperson_.shutdown();
    esp_.shutdown();
    rage_.shutdown();

    menu_document_ = nullptr;
    ready_ = false;
    visible_ = false;
}

bool overlay::visible() const noexcept { return visible_; }

void overlay::toggle() noexcept {
    if (ready_) {
        set_visible(!visible_);
    }
}

void overlay::set_esp(bool enabled) noexcept {
    esp_.options().enabled = enabled;
    menu_.refresh();
    ZEPHYRUS_INFO("ESP {}", enabled ? "on" : "off");
}

void overlay::set_rage(bool enabled) noexcept {
    rage_.global_profile().enabled = enabled;
    menu_.refresh();
    ZEPHYRUS_INFO("rage {}", enabled ? "on" : "off");
}

void overlay::request_bots(int count) noexcept {
    bots_requested_ = std::clamp(count, 0, 16);
    bot_spawn_cooldown_ = 0;
}

void overlay::initialize_runtime() noexcept {
    if (ready_ || rmlui_ == nullptr) {
        return;
    }

    const sdl::api *api{sdl::get()};
    if (api == nullptr) {
        ZEPHYRUS_ERROR("overlay: SDL is not available");
        return;
    }

    if (auto result{rmlui_->init()}; !result) {
        ZEPHYRUS_ERROR("overlay: rmlui init failed: {}", result.error());
        return;
    }

    auto key_hook{hook::function_hook<void(int, bool, int, int)>::install(
        acr::symbols::keypress,
        [this](int code, bool isdown, int cooked, int mod) {
            if (!on_key(code, isdown, cooked, mod)) {
                key_hook_.call_original(code, isdown, cooked, mod);
            }
        })};
    if (key_hook) {
        key_hook_ = std::move(*key_hook);
    } else {
        ZEPHYRUS_ERROR("overlay: keypress hook failed: {}", key_hook.error());
    }

    auto mouse_hook{hook::function_hook<void(int, int)>::install(
        acr::symbols::mousemove, [this](int dx, int dy) {
            if (!on_mouse_move(dx, dy)) {
                mouse_hook_.call_original(dx, dy);
            }
        })};
    if (mouse_hook) {
        mouse_hook_ = std::move(*mouse_hook);
    } else {
        ZEPHYRUS_ERROR("overlay: mousemove hook failed: {}", mouse_hook.error());
    }

    auto grab_hook{hook::function_hook<void(bool)>::install(
        acr::symbols::inputgrab, [this](bool enable) {
            grab_requested_ = enable;
            if (!visible_) {
                grab_hook_.call_original(enable);
            }
        })};
    if (grab_hook) {
        grab_hook_ = std::move(*grab_hook);
    } else {
        ZEPHYRUS_ERROR("overlay: inputgrab hook failed: {}", grab_hook.error());
    }

    if (api->warp_mouse != nullptr) {
        auto warp_hook{
            hook::function_hook<void(Uint16, Uint16)>::install(
                reinterpret_cast<void *>(api->warp_mouse),
                [this](Uint16 x, Uint16 y) {
                    if (!visible_) {
                        warp_hook_.call_original(x, y);
                    }
                })};
        if (warp_hook) {
            warp_hook_ = std::move(*warp_hook);
        } else {
            ZEPHYRUS_ERROR("overlay: SDL_WarpMouse hook failed: {}",
                           warp_hook.error());
        }
    }

    if (auto game{acr::game::create()}; game) {
        game_ = std::move(*game);
        if (auto result{esp_.initialize(*game_)}; !result) {
            ZEPHYRUS_WARN("overlay: {}", result.error());
        }
        if (auto result{rage_.initialize(*game_)}; !result) {
            ZEPHYRUS_ERROR("overlay: {}", result.error());
        }
        if (auto result{thirdperson_.initialize(*game_)}; !result) {
            ZEPHYRUS_WARN("overlay: {}", result.error());
        }
        if (auto result{movement_.initialize(*game_)}; !result) {
            ZEPHYRUS_ERROR("overlay: {}", result.error());
        }
        if (auto result{exploits_.initialize(*game_, &rage_)}; !result) {
            ZEPHYRUS_ERROR("overlay: {}", result.error());
        }
    } else {
        ZEPHYRUS_ERROR("overlay: {}", game.error());
    }

    if (auto result{rmlui_->load_document("ui/menu/menu.rml", "menu")};
        !result) {
        ZEPHYRUS_WARN("overlay: document load failed: {}", result.error());
    } else if (Rml::ElementDocument *document{rmlui_->document()};
               document != nullptr) {
        menu_document_ = document;
        menu_.initialize(
            *document, game_ ? &*game_ : nullptr, &esp_, &rage_,
            &thirdperson_, &movement_, &exploits_, &menu_key_, [this] {
                if (game_.has_value() && game_->valid()) {
                    (void)game_->add_bot();
                }
            },
            config_actions{
                .list = [this] { return list_configs(); },
                .save = [this](const std::string &name) {
                    save_config_named(name);
                },
                .load = [this](const std::string &name) {
                    load_config_named(name);
                },
                .remove = [this](const std::string &name) {
                    remove_config_named(name);
                },
                .reset = [this] { reset_config(); },
                .export_clipboard =
                    [this] { export_config_clipboard(); },
                .import_clipboard =
                    [this] { import_config_clipboard(); },
                .open_folder = [this] { open_config_folder(); }});

        // Configuration policy: defaults on startup, plus an explicit
        // profile via ZEPHYRUS_ACR_CONFIG. `latest.toml` is recovery only.
        config_ready_ = true;
        if (const char *profile{std::getenv("ZEPHYRUS_ACR_CONFIG")};
            profile != nullptr && profile[0] != '\0') {
            load_config_file(profile);
        } else {
            config_snapshot_ = current_config_dump();
        }
        menu_.refresh();
        if (!visible_) {
            document->Hide();
        }
    }

    if (auto result{rmlui_->load_document("ui/esp/esp.rml", "esp")}; !result) {
        ZEPHYRUS_WARN("overlay: esp document load failed: {}", result.error());
    } else if (Rml::ElementDocument *document{rmlui_->document()};
               document != nullptr) {
        esp_.initialize_labels(*document);
    }

    if (const char *count{std::getenv("ZEPHYRUS_ACR_BOTS")};
        count != nullptr) {
        bots_requested_ = std::clamp(std::atoi(count), 0, 16);
        ZEPHYRUS_INFO("overlay: {} bots requested", bots_requested_);
    }

    if (const char *esp{std::getenv("ZEPHYRUS_ACR_ESP")}; esp != nullptr) {
        esp_.options().enabled = std::atoi(esp) != 0;
        ZEPHYRUS_INFO("overlay: esp {}",
                      esp_.options().enabled ? "on" : "off");
    }
    if (const char *rage{std::getenv("ZEPHYRUS_ACR_RAGE")};
        rage != nullptr) {
        rage_.global_profile().enabled = std::atoi(rage) != 0;
        ZEPHYRUS_INFO("overlay: rage {}",
                      rage_.global_profile().enabled ? "on" : "off");
    }
    if (const char *test{std::getenv("ZEPHYRUS_ACR_RAGE_TEST")};
        test != nullptr && std::atoi(test) != 0) {
        auto &profile{rage_.global_profile()};
        profile.enabled = true;
        profile.require_visible = true;
        profile.team_check = true;
        profile.head_magnet = true;
        profile.auto_scope = true;
        profile.no_recoil = true;
        profile.auto_stop = true;
        profile.fov = 180.0F;
        profile.hitchance = 40;
        profile.hitbox = features::rage_hitbox::lethal;
        ZEPHYRUS_INFO("overlay: rage test mode enabled");
    }

    ready_ = true;
    ZEPHYRUS_INFO("overlay runtime ready");
}

void overlay::on_present() noexcept {
    if (!ready_) {
        initialize_runtime();
        if (!ready_) {
            return;
        }
    }

    const std::chrono::steady_clock::time_point now{
        std::chrono::steady_clock::now()};
    if (last_present_.time_since_epoch().count() != 0) {
        const float delta{
            std::chrono::duration<float>(now - last_present_).count()};
        if (delta > 0.0F) {
            fps_ = static_cast<int>(std::lround(1.0F / delta));
        }
    }
    last_present_ = now;

    thirdperson_.update();
    exploits_.update();
    rage_.set_spread_overrides(exploits_.scope_override(),
                               exploits_.crouch_override(),
                               exploits_.velocity_override());
    esp_.set_tracers(exploits_.options().tracers ? &exploits_.tracers()
                                                 : nullptr,
                     exploits_.options().tracer_seconds);
    config_.tick();
    if ((++config_tick_ % 90) == 1) {
        sync_config();
    }
    if ((++command_tick_ % 15) == 1) {
        poll_commands();
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        rmlui_->set_dimensions(viewport[2], viewport[3]);
        // Keep the interface the same physical size across resolutions: scale
        // dp units with the viewport height (Analyst tuned at 1440p HiDPI,
        // zephyrus at 1080p).
        if (Rml::Context *context{rmlui_->context()}; context != nullptr) {
            const float scale{std::clamp(
                static_cast<float>(viewport[3]) / 1080.0F, 0.8F, 1.6F)};
            if (std::abs(context->GetDensityIndependentPixelRatio() - scale) >
                0.001F) {
                context->SetDensityIndependentPixelRatio(scale);
                ZEPHYRUS_TRACE("overlay: ui scale {:.2f}", scale);
            }
        }
    }

    if (bots_spawned_ < bots_requested_ && game_.has_value() &&
        game_->valid()) {
        if (--bot_spawn_cooldown_ <= 0) {
            if (game_->add_bot()) {
                ++bots_spawned_;
                ZEPHYRUS_INFO("overlay: bot spawned ({}/{})", bots_spawned_,
                              bots_requested_);
            }
            bot_spawn_cooldown_ = 30;
        }
    }

    const bool esp_active{esp_.options().enabled &&
                          esp_.options().toggle_key.active()};
    const bool fading{exploits_.fade_alpha() > 0.0F};
    if (!visible_ && !esp_active && !fading) {
        return;
    }

    // Window dragging: follow the physical pointer 1:1 and only recentre it
    // when it gets close to an edge. Warping every frame made the cursor jump
    // to the centre on mousedown and could leave the drag stuck when the
    // button-up event was lost; recentring near an edge is enough to keep the
    // title bar attached even at screen corners (the top-left especially).
    if (visible_ && menu_.dragging() && viewport[2] > 0 && viewport[3] > 0) {
        const sdl::api *api{sdl::get()};
        int x{};
        int y{};
        const Uint8 buttons{api != nullptr ? api->get_mouse_state(&x, &y)
                                           : static_cast<Uint8>(0)};
        if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) == 0) {
            // The release event never reached the UI (seen after heavy
            // warping): end the drag explicitly so it cannot get stuck.
            if (Rml::Context *context{rmlui_->context()}; context != nullptr) {
                context->ProcessMouseButtonUp(
                    backend::system_interface_sdl12::convert_mouse_button(
                        SDL_BUTTON_LEFT),
                    backend::system_interface_sdl12::get_key_modifier_state());
            }
        } else {
            const int dx{x - drag_sample_x_};
            const int dy{y - drag_sample_y_};
            if (dx != 0 || dy != 0) {
                drag_cursor_x_ += dx;
                drag_cursor_y_ += dy;
                menu_.set_cursor(drag_cursor_x_, drag_cursor_y_);
                if (Rml::Context *context{rmlui_->context()};
                    context != nullptr) {
                    context->ProcessMouseMove(
                        drag_cursor_x_, drag_cursor_y_,
                        backend::system_interface_sdl12::
                            get_key_modifier_state());
                }
            }

            const int margin_x{
                std::max(viewport[2] / 6, 32)};
            const int margin_y{
                std::max(viewport[3] / 6, 32)};
            const bool near_edge{x < margin_x || y < margin_y ||
                                 x > viewport[2] - margin_x ||
                                 y > viewport[3] - margin_y};
            if (near_edge && warp_hook_.valid()) {
                const int center_x{viewport[2] / 2};
                const int center_y{viewport[3] / 2};
                warp_hook_.call_original(static_cast<Uint16>(center_x),
                                         static_cast<Uint16>(center_y));
                drag_sample_x_ = center_x;
                drag_sample_y_ = center_y;
            } else {
                drag_sample_x_ = x;
                drag_sample_y_ = y;
            }
        }
    }

    if (visible_) {
        update_cursor();
        menu_.set_cursor(cursor_x_, cursor_y_);
        menu_.set_viewport(viewport[2], viewport[3]);
    }

    gl_state_guard guard{};
    guard.apply_overlay_state();

    if (esp_active) {
        esp_.render(viewport[2], viewport[3]);
    }
    esp_.update_labels();
    draw_fade(viewport[2], viewport[3]);

    if (visible_) {
        menu_.sync(fps_, visible_);
    }

    Backend::BeginFrame();
    rmlui_->update();
    rmlui_->render();
    Backend::PresentFrame();
}

bool overlay::on_key(int code, bool isdown, int cooked, int mod) noexcept {
    if (!ready_) {
        return false;
    }

    // Keybind capture takes priority while the menu waits for a new bind.
    if (isdown && code > 0 && menu_.capturing()) {
        menu_.capture_key(code);
        return true;
    }

    // Feature keybinds follow the Osiris model (Off/Always/Hold/Toggle).
    if (code == SDLK_SPACE) {
        movement_.set_jump_held(isdown);
    }
    if (!visible_ && exploits_.on_key(code, isdown)) {
        return true;
    }

    esp_.options().toggle_key.on_key(code, isdown);
    rage_.toggle_key.on_key(code, isdown);
    rage_.trigger_key.on_key(code, isdown);
    thirdperson_.toggle_key.on_key(code, isdown);
    movement_.toggle_key.on_key(code, isdown);
    if (esp_.options().toggle_key.matches(code) ||
        rage_.toggle_key.matches(code) || rage_.trigger_key.matches(code) ||
        thirdperson_.toggle_key.matches(code) ||
        movement_.toggle_key.matches(code)) {
        ZEPHYRUS_TRACE(
            "overlay: keybind code={} down={} esp={} rage={} trig={} third={}",
            code, isdown, esp_.options().toggle_key.active(),
            rage_.toggle_key.active(), rage_.trigger_key.active(),
            thirdperson_.toggle_key.active());
    }

    if (thirdperson_.toggle_key.matches(code)) {
        switch (thirdperson_.toggle_key.mode) {
        case keybind_mode::toggle:
            if (isdown) {
                thirdperson_.set_enabled(!thirdperson_.enabled());
            }
            break;
        case keybind_mode::hold:
            thirdperson_.set_enabled(isdown);
            break;
        case keybind_mode::always:
            if (isdown) {
                thirdperson_.set_enabled(true);
            }
            break;
        case keybind_mode::off:
        default:
            break;
        }
        return true;
    }

    if (menu_key_.on_key(code, isdown)) {        switch (menu_key_.mode) {
        case keybind_mode::toggle:
            if (isdown) {
                set_visible(!visible_);
            }
            break;
        case keybind_mode::hold:
            set_visible(isdown);
            break;
        case keybind_mode::always:
            if (isdown) {
                set_visible(true);
            }
            break;
        case keybind_mode::off:
            break;
        }
        return true;
    }

    if (!visible_) {
        if (isdown) {
            pressed_keys_.insert(code);
        } else {
            pressed_keys_.erase(code);
        }
        return false;
    }

    if (code < 0) {
        const sdl::api *api{sdl::get()};
        if (api == nullptr) {
            return true;
        }
        int x{};
        int y{};
        api->get_mouse_state(&x, &y);
        cursor_x_ = x;
        cursor_y_ = y;
        menu_.set_cursor(x, y);
        ZEPHYRUS_TRACE("overlay: mouse event code={} down={} at {},{}", code,
                       isdown, x, y);
        Rml::Context *context{rmlui_->context()};
        if (context == nullptr) {
            return true;
        }
        context->ProcessMouseMove(x, y,
                                  backend::system_interface_sdl12::
                                      get_key_modifier_state());

        const int button{-code};
        if (button == SDL_BUTTON_WHEELUP) {
            context->ProcessMouseWheel(-1.0f, mod);
            return true;
        }
        if (button == SDL_BUTTON_WHEELDOWN) {
            context->ProcessMouseWheel(1.0f, mod);
            return true;
        }

        const int rml_button{
            backend::system_interface_sdl12::convert_mouse_button(button)};
        if (isdown) {
            context->ProcessMouseButtonDown(rml_button, mod);
            if (menu_.dragging()) {
                drag_cursor_x_ = x;
                drag_cursor_y_ = y;
                drag_sample_x_ = x;
                drag_sample_y_ = y;
            }
        } else {
            context->ProcessMouseButtonUp(rml_button, mod);
        }
        return true;
    }

    Rml::Context *context{rmlui_->context()};
    if (context == nullptr) {
        return true;
    }

    const Rml::Input::KeyIdentifier key{
        backend::system_interface_sdl12::convert_key(code)};
    if (isdown) {
        context->ProcessKeyDown(key, mod);

        if (cooked > 0) {
            char utf8[8]{};
            const auto character{static_cast<unsigned>(cooked)};
            std::size_t written{};
            if (character < 0x80) {
                utf8[0] = static_cast<char>(character);
                written = 1;
            } else if (character < 0x800) {
                utf8[0] = static_cast<char>(0xC0 | (character >> 6));
                utf8[1] = static_cast<char>(0x80 | (character & 0x3F));
                written = 2;
            } else {
                utf8[0] = static_cast<char>(0xE0 | (character >> 12));
                utf8[1] = static_cast<char>(0x80 |
                                            ((character >> 6) & 0x3F));
                utf8[2] = static_cast<char>(0x80 | (character & 0x3F));
                written = 3;
            }
            context->ProcessTextInput(
                Rml::String{utf8, static_cast<std::size_t>(written)});
        }
    } else {
        context->ProcessKeyUp(key, mod);
    }
    return true;
}

bool overlay::on_mouse_move(int dx, int dy) noexcept {
    (void)dx;
    (void)dy;
    return ready_ && visible_;
}

void overlay::set_visible(bool visible) noexcept {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;

    if (menu_document_ != nullptr) {
        if (visible_) {
            menu_document_->Show();
        } else {
            menu_document_->Hide();
        }
    }

    const sdl::api *api{sdl::get()};
    if (api == nullptr) {
        return;
    }

    if (visible_) {
        api->enable_unicode(1);
        api->wm_grab_input(SDL_GRAB_OFF);
        api->show_cursor(SDL_ENABLE);

        // The game thinks these keys are held; release their binds so the
        // player does not keep moving while the overlay is open.
        if (key_hook_.valid()) {
            for (const int code : pressed_keys_) {
                key_hook_.call_original(code, false, 0, 0);
            }
        }
        pressed_keys_.clear();

        int x{};
        int y{};
        api->get_mouse_state(&x, &y);
        cursor_x_ = x;
        cursor_y_ = y;
        if (Rml::Context *context{rmlui_->context()}; context != nullptr) {
            context->ProcessMouseMove(x, y,
                                      backend::system_interface_sdl12::
                                          get_key_modifier_state());
        }
        menu_.refresh();
        ZEPHYRUS_INFO("overlay shown");
    } else {
        api->show_cursor(grab_requested_ ? SDL_DISABLE : SDL_ENABLE);
        api->wm_grab_input(grab_requested_ ? SDL_GRAB_ON : SDL_GRAB_OFF);
        ZEPHYRUS_INFO("overlay hidden");
    }
}

void overlay::update_cursor() noexcept {
    const sdl::api *api{sdl::get()};
    if (api == nullptr || rmlui_ == nullptr) {
        return;
    }
    // The relative drag accumulator owns the cursor during a drag; the
    // absolute read would fight it (and is stuck at the window edge anyway).
    if (menu_.dragging()) {
        return;
    }
    int x{};
    int y{};
    api->get_mouse_state(&x, &y);
    if (x == cursor_x_ && y == cursor_y_) {
        return;
    }
    cursor_x_ = x;
    cursor_y_ = y;
    if (Rml::Context *context{rmlui_->context()}; context != nullptr) {
        context->ProcessMouseMove(x, y, backend::system_interface_sdl12::
                                             get_key_modifier_state());
    }
}

std::string overlay::current_config_dump() noexcept {
    toml::table root{};
    write_config(root, esp_, rage_, thirdperson_, movement_, exploits_,
                 menu_key_, menu_.window_geometry());
    std::ostringstream stream;
    stream << toml::toml_formatter{root};
    return stream.str();
}

void overlay::save_config() noexcept {
    if (!config_ready_) {
        return;
    }
    toml::table root{};
    write_config(root, esp_, rage_, thirdperson_, movement_, exploits_,
                 menu_key_, menu_.window_geometry());
    std::ostringstream stream;
    stream << toml::toml_formatter{root};
    config_snapshot_ = stream.str();
    config_.replace_root(std::move(root));
    if (config_.save()) {
        config_.clear_dirty();
        ZEPHYRUS_INFO("config: latest snapshot saved to {}",
                      config_.path().string());
    }
}

void overlay::load_config_file(const std::filesystem::path &path) noexcept {
    config::document source{};
    source.set_path(path);
    if (!source.load()) {
        ZEPHYRUS_WARN("config: cannot load {}", path.string());
        return;
    }
    menu::geometry geometry{};
    read_config(source.root(), esp_, rage_, thirdperson_, movement_,
                exploits_, menu_key_, geometry);
    if (geometry.valid) {
        menu_.apply_geometry(geometry);
    }
    config_snapshot_ = current_config_dump();
    menu_.refresh();
    ZEPHYRUS_INFO("config: applied {}", path.string());
}

void overlay::load_latest_config() noexcept {
    load_config_file(config_.path());
}

void overlay::save_profile_config() noexcept {
    if (!config_ready_) {
        return;
    }
    const auto now{std::chrono::system_clock::now()};
    const std::time_t stamp{std::chrono::system_clock::to_time_t(now)};
    std::tm broken{};
    localtime_r(&stamp, &broken);
    char name[64]{};
    std::strftime(name, sizeof(name), "zephyrus-%Y%m%d-%H%M%S.toml",
                  &broken);
    const std::filesystem::path path{config::document::profiles_dir("acr") / name};
    toml::table root{};
    write_config(root, esp_, rage_, thirdperson_, movement_, exploits_,
                 menu_key_, menu_.window_geometry());
    config::document profile{};
    profile.set_path(path);
    profile.replace_root(std::move(root));
    if (profile.save()) {
        ZEPHYRUS_INFO("config: profile written to {}", path.string());
    } else {
        ZEPHYRUS_WARN("config: failed to write profile {}", path.string());
    }
}

void overlay::reset_config() noexcept {
    esp_.options() = features::esp_options{};
    rage_.reset_profiles();
    thirdperson_.set_enabled(false);
    thirdperson_.set_distance(10.0F);
    thirdperson_.toggle_key = keybind{};
    movement_.options() = features::movement_options{};
    movement_.toggle_key = keybind{};
    exploits_.options() = features::exploits_options{};
    exploits_.knife_key = keybind{};
    exploits_.nade_key = keybind{};
    exploits_.blink_key = keybind{};
    exploits_.blink_crosshair_key = keybind{};
    exploits_.blink_target_key = keybind{};
    menu_key_.key = SDLK_INSERT;
    menu_key_.mode = keybind_mode::toggle;
    menu_key_.reset();
    movement_.toggle_key.mode = keybind_mode::off;
    exploits_.knife_key.mode = keybind_mode::hold;
    exploits_.nade_key.mode = keybind_mode::hold;
    exploits_.blink_key.mode = keybind_mode::hold;
    exploits_.blink_crosshair_key.mode = keybind_mode::hold;
    exploits_.blink_target_key.mode = keybind_mode::hold;
    menu_.refresh();
    save_config();
    ZEPHYRUS_INFO("config: reset to defaults");
}

void overlay::save_config_named(const std::string &name) noexcept {
    if (!config_ready_) {
        return;
    }
    const std::string clean{sanitize_config_name(name)};
    if (clean.empty() || clean == "latest") {
        save_config();
        return;
    }
    toml::table root{};
    write_config(root, esp_, rage_, thirdperson_, movement_, exploits_,
                 menu_key_, menu_.window_geometry());
    config::document profile{};
    profile.set_path(config::document::profiles_dir("acr") / (clean + ".toml"));
    profile.replace_root(std::move(root));
    if (profile.save()) {
        ZEPHYRUS_INFO("config: profile '{}' saved", clean);
    } else {
        ZEPHYRUS_WARN("config: failed to save profile '{}'", clean);
    }
}

std::vector<std::string> overlay::list_configs() const noexcept {
    std::vector<std::string> result;
    result.emplace_back("latest");
    std::error_code error;
    const std::filesystem::path directory{config::document::profiles_dir("acr")};
    if (std::filesystem::exists(directory, error)) {
        for (const std::filesystem::directory_entry &entry :
             std::filesystem::directory_iterator{directory, error}) {
            if (!entry.is_regular_file(error)) {
                continue;
            }
            const std::string name{entry.path().filename().string()};
            if (name.size() > 5 && name.ends_with(".toml")) {
                result.push_back(name.substr(0, name.size() - 5));
            }
        }
    }
    std::sort(result.begin() + 1, result.end());
    return result;
}

void overlay::remove_config_named(const std::string &name) noexcept {
    const std::string clean{sanitize_config_name(name)};
    if (clean.empty() || clean == "latest") {
        ZEPHYRUS_WARN("config: refusing to delete the latest snapshot");
        return;
    }
    std::error_code error;
    const std::filesystem::path path{config::document::profiles_dir("acr") /
                                     (clean + ".toml")};
    if (std::filesystem::remove(path, error)) {
        ZEPHYRUS_INFO("config: profile '{}' deleted", clean);
    } else {
        ZEPHYRUS_WARN("config: cannot delete profile '{}'", clean);
    }
    menu_.refresh();
}

void overlay::open_config_folder() noexcept {
    const std::filesystem::path directory{config::document::profiles_dir("acr")};
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    const std::string command{"xdg-open \"" + directory.string() +
                              "\" >/dev/null 2>&1 &"};
    if (std::system(command.c_str()) != 0) {
        ZEPHYRUS_WARN("config: xdg-open failed for {}", directory.string());
    }
}

void overlay::load_config_named(const std::string &name) noexcept {
    // Absolute/relative paths and explicit ".toml" names are loaded as-is;
    // everything else is a profile stem inside the profiles directory.
    if (name.find('/') != std::string::npos || name.ends_with(".toml")) {
        load_config_file(name);
        return;
    }
    const std::string clean{sanitize_config_name(name)};
    if (clean.empty() || clean == "latest") {
        load_latest_config();
        return;
    }
    const std::filesystem::path path{config::document::profiles_dir("acr") /
                                     (clean + ".toml")};
    load_config_file(path);
}

void overlay::export_config_clipboard() noexcept {
    const std::string text{current_config_dump()};
    backend::system_interface_sdl12::SetClipboard(text);
    ZEPHYRUS_INFO("config: exported to clipboard ({} bytes)", text.size());
}

void overlay::import_config_clipboard() noexcept {
    const std::string text{backend::system_interface_sdl12::GetClipboard()};
    if (text.empty()) {
        ZEPHYRUS_WARN("config: clipboard is empty");
        return;
    }
    try {
        const toml::table root{toml::parse(text)};
        menu::geometry geometry{};
        read_config(root, esp_, rage_, thirdperson_, movement_, exploits_,
                    menu_key_, geometry);
        if (geometry.valid) {
            menu_.apply_geometry(geometry);
        }
        config_snapshot_ = current_config_dump();
        menu_.refresh();
        ZEPHYRUS_INFO("config: imported from clipboard");
    } catch (const toml::parse_error &failure) {
        ZEPHYRUS_WARN("config: clipboard parse error: {}",
                      std::string{failure.description()});
    }
}

void overlay::sync_config() noexcept {
    if (!config_ready_) {
        return;
    }
    toml::table root{};
    write_config(root, esp_, rage_, thirdperson_, movement_, exploits_,
                 menu_key_, menu_.window_geometry());
    std::ostringstream stream;
    stream << toml::toml_formatter{root};
    std::string text{stream.str()};
    if (text == config_snapshot_) {
        return;
    }
    config_snapshot_ = std::move(text);
    config_.replace_root(std::move(root));
}

void overlay::poll_commands() noexcept {
    if (command_path_.empty()) {
        return;
    }
    std::error_code error;
    if (!std::filesystem::exists(command_path_, error)) {
        return;
    }
    std::ifstream stream{command_path_};
    if (!stream) {
        return;
    }
    std::string line;
    while (std::getline(stream, line)) {
        while (!line.empty() &&
               (line.back() == '\r' || line.back() == '\n' ||
                line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        const std::size_t start{line.find_first_not_of(" \t")};
        if (start == std::string::npos) {
            continue;
        }
        line = line.substr(start);
        ZEPHYRUS_INFO("overlay: command '{}'", line);
        if (line == "knife") {
            exploits_.trigger_knife();
        } else if (line.rfind("knife ", 0) == 0) {
            exploits_.trigger_knife_client(std::atoi(line.c_str() + 6));
        } else if (line == "nade") {
            exploits_.trigger_nade();
        } else if (line.rfind("nade ", 0) == 0) {
            exploits_.trigger_nade_client(std::atoi(line.c_str() + 5));
        } else if (line == "blink") {
            exploits_.trigger_blink(0);
        } else if (line == "blink_crosshair") {
            exploits_.trigger_blink(1);
        } else if (line == "blink_target") {
            exploits_.trigger_blink(2);
        } else if (line == "movement 1" || line == "movement on") {
            movement_.toggle_key.mode = keybind_mode::always;
            movement_.toggle_key.key = 0;
        } else if (line == "movement 0" || line == "movement off") {
            movement_.toggle_key.mode = keybind_mode::off;
            movement_.toggle_key.key = 0;
            movement_.toggle_key.reset();
        } else if (line == "air 1") {
            movement_.options().air_control = true;
        } else if (line == "air 0") {
            movement_.options().air_control = false;
        } else if (line == "bhop 1") {
            movement_.options().bhop = true;
        } else if (line == "bhop 0") {
            movement_.options().bhop = false;
        } else if (line == "speed 1") {
            movement_.options().speedhack = true;
        } else if (line == "speed 0") {
            movement_.options().speedhack = false;
        } else if (line.rfind("key ", 0) == 0) {
            const std::string rest{line.substr(4)};
            const std::size_t space{rest.find(' ')};
            const std::string name{
                space == std::string::npos ? rest : rest.substr(0, space)};
            const bool down{space == std::string::npos ||
                            rest.substr(space + 1) != "0"};
            static const std::unordered_map<std::string, std::string_view>
                inputs{{"forward", "_Z7forwardb"},
                       {"backward", "_Z8backwardb"},
                       {"left", "_Z4leftb"},
                       {"right", "_Z5rightb"},
                       {"jump", "_Z5jumpnb"},
                       {"attack", "_Z6attackb"}};
            const auto found{inputs.find(name)};
            if (found == inputs.end()) {
                ZEPHYRUS_WARN("overlay: unknown input '{}'", name);
            } else if (auto address{hook::resolve(found->second)}; !address) {
                ZEPHYRUS_WARN("overlay: cannot resolve input '{}': {}", name,
                              address.error());
            } else {
                reinterpret_cast<void (*)(bool)>(*address)(down);
            }
        } else if (line.rfind("goto ", 0) == 0) {
            exploits_.blink_to_client(std::atoi(line.c_str() + 5));
        } else if (line.rfind("lookat ", 0) == 0) {
            if (game_.has_value() && game_->valid()) {
                const int cn{std::atoi(line.c_str() + 7)};
                const acr::player me{game_->local_player()};
                const acr::vec3 a{me.origin()};
                for (const acr::player &entity : game_->players()) {
                    if (!entity.valid() || entity.clientnum() != cn) {
                        continue;
                    }
                    const acr::vec3 b{entity.origin()};
                    const float dx{b.x - a.x};
                    const float dy{b.y - a.y};
                    const float dz{b.z - a.z};
                    constexpr float rad2deg{57.29577951308232F};
                    const float yaw{
                        std::atan2(dx, -dy) * rad2deg};
                    const float pitch{
                        std::atan2(dz, std::sqrt(dx * dx + dy * dy)) *
                        rad2deg};
                    acr::memory::write(me.base(), acr::offsets::physent_yaw,
                                       yaw);
                    acr::memory::write(me.base(), acr::offsets::physent_pitch,
                                       pitch);
                    ZEPHYRUS_INFO("overlay: lookat {} yaw={:.1f} pitch={:.1f}",
                                  cn, static_cast<double>(yaw),
                                  static_cast<double>(pitch));
                    break;
                }
            }
        } else if (line == "players") {
            if (game_.has_value() && game_->valid()) {
                const acr::player me{game_->local_player()};
                const acr::vec3 local{me.origin()};
                for (const acr::player &entity : game_->players()) {
                    if (!entity.valid()) {
                        continue;
                    }
                    const acr::vec3 o{entity.origin()};
                    const float dx{o.x - local.x};
                    const float dy{o.y - local.y};
                    const float dz{o.z - local.z};
                    ZEPHYRUS_INFO(
                        "players: cn={} team={} alive={} hp={} prot={} d={:.0f} "
                        "o=({:.0f},{:.0f},{:.0f})",
                        entity.clientnum(), entity.team(), entity.alive(),
                        entity.health(),
                        entity.protection_ms(game_->now(),
                                             game_->spawn_protect_ms()),
                        static_cast<double>(
                            std::sqrt(dx * dx + dy * dy + dz * dz)),
                        static_cast<double>(o.x), static_cast<double>(o.y),
                        static_cast<double>(o.z));
                }
            }
        } else if (line == "save" || line == "config save") {
            save_config();
        } else if (line.rfind("config save ", 0) == 0) {
            save_config_named(line.substr(12));
        } else if (line == "config latest" || line == "config load") {
            load_latest_config();
        } else if (line.rfind("config load ", 0) == 0) {
            load_config_named(line.substr(12));
        } else if (line == "config export") {
            export_config_clipboard();
        } else if (line == "config import") {
            import_config_clipboard();
        } else if (line == "config profile") {
            save_profile_config();
        } else if (line.rfind("config delete ", 0) == 0) {
            remove_config_named(line.substr(14));
        } else if (line == "config folder") {
            open_config_folder();
        } else if (line == "config list") {
            for (const std::string &name : list_configs()) {
                ZEPHYRUS_INFO("config list: {}", name);
            }
        } else if (line == "reset" || line == "config reset") {
            reset_config();
        } else if (line == "ui reset") {
            menu_.reset_window_layout();
            save_config();
        } else if (line.rfind("ui tab ", 0) == 0) {
            menu_.show_tab(line.substr(7));
        } else if (line.rfind("ui panel ", 0) == 0) {
            menu_.show_panel(line.substr(9));
        } else if (line == "ui open") {
            set_visible(true);
        } else if (line == "ui close") {
            set_visible(false);
        } else if (line == "dump") {
            if (game_.has_value() && game_->valid()) {
                const acr::player me{game_->local_player()};
                const acr::vec3 origin{me.origin()};
                const acr::vec3 velocity{me.velocity()};
                ZEPHYRUS_INFO(
                    "dump: alive={} state={} gun={} o=({:.1f},{:.1f},{:.1f}) "
                    "vel=({:.2f},{:.2f},{:.2f}) move={} strafe={} floor={} "
                    "scope={} crouch={} hp={} | mov(sh={:.2f} bhop={} air={}) "
                    "pkt(scope={} crouch={} vel={} floor={})",
                    me.alive(), me.state(), me.weapon_type(),
                    static_cast<double>(origin.x),
                    static_cast<double>(origin.y),
                    static_cast<double>(origin.z),
                    static_cast<double>(velocity.x),
                    static_cast<double>(velocity.y),
                    static_cast<double>(velocity.z),
                    static_cast<int>(me.move_input()),
                    static_cast<int>(me.strafe_input()), me.onfloor(),
                    me.scoping(), me.crouching(), me.health(),
                    static_cast<double>(
                        movement_.options().speedhack_multiplier),
                    movement_.options().bhop,
                    movement_.options().air_control,
                    exploits_.options().force_scope,
                    exploits_.options().force_crouch,
                    exploits_.options().zero_velocity,
                    exploits_.options().force_onfloor);
            }
        } else {
            ZEPHYRUS_WARN("overlay: unknown command '{}'", line);
        }
    }
    stream.close();
    std::filesystem::remove(command_path_, error);
}

void overlay::draw_fade(int width, int height) noexcept {
    const float alpha{exploits_.fade_alpha()};
    if (alpha <= 0.001F || width <= 0 || height <= 0) {
        return;
    }
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0,
            -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0F, 0.0F, 0.0F, alpha);
    glBegin(GL_QUADS);
    glVertex2f(0.0F, 0.0F);
    glVertex2f(static_cast<float>(width), 0.0F);
    glVertex2f(static_cast<float>(width), static_cast<float>(height));
    glVertex2f(0.0F, static_cast<float>(height));
    glEnd();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
} // namespace zephyrus::overlay
