// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/asset/ui.hpp>
#include <zephyrus/hook/hook.hpp>
#include <zephyrus/log.hpp>
#include <zephyrus/overlay/acr/overlay.hpp>
#include <zephyrus/platform/detection.hpp>
#include <zephyrus/sdl/sdl.hpp>
#include <zephyrus/zephyrus.hpp>

#include <boost/di.hpp>
#include <fmt/format.h>

#include <memory>

#if ZEPHYRUS_OS_LINUX
namespace {
zephyrus::zephyrus *app{};
zephyrus::overlay::overlay *overlay{};
std::unique_ptr<zephyrus::hook::function_hook<void()>> quit_hook{};
std::unique_ptr<zephyrus::hook::function_hook<void()>> sdl_quit_hook{};

void report(std::string_view stage, std::string_view error) noexcept {
    fmt::println("[zephyrus] {}: {}", stage, error);
    ZEPHYRUS_ERROR("{}: {}", stage, error);
}

/// Tear the whole runtime down in dependency order. Idempotent.
///
/// This must run while the host's OpenGL context and SDL video subsystem are
/// still alive: RmlUi releases its render resources through the backend
/// interfaces, and the SDL cursors are owned by a live SDL. The game exit
/// hooks below call it before the game tears SDL/GL down; the library
/// destructor only reaches it as a fallback for non-standard exits.
void release_runtime() noexcept {
    delete overlay;
    overlay = nullptr;

    if (app != nullptr) {
        app->shutdown();
        delete app;
        app = nullptr;
    }
}

/// The game exits through `quit()` (normal) or `fatal()` -> `cleanup()` ->
/// `SDL_Quit()` (errors). `cleanup()` is inlined into `quit()`, so hook both
/// the explicit quit entry point and `SDL_Quit` itself, which every path has
/// to reach and which still runs with a live SDL/GL state.
zephyrus::hook::result_type install_exit_hooks() noexcept {
    auto quit{zephyrus::hook::function_hook<void()>::install(
        "_Z4quitv", []() {
            release_runtime();
            if (quit_hook != nullptr) {
                quit_hook->call_original();
            }
        })};
    if (!quit) {
        return std::unexpected{"quit hook failed: " + quit.error()};
    }
    quit_hook = std::make_unique<zephyrus::hook::function_hook<void()>>(
        std::move(*quit));

    auto sdl_quit{zephyrus::hook::function_hook<void()>::install(
        "SDL_Quit", []() {
            release_runtime();
            if (sdl_quit_hook != nullptr) {
                sdl_quit_hook->call_original();
            }
        })};
    if (!sdl_quit) {
        return std::unexpected{"SDL_Quit hook failed: " + sdl_quit.error()};
    }
    sdl_quit_hook = std::make_unique<zephyrus::hook::function_hook<void()>>(
        std::move(*sdl_quit));
    return {};
}

/// Uninstall the exit hooks (the last hooks to go).
void uninstall_exit_hooks() noexcept {
    sdl_quit_hook.reset();
    quit_hook.reset();
}
} // namespace

/// Entry point for GDB injection:
///   call (void*)zephyrus_acr_init()
/// The constructor below calls it automatically when the library is
/// `dlopen`ed, so plain `call (void*)dlopen("...zephyrus-acr.so", 2)` works.
extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_init() noexcept {
    if (app != nullptr) {
        return;
    }

    app = boost::di::make_injector(
              boost::di::bind<>.to(cmrc::zephyrus::asset::ui::get_filesystem()),
              boost::di::bind<>.to(zephyrus::backend_mode::injected))
              .create<zephyrus::zephyrus *>();

    if (auto result{app->init()}; !result) {
        report("init", result.error());
    }

    if (auto result{zephyrus::sdl::initialize()}; !result) {
        report("sdl", result.error());
    }

    overlay = new zephyrus::overlay::overlay{};
    if (auto result{overlay->initialize(app->rmlui_manager())}; !result) {
        report("overlay", result.error());
    }

    if (auto result{install_exit_hooks()}; !result) {
        report("exit hooks", result.error());
    }
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_shutdown() noexcept {
    // Unloading from inside one of our own detours would free the trampoline
    // and the owning objects that are still on the stack. Refuse; the caller
    // can retry from a safe point (the game thread outside our callbacks).
    if (zephyrus::hook::active_detour_depth() > 0) {
        fmt::println("[zephyrus] unload refused: called from inside a hook "
                     "callback, retry from a safe point");
        ZEPHYRUS_WARN("unload refused: called from inside a hook callback");
        return;
    }

    ZEPHYRUS_INFO("unloading zephyrus");
    release_runtime();

    // The exit hooks are the last ones to go; with them removed the game is
    // back to pristine code and the whole module can be `dlclose`d.
    uninstall_exit_hooks();
}

/// Test/automation hook: toggle the overlay without a keyboard event.
extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_toggle_overlay() noexcept {
    if (overlay != nullptr) {
        overlay->toggle();
    }
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_set_esp(int enabled) noexcept {
    if (overlay != nullptr) {
        overlay->set_esp(enabled != 0);
    }
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_set_rage(int enabled) noexcept {
    if (overlay != nullptr) {
        overlay->set_rage(enabled != 0);
    }
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_add_bots(int count) noexcept {
    if (overlay != nullptr) {
        overlay->request_bots(count);
    }
}

/// Test/automation hooks for the exploit actions (manual testing).
extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_knife() noexcept {
    if (overlay != nullptr) {
        overlay->exploits().trigger_knife();
    }
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_nade() noexcept {
    if (overlay != nullptr) {
        overlay->exploits().trigger_nade();
    }
}

extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_blink(int mode) noexcept {
    if (overlay != nullptr) {
        overlay->exploits().trigger_blink(mode);
    }
}

/// Enable/disable the movement master switch without touching the menu.
extern "C" __attribute__((visibility("default"))) void
zephyrus_acr_set_movement(int enabled) noexcept {
    if (overlay != nullptr) {
        auto &key{overlay->movement().toggle_key};
        key.mode = zephyrus::keybind_mode::always;
        key.key = 0;
        key.toggled = enabled != 0;
        if (enabled == 0) {
            key.mode = zephyrus::keybind_mode::off;
        }
    }
}

__attribute__((constructor)) void zephyrus_ctor() noexcept {
    zephyrus_acr_init();
}

__attribute__((destructor)) void zephyrus_dtor() noexcept {
    zephyrus_acr_shutdown();
}
#endif
