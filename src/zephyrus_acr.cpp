// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/asset/ui.hpp>
#include <zephyrus/platform/detection.hpp>
#include <zephyrus/zephyrus.hpp>

#include <boost/di.hpp>
#include <fmt/format.h>

#if ZEPHYRUS_OS_LINUX
namespace {
zephyrus::rmlui::cmrc_file_interface file_interface{
    cmrc::zephyrus::asset::ui::get_filesystem()};
auto injector{
    boost::di::make_injector(boost::di::bind<>.to(&file_interface),
                             boost::di::bind<zephyrus::backend_mode>.to(
                                 zephyrus::backend_mode::injected))};
auto app{injector.create<zephyrus::zephyrus>()};
} // namespace

__attribute__((constructor)) void zephyrus_ctor() noexcept {
    if (auto result{app.init()}; !result) {
        fmt::println("{}", result.error());
    }
}

__attribute__((destructor)) void zephyrus_dtor() noexcept { app.shutdown(); }
#endif
