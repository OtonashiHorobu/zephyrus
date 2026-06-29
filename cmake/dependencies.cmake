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
        OVERRIDE_FIND_PACKAGE
        OPTIONS
            "ZLIB_BUILD_TESTING OFF"
            "ZLIB_BUILD_SHARED OFF"
    )
    add_library(ZLIB::ZLIB ALIAS zlibstatic)

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
    set(PNG_LIBRARY "${PROJECT_BINARY_DIR}/_deps/libpng-src")
    set(PNG_PNG_INCLUDE_DIR "${PROJECT_BINARY_DIR}/_deps/libpng-src")
    add_library(PNG::PNG ALIAS png_static)

    CPMAddPackage(
        URI "gh:harfbuzz/harfbuzz@14.2.1#56feae4035bdd48f62ba2b8d8c16232d4d89b3a4"
        OVERRIDE_FIND_PACKAGE
        OPTIONS
            "HB_HAVE_FREETYPE OFF"
            "SKIP_INSTALL_ALL ON"
    )
    add_library(HarfBuzz::HarfBuzz ALIAS harfbuzz)

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
        URI "gh:mikke89/RmlUi@6.2#2230d1a6e8e0848ed87a5761e2a5160b2a175ba4"
    )
endfunction()
