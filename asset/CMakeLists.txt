# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

cmrc_add_resource_library(zephyrus-asset-ui
    ALIAS Zephyrus::Asset::UI
    NAMESPACE zephyrus::asset::ui
    ui/csgo/menu.rml
    ui/esp/esp.rml
    ui/main-menu/main-menu.rml
    ui/menu/menu.rml
)

add_library(zephyrus-asset INTERFACE)
add_library(Zephyrus::Asset ALIAS zephyrus-asset)
target_link_libraries(zephyrus-asset INTERFACE Zephyrus::Asset::UI)
