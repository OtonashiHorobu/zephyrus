# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

message(STATUS "[Zephyrus ACR] Downloading CPM...")
include("${PROJECT_SOURCE_DIR}/cmake/get-cpm.cmake")

function(setup_dependencies)
    CPMAddPackage("gh:TheLartians/Ccache.cmake@1.2.5#639f153a960f04407618a68d4dc68c9e6c3cd10d")
    CPMAddPackage("gh:vector-of-bool/cmrc@2.0.1#952ffddba731fc110bd50409e8d2b8a06abbd237")
    CPMAddPackage("gh:gabime/spdlog@1.17.0#79524ddd08a4ec981b7fea76afd08ee05f83755d")
    CPMAddPackage("gh:fmtlib/fmt@12.2.0#1be298e1bd68957e4cd352e1f676f00e07dcfb57")
    CPMAddPackage("gh:boost-ext/ut@2.3.1#f923e6fe4b7542d75e0c4ee54ad0af6a5382a87c")

    CPMAddPackage(
        URI "gh:madler/zlib@1.3.2#da607da739fa6047df13e66a2af6b8bec7c2a498"
        OPTIONS
            "ZLIB_BUILD_TESTING OFF"
            "ZLIB_BUILD_SHARED OFF"
    )
    set(ZLIB_ROOT "${PROJECT_BINARY_DIR}/_deps/zlib-src")
    if(zlib_ADDED)
        if(NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        endif()
    endif()

    CPMAddPackage(
        URI "gh:pnggroup/libpng@1.8.0#d1d0abeffede1cc898ddc3d0e600839cf026d749"
        OPTIONS
            "PNG_TESTS OFF"
            "PNG_TOOLS OFF"
            "PNG_SHARED OFF"
    )
    set(PNG_ROOT "${PROJECT_BINARY_DIR}/_deps/libpng-src")
    if(png_ADDED)
        if(NOT TARGET PNG::PNG)
            add_library(PNG::PNG ALIAS png_static)
        endif()
    endif()

    CPMAddPackage("https://gitlab.com/bzip2/bzip2.git@1.0.8#6a8690fc8d26c815e798c588f796eabe9d684cf0")
    set(BZip2_ROOT "${PROJECT_BINARY_DIR}/_deps/bzip2-src")

    CPMAddPackage("gh:harfbuzz/harfbuzz@14.2.1#56feae4035bdd48f62ba2b8d8c16232d4d89b3a4")
    set(HarfBuzz_ROOT "${PROJECT_BINARY_DIR}/_deps/harfbuzz-src")
    if(harfbuzz_ADDED)
        if(NOT TARGET HarfBuzz::HarfBuzz)
            add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz)
        endif()
    endif()

    CPMAddPackage(
        URI "gh:freetype/freetype@2.14.3#0a0221a1347e2f1e07c395263540026e9a0aa7c7"
        OPTIONS "FT_DYNAMIC_HARFBUZZ FALSE"
    )
    if(freetype_ADDED)
        if(NOT TARGET Freetype::Freetype)
            add_library(Freetype::Freetype ALIAS freetype)
        endif()
    endif()
    CPMAddPackage(
        URI "gh:mikke89/RmlUi@6.2#2230d1a6e8e0848ed87a5761e2a5160b2a175ba4"
    )
endfunction()
