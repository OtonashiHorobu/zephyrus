# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

message(STATUS "[Zephyrus ACR] Downloading CPM...")
include("${PROJECT_SOURCE_DIR}/cmake/get-cpm.cmake")

function(setup_dependencies)
    find_package(OpenGL REQUIRED)

    CPMAddPackage(
        URI "gh:TheLartians/Ccache.cmake@1.2.5#639f153a960f04407618a68d4dc68c9e6c3cd10d"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:vector-of-bool/cmrc@2.0.1#952ffddba731fc110bd50409e8d2b8a06abbd237"
        OVERRIDE_FIND_PACKAGE
        OPTIONS "CMAKE_POLICY_VERSION_MINIMUM 4.0"
    )

    CPMAddPackage(
        URI "gh:gabime/spdlog@1.17.0#79524ddd08a4ec981b7fea76afd08ee05f83755d"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:fmtlib/fmt@12.2.0#1be298e1bd68957e4cd352e1f676f00e07dcfb57"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:boost-ext/ut@2.3.1#f923e6fe4b7542d75e0c4ee54ad0af6a5382a87c"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:boost-ext/di@1.3.2#629454327c37cb1ed86c7b1b33e955c942bbf537"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:madler/zlib@1.3.2#da607da739fa6047df13e66a2af6b8bec7c2a498"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "ZLIB_BUILD_TESTING OFF"
        "ZLIB_BUILD_SHARED OFF"
    )
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")

    CPMAddPackage(
        URI "https://gitlab.com/bzip2/bzip2.git@1.0.8#6a8690fc8d26c815e798c588f796eabe9d684cf0"
        OVERRIDE_FIND_PACKAGE
    )
    add_library(bzip2 STATIC
        "${bzip2_SOURCE_DIR}/blocksort.c"
        "${bzip2_SOURCE_DIR}/huffman.c"
        "${bzip2_SOURCE_DIR}/crctable.c"
        "${bzip2_SOURCE_DIR}/randtable.c"
        "${bzip2_SOURCE_DIR}/compress.c"
        "${bzip2_SOURCE_DIR}/decompress.c"
        "${bzip2_SOURCE_DIR}/bzlib.c"
    )
    target_include_directories(bzip2 PUBLIC "${bzip2_SOURCE_DIR}")
    if(MSVC)
        target_compile_definitions(bzip2 PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    add_library(BZip2::BZip2 ALIAS bzip2)

    CPMAddPackage(
        URI "gh:google/brotli@1.2.0#028fb5a23661f123017c060daa546b55cf4bde29"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "BROTLI_DISABLE_TESTS ON"
        "BROTLI_BUILD_TOOLS OFF"
    )
    set(BROTLIDEC_LIBRARIES brotlidec CACHE INTERNAL "Force freetype to use our brotlidec" FORCE)
    set(BROTLIDEC_INCLUDE_DIRS "${PROJECT_BINARY_DIR}/_deps/brotli-src/c/include" CACHE INTERNAL "Force freetype to use our brotlidec" FORCE)

    CPMAddPackage(
        URI "gh:pnggroup/libpng@1.8.0#d1d0abeffede1cc898ddc3d0e600839cf026d749"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "PNG_TESTS OFF"
        "PNG_TOOLS OFF"
        "PNG_SHARED OFF"
        "SKIP_INSTALL_ALL ON"
    )
    set(PNG_LIBRARY "${libpng_SOURCE_DIR}")
    set(PNG_PNG_INCLUDE_DIR "${libpng_SOURCE_DIR}")
    add_library(PNG::PNG ALIAS png_static)

    CPMAddPackage(
        URI "gh:harfbuzz/harfbuzz@14.2.1#56feae4035bdd48f62ba2b8d8c16232d4d89b3a4"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "HB_HAVE_FREETYPE OFF"
        "SKIP_INSTALL_ALL ON"
    )
    add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz)
    add_library(harfbuzz::harfbuzz ALIAS harfbuzz)

    CPMAddPackage(
        URI "gh:freetype/freetype@2.14.3#0a0221a1347e2f1e07c395263540026e9a0aa7c7"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "FT_DYNAMIC_HARFBUZZ FALSE"
        "FT_REQUIRE_ZLIB ON"
        "FT_REQUIRE_PNG ON"
        "FT_REQUIRE_BZIP2 ON"
        "FT_REQUIRE_HARFBUZZ ON"
        "FT_REQUIRE_BROTLI ON"
        "SKIP_INSTALL_ALL ON"
    )
    add_library(Freetype::Freetype ALIAS freetype)

    CPMAddPackage(
        URI "gh:Samsung/rlottie@0.2#bf689b72b8482c5ea674235854bd11b6d1b42588"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "CMAKE_POLICY_VERSION_MINIMUM 4.0"
        "LOTTIE_MODULE OFF"
    )

    CPMAddPackage(
        URI "gh:LuaJIT/LuaJIT@2.1#a2bde60819d83e6f75130ac2c93ee4b3c7615800"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:ThePhD/sol2@3.5.0#c1f95a773c6f8f4fde8ca3efe872e7286afe4444"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "SOL2_LUA_VERSION LuaJIT"
        "SOL2_BUILD_LUA OFF"
    )

    CPMAddPackage(
        URI "gh:sammycage/lunasvg@1.3.3#2872affa1027cad92a05408a7e6f2547efa7f364"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:libsdl-org/sdl12-compat@1.2.76#8b3be24e25aecea84bb9ccc027fa71118a92fb6f"
        OVERRIDE_FIND_PACKAGE
    )

    CPMAddPackage(
        URI "gh:mikke89/RmlUi@6.2#2230d1a6e8e0848ed87a5761e2a5160b2a175ba4"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
        "RMLUI_LOTTIE_PLUGIN ON"
        "RMLUI_SVG_PLUGIN ON"
        "RMLUI_LUA_BINDINGS ON"
        "RMLUI_LUA_BINDINGS_LIBRARY luajit"
        # "RMLUI_SAMPLES ON"
        # "RMLUI_HARFBUZZ_SAMPLE ON"
    )
endfunction()
