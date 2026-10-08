# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

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
        message(FATAL_ERROR "[Zephyrus ACR] Unknown system processor")
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

message(STATUS "[Zephyrus ACR] Zephyrus ACR version: ${RELEASE_VERSION} ${BRANCH_NAME}:${COMMIT_HASH}")
configure_file(version.cpp.in version.cpp @ONLY)

include(cmake/dependencies.cmake)
setup_dependencies()

# SDL 1.2 headers only. The injected build never links SDL: at runtime all SDL
# symbols are resolved from the instance already loaded by the host game, so
# the overlay shares the exact same SDL state (event queue, video, cursor).
find_path(ZEPHYRUS_SDL_INCLUDE_DIR "SDL/SDL.h" REQUIRED)

add_subdirectory(asset)
add_subdirectory(test)

include_directories(
    "${PROJECT_BINARY_DIR}"
    include
)


add_library(zephyrus-core STATIC
    src/zephyrus/acr/game.cpp
    src/zephyrus/features/esp.cpp
    src/zephyrus/features/rage.cpp
    src/zephyrus/features/skeleton.cpp
    src/zephyrus/features/thirdperson.cpp
    src/zephyrus/hook/inline_hook.cpp
    src/zephyrus/hook/pattern.cpp
    src/zephyrus/hook/resolve.cpp
    src/zephyrus/input/keybind.cpp
    src/zephyrus/log/logging_manager.cpp
    src/zephyrus/overlay/menu.cpp
    src/zephyrus/overlay/overlay.cpp
    src/zephyrus/rmlui/backend/backend_sdl12_gl2.cpp
    src/zephyrus/rmlui/backend/render_interface_gl2.cpp
    src/zephyrus/rmlui/backend/system_interface_sdl12.cpp
    src/zephyrus/rmlui/cmrc_file_interface.cpp
    src/zephyrus/rmlui/rmlui_manager.cpp
    src/zephyrus/sdl/sdl.cpp
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
    ${CMAKE_DL_LIBS}
)
target_include_directories(zephyrus-core PUBLIC
    "${ZEPHYRUS_SDL_INCLUDE_DIR}"
)
target_compile_options(zephyrus-core PRIVATE -Werror)

add_executable(zephyrus-standalone src/zephyrus_standalone.cpp)
target_link_libraries(zephyrus-standalone PRIVATE Zephyrus::Core)
target_compile_options(zephyrus-standalone PRIVATE -Werror)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Standalone owns its process, so the static C++ runtime is safe here.
    target_link_options(zephyrus-standalone PRIVATE
        -static-libstdc++ -static-libgcc)
endif()

add_library(zephyrus-acr SHARED src/zephyrus_acr.cpp)
add_library(Zephyrus::ACR ALIAS zephyrus-acr)
target_link_libraries(zephyrus-acr PRIVATE Zephyrus::Core)
target_compile_options(zephyrus-acr PRIVATE -Werror)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The injected library is dlopen()ed into a C++ process (ACR) that already
    # has libstdc++: never carry a second copy, never leak symbols, and hide
    # every symbol coming from statically linked Conan archives so they cannot
    # interpose on the host game (zlib, brotli, freetype, RmlUi, ...).
    target_compile_options(zephyrus-acr PRIVATE -fvisibility=hidden)
    target_link_options(zephyrus-acr PRIVATE
        -Wl,--exclude-libs,ALL
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/zephyrus-acr.map")
endif()

file(GLOB_RECURSE TEST_SRCS test/*.cpp)
foreach(test_case ${TEST_SRCS})
    get_filename_component(test_name ${test_case} NAME_WE)
    get_filename_component(test_path ${test_case} PATH)
    string(REPLACE "_" "-" test_name "${test_name}")
    message(STATUS "[Zephyrus ACR] Adding test ${test_name} (${test_case})")
    add_executable(${test_name} ${test_case})
    target_link_libraries(${test_name} PRIVATE Zephyrus::Core Boost::ut)
    add_test(${test_name} ${PROJECT_BINARY_DIR}/${test_name})
endforeach()
target_link_libraries(test-cmrc-file-interface PRIVATE Zephyrus::Asset::Test)
