# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

cmake_minimum_required(VERSION 4.2)
project(ZephyrusACR LANGUAGES CXX)

set(BUILD_SHARED_LIBS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
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
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "i386" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i686")
        add_compile_definitions(_X86_)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
        add_compile_definitions(_AMD64_)
    # arm64 support may be added later
    else()
        message(FATAL_ERROR "[Zephyrus ACR] Unknown system processor")
    endif()
endif()

enable_testing()

set(RELEASE_VERSION "0.1.0-alpha")
set(BRANCH_NAME "")
set(COMMIT_HASH "")

find_package(Git REQUIRED)

if (GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} symbolic-ref --short -q HEAD
        OUTPUT_VARIABLE BRANCH_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%h
        OUTPUT_VARIABLE COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    )
endif ()

message(STATUS "[Zephyrus ACR] Zephyrus ACR version: ${RELEASE_VERSION} ${BRANCH_NAME}:${COMMIT_HASH}")
configure_file(version.cpp.in version.cpp @ONLY)

include("${PROJECT_SOURCE_DIR}/cmake/deps.cmake")
setup_dependencies()

include_directories(
    "${PROJECT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
)

add_library(zephyrus-acr-core STATIC
    "${PROJECT_BINARY_DIR}/version.cpp"
    "${PROJECT_SOURCE_DIR}/include/zephyrus/platform/detection.hpp"
    "${PROJECT_SOURCE_DIR}/include/zephyrus/platform/extern.hpp"
    "${PROJECT_SOURCE_DIR}/include/zephyrus/log.hpp"
    "${PROJECT_SOURCE_DIR}/include/zephyrus/version.hpp"
)
target_link_libraries(zephyrus-acr-core PUBLIC fmt spdlog)
target_compile_definitions(zephyrus-acr-core PRIVATE ZEPHYRUS_LIB)

add_executable(zephyrus-acr-standalone
    "${PROJECT_SOURCE_DIR}/src/zephyrus/zephyrus_standalone.cpp"
)
target_link_libraries(zephyrus-acr-standalone zephyrus-acr-core)

add_library(zephyrus-acr SHARED
    "${PROJECT_SOURCE_DIR}/src/zephyrus/zephyrus.cpp"
)
target_link_libraries(zephyrus-acr zephyrus-acr-core)

file(GLOB_RECURSE TEST_SRCS "${PROJECT_SOURCE_DIR}/test/*.cpp")
foreach(test_case ${TEST_SRCS})
    get_filename_component(test_name ${test_case} NAME_WE)
    get_filename_component(test_path ${test_case} PATH)
    string(REPLACE "_" "-" test_name "${test_name}")
    message(STATUS "[Zephyrus ACR] Adding test ${test_name} (${test_case})")
    add_executable(${test_name} ${test_case})
    target_link_libraries(${test_name} zephyrus-acr-core ut)
    add_test(${test_name} ${PROJECT_BINARY_DIR}/${test_name})
endforeach()
