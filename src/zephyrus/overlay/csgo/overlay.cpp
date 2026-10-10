// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/log.hpp>
#include <zephyrus/overlay/csgo/overlay.hpp>
#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/rmlui/backend/system_interface_sdl2.hpp>
#include <zephyrus/sdl2/sdl2.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

#include <SDL2/SDL.h>

#include <GL/gl.h>

namespace zephyrus::csgo::overlay {
namespace {
constexpr std::string_view menu_document_path{"ui/csgo/menu.rml"};

/// There is at most one overlay per process; the SDL2 trampolines are plain
/// function pointers and need a global entry point.
overlay *active_overlay{};
void *swap_original{};
void *poll_original{};

void swap_entry(SDL_Window *window) noexcept {
    if (active_overlay != nullptr) {
        active_overlay->on_present();
    }
    if (swap_original != nullptr) {
        reinterpret_cast<void (*)(SDL_Window *)>(swap_original)(window);
    }
}

int poll_entry(SDL_Event *event) noexcept {
    if (poll_original == nullptr) {
        return 0;
    }
    const int result{
        reinterpret_cast<int (*)(SDL_Event *)>(poll_original)(event)};
    if (result != 0 && active_overlay != nullptr) {
        active_overlay->on_event(*event);
    }
    return result;
}

void *relative_mode_original{};
void *warp_original{};
void *show_cursor_original{};

/// True while the menu owns the mouse: the game must not be allowed to
/// re-enable relative mode, recenter the cursor or hide it, otherwise RmlUi
/// receives frozen relative coordinates and clicks land nowhere.
bool menu_owns_mouse() noexcept {
    return active_overlay != nullptr && active_overlay->ready() &&
           active_overlay->visible();
}

int relative_mode_entry(int enabled) noexcept {
    if (enabled != 0 && menu_owns_mouse()) {
        return 0;
    }
    if (relative_mode_original == nullptr) {
        return -1;
    }
    return reinterpret_cast<int (*)(int)>(relative_mode_original)(enabled);
}

void warp_entry(SDL_Window *window, int x, int y) noexcept {
    if (menu_owns_mouse()) {
        return;
    }
    if (warp_original == nullptr) {
        return;
    }
    reinterpret_cast<void (*)(SDL_Window *, int, int)>(warp_original)(window, x,
                                                                      y);
}

int show_cursor_entry(int toggle) noexcept {
    if (menu_owns_mouse() && toggle == 0) {
        toggle = 1;
    }
    if (show_cursor_original == nullptr) {
        return -1;
    }
    return reinterpret_cast<int (*)(int)>(show_cursor_original)(toggle);
}
} // namespace

overlay::~overlay() noexcept { shutdown(); }

std::expected<void, std::string>
overlay::initialize(rmlui::rmlui_manager &rmlui) noexcept {
    if (active_overlay != nullptr) {
        return std::unexpected{std::string{"overlay already initialized"}};
    }

    auto sdl{sdl2::initialize()};
    if (!sdl) {
        return std::unexpected{"SDL2 unavailable: " + sdl.error()};
    }
    const sdl2::api *api{*sdl};

    rmlui_ = &rmlui;
    menu_key_.key = SDL_SCANCODE_INSERT;
    menu_key_.mode = keybind_mode::toggle;
    menu_key_.reset();
    active_overlay = this;

    // Present. The host's bundled SDL2 exports `FF 25` thunks; patching only
    // the slot misses callers that cached the resolved implementation, so
    // prefer an inline hook on the implementation behind the slot and keep
    // the slot swap as a fallback.
    const auto swap_function{reinterpret_cast<void *>(api->gl_swap_window)};
    auto swap_slot{hook::pointer_slot::from_function(swap_function)};
    void *swap_target{swap_function};
    if (swap_slot && swap_slot->value() != nullptr) {
        swap_target = swap_slot->value();
    }
    auto swap_hook{hook::function_hook<void(SDL_Window *)>::install(
        swap_target, [this](SDL_Window *window) {
            on_present();
            swap_inline_hook_.call_original(window);
        })};
    ZEPHYRUS_INFO(
        "swap hook: thunk={:#x} slot={:#x} target={:#x}",
        reinterpret_cast<std::uintptr_t>(swap_function),
        swap_slot ? reinterpret_cast<std::uintptr_t>(swap_slot->address()) : 0,
        reinterpret_cast<std::uintptr_t>(swap_target));
    if (swap_hook) {
        swap_inline_hook_ = std::move(*swap_hook);
    } else if (swap_slot) {
        swap_slot_ = *swap_slot;
        swap_original = swap_slot_.value();
        if (swap_original == nullptr) {
            active_overlay = nullptr;
            return std::unexpected{
                std::string{"SDL_GL_SwapWindow slot is empty"}};
        }
        swap_slot_.set(reinterpret_cast<void *>(&swap_entry));
    } else {
        active_overlay = nullptr;
        return std::unexpected{"failed to hook SDL_GL_SwapWindow: " +
                               swap_hook.error()};
    }

    // Events: same hybrid approach for SDL_PollEvent.
    const auto poll_function{reinterpret_cast<void *>(api->poll_event)};
    auto poll_slot{hook::pointer_slot::from_function(poll_function)};
    void *poll_target{poll_function};
    if (poll_slot && poll_slot->value() != nullptr) {
        poll_target = poll_slot->value();
    }
    auto poll_hook{hook::function_hook<int(SDL_Event *)>::install(
        poll_target, [this](SDL_Event *event) {
            const int result{poll_inline_hook_.call_original(event)};
            if (result != 0 && event != nullptr) {
                on_event(*event);
            }
            return result;
        })};
    if (poll_hook) {
        poll_inline_hook_ = std::move(*poll_hook);
    } else if (poll_slot) {
        poll_slot_ = *poll_slot;
        poll_original = poll_slot_.value();
        if (poll_original == nullptr) {
            shutdown();
            return std::unexpected{std::string{"SDL_PollEvent slot is empty"}};
        }
        poll_slot_.set(reinterpret_cast<void *>(&poll_entry));
    } else {
        shutdown();
        return std::unexpected{"failed to hook SDL_PollEvent: " +
                               poll_hook.error()};
    }

    // Mouse ownership: while playing, the engine constantly re-enables
    // relative mode, warps the cursor back to the centre and hides it, which
    // freezes RmlUi's pointer. Block those calls while the menu is visible.
    // Clicks themselves are sampled from SDL_GetMouseState every frame (the
    // engine consumes button events through its own paths).
    {
        const auto relative_function{
            reinterpret_cast<void *>(api->set_relative_mouse_mode)};
        if (relative_function != nullptr) {
            auto relative_slot{
                hook::pointer_slot::from_function(relative_function)};
            void *relative_target{relative_function};
            if (relative_slot && relative_slot->value() != nullptr) {
                relative_target = relative_slot->value();
            }
            auto installed{hook::function_hook<int(int)>::install(
                relative_target, [this](int enabled) {
                    if (enabled != 0 && ready_ && visible_) {
                        return 0;
                    }
                    return relative_mode_inline_hook_.call_original(enabled);
                })};
            if (installed) {
                relative_mode_inline_hook_ = std::move(*installed);
            } else if (relative_slot) {
                relative_mode_slot_ = *relative_slot;
                relative_mode_original = relative_mode_slot_.value();
                if (relative_mode_original != nullptr) {
                    relative_mode_slot_.set(
                        reinterpret_cast<void *>(&relative_mode_entry));
                }
            } else {
                ZEPHYRUS_WARN("failed to hook SDL_SetRelativeMouseMode: {}",
                              installed.error());
            }
        }
    }
    {
        const auto warp_function{
            reinterpret_cast<void *>(api->warp_mouse_in_window)};
        if (warp_function != nullptr) {
            auto warp_slot{hook::pointer_slot::from_function(warp_function)};
            void *warp_target{warp_function};
            if (warp_slot && warp_slot->value() != nullptr) {
                warp_target = warp_slot->value();
            }
            auto installed{
                hook::function_hook<void(SDL_Window *, int, int)>::install(
                    warp_target, [this](SDL_Window *window, int x, int y) {
                        if (ready_ && visible_) {
                            return;
                        }
                        warp_inline_hook_.call_original(window, x, y);
                    })};
            if (installed) {
                warp_inline_hook_ = std::move(*installed);
            } else if (warp_slot) {
                warp_slot_ = *warp_slot;
                warp_original = warp_slot_.value();
                if (warp_original != nullptr) {
                    warp_slot_.set(reinterpret_cast<void *>(&warp_entry));
                }
            } else {
                ZEPHYRUS_WARN("failed to hook SDL_WarpMouseInWindow: {}",
                              installed.error());
            }
        }
    }
    {
        const auto show_cursor_function{
            reinterpret_cast<void *>(api->show_cursor)};
        if (show_cursor_function != nullptr) {
            auto show_cursor_slot{
                hook::pointer_slot::from_function(show_cursor_function)};
            void *show_cursor_target{show_cursor_function};
            if (show_cursor_slot && show_cursor_slot->value() != nullptr) {
                show_cursor_target = show_cursor_slot->value();
            }
            auto installed{hook::function_hook<int(int)>::install(
                show_cursor_target, [this](int toggle) {
                    if (toggle == 0 && ready_ && visible_) {
                        toggle = 1;
                    }
                    return show_cursor_inline_hook_.call_original(toggle);
                })};
            if (installed) {
                show_cursor_inline_hook_ = std::move(*installed);
            } else if (show_cursor_slot) {
                show_cursor_slot_ = *show_cursor_slot;
                show_cursor_original = show_cursor_slot_.value();
                if (show_cursor_original != nullptr) {
                    show_cursor_slot_.set(
                        reinterpret_cast<void *>(&show_cursor_entry));
                }
            } else {
                ZEPHYRUS_WARN("failed to hook SDL_ShowCursor: {}",
                              installed.error());
            }
        }
    }

    ZEPHYRUS_INFO("csgo overlay hooks installed");
    return {};
}

void overlay::shutdown() noexcept {
    // Give the host its mouse back before the runtime disappears.
    if (visible_) {
        set_visible(false);
    }
    if (poll_slot_.valid() && poll_original != nullptr) {
        poll_slot_.set(poll_original);
    }
    if (swap_slot_.valid() && swap_original != nullptr) {
        swap_slot_.set(swap_original);
    }
    if (relative_mode_slot_.valid() && relative_mode_original != nullptr) {
        relative_mode_slot_.set(relative_mode_original);
    }
    if (warp_slot_.valid() && warp_original != nullptr) {
        warp_slot_.set(warp_original);
    }
    if (show_cursor_slot_.valid() && show_cursor_original != nullptr) {
        show_cursor_slot_.set(show_cursor_original);
    }
    poll_slot_ = {};
    swap_slot_ = {};
    relative_mode_slot_ = {};
    warp_slot_ = {};
    show_cursor_slot_ = {};
    poll_inline_hook_ = {};
    swap_inline_hook_ = {};
    relative_mode_inline_hook_ = {};
    warp_inline_hook_ = {};
    show_cursor_inline_hook_ = {};
    poll_original = nullptr;
    swap_original = nullptr;
    relative_mode_original = nullptr;
    warp_original = nullptr;
    show_cursor_original = nullptr;
    active_overlay = nullptr;

    on_ready = nullptr;
    esp_draw = nullptr;
    esp_enabled = nullptr;
    key_event = nullptr;
    menu_root_element_ = nullptr;
    esp_layer_ = nullptr;
    menu_.shutdown();
    menu_document_ = nullptr;
    ready_ = false;
    visible_ = false;
    runtime_failed_ = false;
}

void overlay::toggle() noexcept {
    if (ready_) {
        set_visible(!visible_);
    }
}

void overlay::initialize_runtime() noexcept {
    if (rmlui_ == nullptr) {
        return;
    }

    if (auto result{rmlui_->init()}; !result) {
        ZEPHYRUS_ERROR("overlay: rmlui init failed: {}", result.error());
        return;
    }
    if (auto result{rmlui_->load_document(menu_document_path)}; !result) {
        ZEPHYRUS_ERROR("overlay: {}", result.error());
        return;
    }

    menu_document_ = rmlui_->document();
    if (menu_document_ != nullptr) {
        menu_.initialize(*menu_document_);
        if (menu_setup) {
            menu_setup(menu_);
        }
        menu_ready_ = true;
        menu_root_element_ = menu_document_->GetElementById("menu");
        esp_layer_ = menu_document_->GetElementById("esp-layer");
        // The document stays visible so the ESP layer can draw while the
        // menu itself is hidden; `#menu` is toggled instead.
        menu_document_->Show();
        if (menu_root_element_ != nullptr) {
            menu_root_element_->SetProperty("display", "none");
        }
    }
    update_dimensions();
    ready_ = true;
    ZEPHYRUS_INFO("csgo overlay ready (INSERT toggles the menu)");
}

void overlay::refresh_menu_bindings() noexcept {
    if (!menu_ready_ || !menu_setup) {
        return;
    }
    menu_setup(menu_);
}

void overlay::update_dimensions() noexcept {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr || rmlui_ == nullptr) {
        return;
    }

