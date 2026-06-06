/*
 * LP-AM243 UART logging — warnings and errors only by default.
 *
 * Set LEAP_DEVICE_HOST_TRACE_FORCE when building to enable LEAP_AM243_LOG_INFO.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_AM243_LOG_H
#define LEAP_AM243_LOG_H

#include <kernel/dpl/DebugP.h>

#include "board_config.h"
#include "leap/leap_device_host_perf.h"

#define LEAP_AM243_LOG_ERROR(...) DebugP_logError(__VA_ARGS__)
#define LEAP_AM243_LOG_WARN(...)  DebugP_logWarn(__VA_ARGS__)

#if LEAP_DEVICE_HOST_TRACE_ENABLE
#define LEAP_AM243_LOG_INFO(...) DebugP_log(__VA_ARGS__)
#else
#define LEAP_AM243_LOG_INFO(...) ((void)0)
#endif

#endif /* LEAP_AM243_LOG_H */
