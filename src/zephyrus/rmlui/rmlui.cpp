// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#include <zephyrus/asset/ui.hpp>
#include <zephyrus/rmlui/rmlui.hpp>

#include <RmlUi/Core/Core.h>

namespace zephyrus::rmlui {
void rmlui::init() {
    file_interface_ = std::make_unique<cmrc_file_interface>(
        cmrc::zephyrus::asset::ui::get_filesystem());
    Rml::SetFileInterface(file_interface_.get());
}
} // namespace zephyrus::rmlui
