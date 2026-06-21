# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

message(STATUS "[Zephyrus ACR] Downloading CPM...")
include("${PROJECT_SOURCE_DIR}/cmake/get-cpm.cmake")

function(setup_dependencies)
    set(CCAHCE_CMAKE_VERSION "1.2.5")
    set(CCACHE_CMAKE_HASH "a4114a1916e6204c45fe565ec219bcbe26eccd28")
    set(CMRC_VERSION "2.0.1")
    set(CMRC_HASH "952ffddba731fc110bd50409e8d2b8a06abbd237")
    set(SPDLOG_VERSION "1.17.0")
    set(SPDLOG_HASH "f544fce006850a58063277068948dcda5681a27c")
    set(FMT_VERSION "12.2.0")
    set(FMT_HASH "588b3a0f8f6a8bcf2a959cae882d5b2703e86737")
    set(UT_VERSION "2.3.1")
    set(UT_HASH "59a9beba0763dbb45b3cc68e4cf484c659319a97")

    CPMAddPackage(
        NAME ccache-cmake
        GITHUB_REPOSITORY TheLartians/Ccache.cmake
        VERSION "${CCAHCE_CMAKE_VERSION}"
        GIT_TAG "${CCACHE_CMAKE_HASH}"
    )

    CPMAddPackage(
        NAME cmrc
        GITHUB_REPOSITORY vector-of-bool/cmrc
        VERSION "${CMRC_VERSION}"
        GIT_TAG "${CMRC_HASH}"
    )

    CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        VERSION "${SPDLOG_VERSION}"
        GIT_TAG "${SPDLOG_HASH}"
        OPTIONS "SPDLOG_BUILD_SHARED OFF"
    )

    CPMAddPackage(
        NAME fmt
        GITHUB_REPOSITORY fmtlib/fmt
        VERSION "${FMT_VERSION}"
        GIT_TAG "${FMT_HASH}"
    )

    CPMAddPackage(
        NAME ut
        GITHUB_REPOSITORY boost-ext/ut
        VERSION "${UT_VERSION}"
        GIT_TAG "${UT_HASH}"
    )
endfunction()
