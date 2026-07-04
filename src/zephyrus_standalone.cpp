// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/asset/ui.hpp>
#include <zephyrus/zephyrus.hpp>

#include <RmlUi/Core/Context.h>
#include <RmlUi_Backend.h>
#include <boost/di.hpp>
#include <fmt/format.h>

int main() {
    using namespace boost::di;
    zephyrus::rmlui::cmrc_file_interface file_interface{
        cmrc::zephyrus::asset::ui::get_filesystem()};
    auto injector{make_injector(
        bind<>.to(&file_interface),
        bind<zephyrus::backend_mode>.to(zephyrus::backend_mode::standalone))};
    auto app{injector.create<zephyrus::zephyrus>()};

    if (auto result{app.init()}; !result) {
        fmt::println("{}", result.error());
        return 1;
    }

    auto *context{app.get_rmlui_context()};
    if (!context) {
        return 1;
    }

    bool running{true};
    while (running) {
        running = Backend::ProcessEvents(context);
        context->Update();

        Backend::BeginFrame();
        context->Render();
        Backend::PresentFrame();
    }

    app.shutdown();
    return 0;
}
