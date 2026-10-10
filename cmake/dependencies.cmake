# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

# All third-party dependencies are provided by Conan 2 (see conanfile.py).
# This file only re-exposes them as CMake packages to keep the top-level
# CMakeLists free of package-manager details.

function(setup_dependencies)
    find_package(fmt CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(ut CONFIG REQUIRED)
    find_package(boost-ext-di CONFIG REQUIRED)
    find_package(RmlUi CONFIG REQUIRED)
    find_package(Zydis CONFIG REQUIRED)
    find_package(cmrc CONFIG REQUIRED)
    find_package(tomlplusplus CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
endfunction()