    // Use the real GL drawable size: SDL may report the logical window size
    // under compositor scaling, and RmlUi's layer/filter chain needs texel
    // coordinates that match the framebuffer (otherwise composites tile the
    // menu texture and smear it over the frame).
    int width{};
    int height{};
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] > 0 && viewport[3] > 0) {
        width = viewport[2];
        height = viewport[3];
    } else if (SDL_Window *window{api->gl_get_current_window()};
               window != nullptr) {
        api->gl_get_drawable_size(window, &width, &height);
    }
    if (width <= 0 || height <= 0 || (width == width_ && height == height_)) {
        return;
    }

    {
        int window_width{};
        int window_height{};
        int drawable_width{};
        int drawable_height{};
        if (SDL_Window *window{api->gl_get_current_window()};
            window != nullptr) {
            api->get_window_size(window, &window_width, &window_height);
            api->gl_get_drawable_size(window, &drawable_width,
                                      &drawable_height);
        }
        GLint gl_viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, gl_viewport);
        ZEPHYRUS_INFO("ui size: chosen={}x{} gl_viewport={}x{} window={}x{} "
                      "drawable={}x{}",
                      width, height, gl_viewport[2], gl_viewport[3],
                      window_width, window_height, drawable_width,
                      drawable_height);
    }

    width_ = width;
    height_ = height;
    menu_.set_viewport(width, height);
    rmlui_->set_dimensions(width, height);
}

