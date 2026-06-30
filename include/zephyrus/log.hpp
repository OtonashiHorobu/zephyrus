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
#define ZEPHYRUS_LOGGER ::spdlog::get(ZEPHYRUS_LOGGER_NAME)
#define ZEPHYRUS_TRACE(...)                                                    \
    if (auto _lg{::spdlog::get(ZEPHYRUS_LOGGER_NAME)}; _lg) {                  \
        SPDLOG_LOGGER_TRACE(_lg, __VA_ARGS__);                                 \
    }
#define ZEPHYRUS_DEBUG(...)                                                    \
    if (auto _lg{::spdlog::get(ZEPHYRUS_LOGGER_NAME)}; _lg) {                  \
        SPDLOG_LOGGER_DEBUG(_lg, __VA_ARGS__);                                 \
    }
#define ZEPHYRUS_INFO(...)                                                     \
    if (auto _lg{::spdlog::get(ZEPHYRUS_LOGGER_NAME)}; _lg) {                  \
        SPDLOG_LOGGER_INFO(_lg, __VA_ARGS__);                                  \
    }
#define ZEPHYRUS_WARN(...)                                                     \
    if (auto _lg{::spdlog::get(ZEPHYRUS_LOGGER_NAME)}; _lg) {                  \
        SPDLOG_LOGGER_WARN(_lg, __VA_ARGS__);                                  \
    }
#define ZEPHYRUS_ERROR(...)                                                    \
    if (auto _lg{::spdlog::get(ZEPHYRUS_LOGGER_NAME)}; _lg) {                  \
        SPDLOG_LOGGER_ERROR(_lg, __VA_ARGS__);                                 \
    }
#define ZEPHYRUS_CRITICAL(...)                                                 \
    if (auto _lg{::spdlog::get(ZEPHYRUS_LOGGER_NAME)}; _lg) {                  \
        SPDLOG_LOGGER_CRITICAL(_lg, __VA_ARGS__);                              \
    }
#define ZEPHYRUS_LOGGER_TRACE SPDLOG_LOGGER_TRACE
#define ZEPHYRUS_LOGGER_DEBUG SPDLOG_LOGGER_DEBUG
#define ZEPHYRUS_LOGGER_INFO SPDLOG_LOGGER_INFO
#define ZEPHYRUS_LOGGER_WARN SPDLOG_LOGGER_WARN
#define ZEPHYRUS_LOGGER_ERROR SPDLOG_LOGGER_ERROR
#define ZEPHYRUS_LOGGER_CRITICAL SPDLOG_LOGGER_CRITICAL

namespace zephyrus::log {
void init_logger();
} // namespace zephyrus::log
