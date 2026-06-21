// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#if defined(__linux__)
#define ZEPHYRUS_OS_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
#define ZEPHYRUS_OS_WINDOWS 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define ZEPHYRUS_OS_MACOS 1
#endif
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define ZEPHYRUS_ARCH_X86_64 1
#elif defined(__i386__) || defined(_M_IX86)
#define ZEPHYRUS_ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ZEPHYRUS_ARCH_ARM64 1
#elif defined(__arm__)
#define ZEPHYRUS_ARCH_ARM 1
#endif

#if defined(__clang__)
#define ZEPHYRUS_CC_CLANG 1
#elif defined(__GNUC__)
#define ZEPHYRUS_CC_GCC 1
#elif defined(__MSC_VER)
#define ZEPHYRUS_CC_MSVC 1
#endif
