# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2026 Otonashi Horobu

cmake_minimum_required(VERSION 4.2)
project(ZephyrusACR LANGUAGES C CXX)

set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING ON)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_SYSTEM_PREFIX_PATH "")
set(CMAKE_SYSTEM_LIBRARY_PATH "")
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

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
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_link_options(-static-libstdc++ -static-libgcc)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-fvisibility=hidden)
    endif()
endif()

add_compile_options(-Wall -Wextra)

if (BUILD_TESTING)
    enable_testing()
endif()

set(RELEASE_VERSION "0.1.0-alpha")
set(BRANCH_NAME "")
set(COMMIT_HASH "")

find_package(Git)

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

include(cmake/dependencies.cmake)
setup_dependencies()

add_subdirectory(asset)
add_subdirectory(test)

include_directories(
    "${PROJECT_BINARY_DIR}"
    include
)

add_library(zephyrus-core STATIC
    "${PROJECT_BINARY_DIR}/version.cpp"
    src/zephyrus/rmlui/cmrc_file_interface.cpp
    src/zephyrus/log.cpp
    src/zephyrus/version.cpp
    src/zephyrus/zephyrus.cpp
)
add_library(Zephyrus::Core ALIAS zephyrus-core)
target_link_libraries(zephyrus-core PUBLIC
    Zephyrus::Asset
    fmt::fmt
    spdlog::spdlog
    RmlUi::Core
)
target_compile_options(zephyrus-core PRIVATE -Werror)

add_executable(zephyrus-standalone
    src/zephyrus/zephyrus_standalone.cpp
)
target_link_libraries(zephyrus-standalone PRIVATE Zephyrus::Core)
target_compile_options(zephyrus-standalone PRIVATE -Werror)

add_library(zephyrus-acr SHARED
    src/zephyrus/zephyrus_acr.cpp
)
add_library(Zephyrus::ACR ALIAS zephyrus-acr)
target_link_libraries(zephyrus-acr PRIVATE Zephyrus::Core)
target_compile_options(zephyrus-acr PRIVATE -Werror)

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

target_link_libraries(cmrc-file-interface-test PRIVATE Zephyrus::Asset::Test)
target_link_libraries(cmrc-file-interface-thread-test PRIVATE Zephyrus::Asset::Test)
