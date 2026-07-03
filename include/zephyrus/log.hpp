// SPDX-License-Identifier: MIT
//
// SPDX-FileCopyrightText: Copyright(c) 2026 Otonashi Horobu

#pragma once

#if !defined(NDEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#define SPDLOG_NO_SOURCE_LOC
#endif
#include <spdlog/spdlog.h>

#define ZEPHYRUS_LOGGER_NAME "zephyrus"

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
#define ZEPHYRUS_TRACE(...)                                                    \
    if (auto _lg{::spdlog::default_logger()}; _lg) {                           \
        SPDLOG_LOGGER_TRACE(_lg, __VA_ARGS__);                                 \
    }
#else
#define ZEPHYRUS_TRACE(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
#define ZEPHYRUS_DEBUG(...)                                                    \
    if (auto _lg{::spdlog::default_logger()}; _lg) {                           \
        SPDLOG_LOGGER_DEBUG(_lg, __VA_ARGS__);                                 \
    }
#else
#define ZEPHYRUS_DEBUG(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_INFO
#define ZEPHYRUS_INFO(...)                                                     \
    if (auto _lg{::spdlog::default_logger()}; _lg) {                           \
        SPDLOG_LOGGER_INFO(_lg, __VA_ARGS__);                                  \
    }
#else
#define ZEPHYRUS_INFO(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_WARN
#define ZEPHYRUS_WARN(...)                                                     \
    if (auto _lg{::spdlog::default_logger()}; _lg) {                           \
        SPDLOG_LOGGER_WARN(_lg, __VA_ARGS__);                                  \
    }
#else
#define ZEPHYRUS_WARN(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_ERROR
#define ZEPHYRUS_ERROR(...)                                                    \
    if (auto _lg{::spdlog::default_logger()}; _lg) {                           \
        SPDLOG_LOGGER_ERROR(_lg, __VA_ARGS__);                                 \
    }
#else
#define ZEPHYRUS_ERROR(...) (void)0
#endif

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define ZEPHYRUS_CRITICAL(...)                                                 \
    if (auto _lg{::spdlog::default_logger()}; _lg) {                           \
        SPDLOG_LOGGER_CRITICAL(_lg, __VA_ARGS__);                              \
    }
#else
#define ZEPHYRUS_CRITICAL(...) (void)0
#endif