void overlay::set_visible(bool visible) noexcept {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;

    if (menu_document_ != nullptr) {
        if (menu_root_element_ != nullptr) {
            menu_root_element_->SetProperty("display",
                                            visible ? "block" : "none");
        } else if (visible_) {
            menu_document_->Show();
        } else {
            menu_document_->Hide();
        }
    }

    const sdl2::api *api{sdl2::get()};
    if (api != nullptr) {
        if (visible_) {
            // Remember the host's state once so closing the menu restores it
            // exactly: the main menu uses an absolute cursor while the game
            // grabs the mouse only in-game.
            if (!cursor_state_saved_) {
                saved_relative_mode_ =
                    api->get_relative_mouse_mode() != SDL_FALSE;
                saved_cursor_visible_ = api->show_cursor(-1);
                cursor_state_saved_ = true;
            }
            api->set_relative_mouse_mode(SDL_FALSE);
            api->show_cursor(1);
            // Seed the button tracker: a button held across the toggle must
            // not produce a phantom click on the first polled frame.
            if (api->get_mouse_state != nullptr) {
                int mouse_x{};
                int mouse_y{};
                previous_buttons_ = api->get_mouse_state(&mouse_x, &mouse_y);
            }
        } else if (cursor_state_saved_) {
            api->set_relative_mouse_mode(saved_relative_mode_ ? SDL_TRUE
                                                              : SDL_FALSE);
            api->show_cursor(saved_cursor_visible_);
            cursor_state_saved_ = false;
            // Release buttons RmlUi may still consider pressed.
            if (previous_buttons_ != 0 && rmlui_ != nullptr) {
                if (Rml::Context *context{rmlui_->context()};
                    context != nullptr) {
                    const int modifiers{backend::system_interface_sdl2::
                                            get_key_modifier_state()};
                    if ((previous_buttons_ & SDL_BUTTON_LMASK) != 0) {
                        context->ProcessMouseButtonUp(0, modifiers);
                    }
                    if ((previous_buttons_ & SDL_BUTTON_RMASK) != 0) {
                        context->ProcessMouseButtonUp(1, modifiers);
                    }
                    if ((previous_buttons_ & SDL_BUTTON_MMASK) != 0) {
                        context->ProcessMouseButtonUp(2, modifiers);
                    }
                }
            }
            previous_buttons_ = 0;
        }
    }
    ZEPHYRUS_INFO("menu {}", visible ? "opened" : "closed");
}

