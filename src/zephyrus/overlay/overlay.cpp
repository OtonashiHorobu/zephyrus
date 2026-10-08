// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/overlay/overlay.hpp>

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
#include <cstdint>
#include <cstdlib>
#include <string>

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
} // namespace

overlay::~overlay() noexcept { shutdown(); }

std::expected<void, std::string>
overlay::initialize(rmlui::rmlui_manager &rmlui) noexcept {
    if (present_hook_.valid()) {
        return std::unexpected{"overlay is already initialized"};
    }

    menu_key_.key = SDLK_INSERT;
    menu_key_.mode = keybind_mode::toggle;

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
            &thirdperson_, &menu_key_, [this] {
                if (game_.has_value() && game_->valid()) {
                    (void)game_->add_bot();
                }
            });
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
    if (!visible_ && !esp_active) {
        return;
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
    esp_.options().toggle_key.on_key(code, isdown);
    rage_.toggle_key.on_key(code, isdown);
    rage_.trigger_key.on_key(code, isdown);
    thirdperson_.toggle_key.on_key(code, isdown);
    if (esp_.options().toggle_key.matches(code) ||
        rage_.toggle_key.matches(code) || rage_.trigger_key.matches(code) ||
        thirdperson_.toggle_key.matches(code)) {
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
} // namespace zephyrus::overlay
