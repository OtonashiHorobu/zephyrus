// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/asset/ui.hpp>
#include <zephyrus/features/acr/esp.hpp>
#include <zephyrus/features/acr/rage.hpp>
#include <zephyrus/overlay/acr/menu.hpp>
#include <zephyrus/rmlui/backend/backend.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <boost/di.hpp>
#include <fmt/format.h>

#include <chrono>

int main(int argc, char **argv) {
    auto injector{boost::di::make_injector(
        boost::di::bind<>.to(cmrc::zephyrus::asset::ui::get_filesystem()),
        boost::di::bind<zephyrus::backend_mode>.to(
            zephyrus::backend_mode::standalone))};
    auto app{injector.create<zephyrus::zephyrus>()};

    if (auto result{app.init()}; !result) {
        fmt::println("{}", result.error());
        return 1;
    }

    if (auto result{app.rmlui_manager().init()}; !result) {
        fmt::println("{}", result.error());
        return 1;
    }

    // Optional document argument, defaults to the overlay menu.
    const char *document_path{argc > 1 ? argv[1] : "ui/menu/menu.rml"};
    if (auto result{app.rmlui_manager().load_document(document_path)};
        !result) {
        fmt::println("{}", result.error());
    }

    auto *context{app.rmlui_manager().context()};
    if (!context) {
        return 1;
    }

    // Wire the menu controller so the standalone can drive the real widgets
    // without a running game (no game pointer, features are local instances).
    zephyrus::features::esp esp;
    zephyrus::features::rage rage;
    zephyrus::features::thirdperson thirdperson;
    zephyrus::features::movement movement;
    zephyrus::features::exploits exploits;
    zephyrus::overlay::menu menu;
    zephyrus::keybind menu_key;
    if (auto *document{app.rmlui_manager().document()}; document != nullptr) {
        menu.initialize(*document, nullptr, &esp, &rage, &thirdperson,
                        &movement, &exploits, &menu_key, [] {},
                        zephyrus::overlay::config_actions{});
    }

    bool running{true};
    auto last{std::chrono::steady_clock::now()};
    while (running) {
        running = Backend::ProcessEvents(context);

        const auto now{std::chrono::steady_clock::now()};
        const auto delta{
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last)
                .count()};
        last = now;
        const int fps{delta > 0 ? static_cast<int>(1000 / delta) : 0};
        menu.sync(fps, true);

        context->Update();

        Backend::BeginFrame();
        context->Render();
        Backend::PresentFrame();
    }

    menu.shutdown();
    app.shutdown();
    return 0;
}