void overlay::on_present() noexcept {
    static bool first_present{true};
    if (first_present) {
        first_present = false;
        ZEPHYRUS_INFO("swap hook fired (first frame)");
    }

    if (!ready_) {
        if (runtime_failed_) {
            return;
        }
        initialize_runtime();
        if (!ready_) {
            runtime_failed_ = true;
            return;
        }
    }

    // Between frames on the game thread: the safe point for installing
    // inline hooks on functions the game itself calls every tick. The
    // callback clears itself once it has nothing left to do.
    if (on_ready) {
        on_ready();
    }

    update_dimensions();

    const bool esp_active{esp_enabled && esp_enabled()};

    // Skip the whole RmlUi frame when there is nothing to draw: an empty
    // composite can leave stale traces on the host framebuffer.
    if (!visible_ && !esp_active) {
        return;
    }

    if (visible_) {
        if (const sdl2::api *api{sdl2::get()}; api != nullptr) {
            // The game may re-grab the mouse every frame; keep it released
            // while the menu is up so the system cursor can move.
            api->set_relative_mouse_mode(SDL_FALSE);
            api->show_cursor(1);
        }
        poll_mouse_buttons();
    }

    // The ESP layer is cleared by `esp_draw` when ESP is disabled: without
    // that, turning it off (or dying) would leave the last boxes on screen.
    if (esp_layer_ != nullptr && esp_draw != nullptr) {
        esp_draw(*esp_layer_);
    }

    menu_.tick();

    rmlui_->update();
    Backend::BeginFrame();
    rmlui_->render();
    Backend::PresentFrame();
}

