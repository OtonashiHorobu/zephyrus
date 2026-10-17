# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu
#
# CS:GO-only tree. The ACR game module lives on the `acr` branch; see
# BRANCHES.md.

cmake_minimum_required(VERSION 4.2)
project(Zephyrus LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR
   CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_definitions(NDEBUG)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(NOMINMAX)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
    endif()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        link_libraries(ws2_32 wsock32)
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "i386" OR
        CMAKE_SYSTEM_PROCESSOR MATCHES "i686")
        add_compile_definitions(_X86_)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR
        CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        add_compile_definitions(_AMD64_)
        # arm64 support may be added later
    else()
        message(FATAL_ERROR "[Zephyrus] Unknown system processor")
    endif()
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR
        CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-fvisibility=hidden)
    endif()
endif()

add_compile_options(-Wall -Wextra)

if(BUILD_TESTING)
    enable_testing()
endif()

if(NOT DEFINED RELEASE_VERSION)
    set(RELEASE_VERSION "0.0.0-dev")
endif()
if(NOT DEFINED BRANCH_NAME)
    set(BRANCH_NAME "")
endif()
if(NOT DEFINED COMMIT_HASH)
    set(COMMIT_HASH "")
endif()

message(STATUS "[Zephyrus] Zephyrus CS:GO version: ${RELEASE_VERSION} ${BRANCH_NAME}:${COMMIT_HASH}")
configure_file(version.cpp.in version.cpp @ONLY)

include(cmake/dependencies.cmake)
setup_dependencies()

# SDL2 headers only. The injected build never links SDL: at runtime all SDL
# symbols are resolved from the instance already loaded by the host game, so
# the overlay shares the exact same SDL state (event queue, video, cursor).
find_path(ZEPHYRUS_SDL2_INCLUDE_DIR "SDL2/SDL.h" REQUIRED)

add_subdirectory(asset)
add_subdirectory(test)

include_directories(
    "${PROJECT_BINARY_DIR}"
    include
)


# Shared core: everything that is game-agnostic (hook engine, config, RmlUi
# runtime, logging, version). Game modules link this and never the other way
# around.
add_library(zephyrus-core STATIC
    src/zephyrus/config/config.cpp
    src/zephyrus/hook/inline_hook.cpp
    src/zephyrus/hook/pattern.cpp
    src/zephyrus/hook/pointer_slot.cpp
    src/zephyrus/hook/resolve.cpp
    src/zephyrus/input/keybind.cpp
    src/zephyrus/log/logging_manager.cpp
    src/zephyrus/rmlui/cmrc_file_interface.cpp
    src/zephyrus/rmlui/rmlui_manager.cpp
    src/zephyrus/version.cpp
    src/zephyrus/zephyrus.cpp
    "${PROJECT_BINARY_DIR}/version.cpp"
)
add_library(Zephyrus::Core ALIAS zephyrus-core)
target_link_libraries(zephyrus-core PUBLIC
    Zephyrus::Asset
    fmt::fmt
    spdlog::spdlog
    RmlUi::Core
    boost-ext-di::boost-ext-di
    OpenGL::GL
    Zydis::Zydis
    tomlplusplus::tomlplusplus
    ${CMAKE_DL_LIBS}
)
target_compile_options(zephyrus-core PRIVATE -Werror)

# CS:GO game module: Source SDK bindings, features, overlay/menu and the SDL2
# function table / backend.
add_library(zephyrus-csgo-core STATIC
    src/zephyrus/csgo/game.cpp
    src/zephyrus/csgo/hooks/client_mode_hooks.cpp
    src/zephyrus/csgo/interfaces.cpp
    src/zephyrus/csgo/netvars.cpp
    src/zephyrus/csgo/patterns.cpp
    src/zephyrus/features/csgo/animations.cpp
    src/zephyrus/features/csgo/anti_aim.cpp
    src/zephyrus/features/csgo/autowall.cpp
    src/zephyrus/features/csgo/backtrack.cpp
    src/zephyrus/features/csgo/esp.cpp
    src/zephyrus/features/csgo/fake_lag.cpp
    src/zephyrus/features/csgo/hitchance.cpp
    src/zephyrus/features/csgo/movement.cpp
    src/zephyrus/features/csgo/rage.cpp
    src/zephyrus/features/csgo/resolver.cpp
    src/zephyrus/features/csgo/visuals.cpp
    src/zephyrus/overlay/csgo/menu.cpp
    src/zephyrus/overlay/csgo/overlay.cpp
    src/zephyrus/rmlui/backend/system_interface_sdl2.cpp
    src/zephyrus/sdl2/sdl2.cpp
)
add_library(Zephyrus::CSGOCore ALIAS zephyrus-csgo-core)
target_link_libraries(zephyrus-csgo-core PUBLIC Zephyrus::Core)
target_include_directories(zephyrus-csgo-core PUBLIC
    "${ZEPHYRUS_SDL2_INCLUDE_DIR}"
)
target_compile_options(zephyrus-csgo-core PRIVATE -Werror)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(zephyrus-csgo-core PRIVATE -fvisibility=hidden)
endif()

add_library(zephyrus-csgo SHARED
    src/zephyrus_csgo.cpp
    # The per-game backend defines the Backend:: symbols the shared core
    # references; kept in the shared target so static link order cannot drop
    # it before libzephyrus-core.a is scanned.
    src/zephyrus/rmlui/backend/backend_sdl2_gl3.cpp
    # Official RmlUi GL3 renderer (MIT): shader based, restores the host's GL
    # state around every frame. Third-party code, so not subject to -Werror.
    src/zephyrus/rmlui/backend/render_interface_gl3.cpp
)
set_source_files_properties(src/zephyrus/rmlui/backend/render_interface_gl3.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-error"
    COMPILE_DEFINITIONS "RMLUI_NUM_MSAA_SAMPLES=1")
add_library(Zephyrus::CSGO ALIAS zephyrus-csgo)
target_link_libraries(zephyrus-csgo PRIVATE Zephyrus::CSGOCore)
target_compile_options(zephyrus-csgo PRIVATE -Werror)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The injected library is dlopen()ed into a C++ process that already has
    # libstdc++: never carry a second copy, never leak symbols, and hide every
    # symbol coming from statically linked Conan archives so they cannot
    # interpose on the host game (zlib, brotli, freetype, RmlUi, ...).
    target_compile_options(zephyrus-csgo PRIVATE -fvisibility=hidden)
    target_link_options(zephyrus-csgo PRIVATE
        -Wl,--exclude-libs,ALL
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/zephyrus-csgo.map")
endif()

file(GLOB_RECURSE TEST_SRCS test/*.cpp)
foreach(test_case ${TEST_SRCS})
    get_filename_component(test_name ${test_case} NAME_WE)
    get_filename_component(test_path ${test_case} PATH)
    string(REPLACE "_" "-" test_name "${test_name}")
    message(STATUS "[Zephyrus] Adding test ${test_name} (${test_case})")
    add_executable(${test_name} ${test_case})
    target_link_libraries(${test_name} PRIVATE Zephyrus::Core Boost::ut)
    add_test(${test_name} ${PROJECT_BINARY_DIR}/${test_name})
endforeach()
target_link_libraries(test-cmrc-file-interface PRIVATE Zephyrus::Asset::Test)
