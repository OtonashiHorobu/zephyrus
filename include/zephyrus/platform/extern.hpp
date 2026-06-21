// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#include <zephyrus/platform/detection.hpp>

#if ZEPHYRUS_CC_MSVC
#define ZEPHYRUS_EXPORT __declspec(dllexport)
#define ZEPHYRUS_IMPORT __declspec(dllimport)
#else
#define ZEPHYRUS_EXPORT __attribute__((visibility("default")))
#define ZEPHYRUS_IMPORT
#endif

#if defined(ZEPHYRUS_LIB)
#define ZEPHYRUS_EXTERN ZEPHYRUS_EXPORT
#else
#define ZEPHYRUS_EXTERN ZEPHYRUS_IMPORT
#endif