void overlay::poll_mouse_buttons() noexcept {
    const sdl2::api *api{sdl2::get()};
    if (api == nullptr || api->get_mouse_state == nullptr ||
        rmlui_ == nullptr) {
        return;
    }
    Rml::Context *context{rmlui_->context()};
    if (context == nullptr) {
        return;
    }

    int mouse_x{};
    int mouse_y{};
    const std::uint32_t buttons{api->get_mouse_state(&mouse_x, &mouse_y)};
    if (buttons == previous_buttons_) {
        return;
    }

    const int modifiers{
        backend::system_interface_sdl2::get_key_modifier_state()};
    const auto edge{[&](std::uint32_t mask, int button) {
        if ((buttons & mask) == (previous_buttons_ & mask)) {
            return;
        }
        if ((buttons & mask) != 0) {
            context->ProcessMouseButtonDown(button, modifiers);
        } else {
            context->ProcessMouseButtonUp(button, modifiers);
        }
    }};
    edge(SDL_BUTTON_LMASK, 0);
    edge(SDL_BUTTON_RMASK, 1);
    edge(SDL_BUTTON_MMASK, 2);
    previous_buttons_ = buttons;
}

void overlay::on_event(SDL_Event &event) noexcept {
    const bool input_event{
        event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
        event.type == SDL_TEXTINPUT || event.type == SDL_MOUSEMOTION ||
        event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
        event.type == SDL_MOUSEWHEEL};
    if (!input_event) {
        return;
    }

    static bool first_event{true};
    if (first_event) {
        first_event = false;
        ZEPHYRUS_INFO("poll hook fired (first event)");
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        const int scancode{static_cast<int>(event.key.keysym.scancode)};
        const bool fresh_down{event.type == SDL_KEYDOWN &&
                              event.key.repeat == 0};

        // Key capture owns the next press, including the menu key itself.
        if (fresh_down && menu_.capturing_key() &&
            menu_.capture_key_event(scancode)) {
            event.type = 0;
            return;
        }

        // Menu key: Off / Always / Hold / Toggle via the shared key binding.
        const bool matched{menu_key_.matches(scancode)};
        if (matched) {
            menu_key_.on_key(scancode, fresh_down);
        }
        if (menu_key_.mode == keybind_mode::always ||
            menu_key_.mode == keybind_mode::off) {
            // These modes apply even without a key press (the key may be 0).
            if (const bool want{menu_key_.active()}; want != visible_) {
                set_visible(want);
            }
        } else if (matched) {
            if (const bool want{menu_key_.active()}; want != visible_) {
                set_visible(want);
            }
        }
        if (matched) {
            event.type = 0;
            return;
        }

        if (key_event) {
            key_event(scancode, fresh_down);
        }
    }

    if (visible_ && ready_) {
        backend::system_interface_sdl2::handle_input_event(
            rmlui_ != nullptr ? rmlui_->context() : nullptr, event);
        // Swallow keyboard/mouse input while the menu is open so the player
        // does not move or shoot behind it.
        event.type = 0;
    }
}
} // namespace zephyrus::csgo::overlay
