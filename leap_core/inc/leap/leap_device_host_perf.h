/*
 * leap_device_host_perf.h
 *
 * Shared embedded device-host performance checklist IDs and helpers.
 * See docs/LEAP_DEVICE_PERFORMANCE.md for per-platform status.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DEVICE_HOST_PERF_H
#define LEAP_DEVICE_HOST_PERF_H

#include "leap/leap_protocol.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_DEVICE_PERF_CHECKLIST_VERSION 1u

/*
 * Checklist item IDs (M0–M3). Platforms mark each item done/pending/N/A in
 * docs/LEAP_DEVICE_PERFORMANCE.md.
 */
typedef enum LeapDevicePerfItem
{
    LEAP_DEVICE_PERF_M0_DEVICE_REPLY_METRICS = 0,
    LEAP_DEVICE_PERF_M1_NO_MAIN_LOOP_SLEEP,
    LEAP_DEVICE_PERF_M1_GPIO_MODE_ONCE,
    LEAP_DEVICE_PERF_M1_OUTPUTS_DIRTY,
    LEAP_DEVICE_PERF_M1_TX_PBUF_POOL,
    LEAP_DEVICE_PERF_M1_NO_HOT_PATH_TRACE,
    LEAP_DEVICE_PERF_M1_FAST_SERVICE_PEEK,
    LEAP_DEVICE_PERF_M1_PD_INPUT_REFRESH_ONLY,
    LEAP_DEVICE_PERF_M2_PD_FAST_PATH,
    LEAP_DEVICE_PERF_M2_SAMPLED_DIAG,
    LEAP_DEVICE_PERF_M3_SUB_MS_WIRE_RTT,
    LEAP_DEVICE_PERF_ITEM_COUNT
} LeapDevicePerfItem;

/* Target gates (microseconds) for soak validation. */
#define LEAP_DEVICE_PERF_TARGET_DEVICE_REPLY_P99_US 350u
#define LEAP_DEVICE_PERF_TARGET_WIRE_RTT_AVG_US       800u

/*
 * M2b: update LAST_REPLY_LATENCY_US / cycle-time counters every N PD replies.
 * Set to 1 to record every exchange (debug); 64 is typical for soak benches.
 */
#ifndef LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL
#define LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL 64u
#endif

#ifndef LEAP_DEVICE_HOST_TRACE_ENABLE
#if defined(NDEBUG) && !defined(LEAP_DEVICE_HOST_TRACE_FORCE)
#define LEAP_DEVICE_HOST_TRACE_ENABLE 0
#else
#define LEAP_DEVICE_HOST_TRACE_ENABLE 1
#endif
#endif

#define LEAP_DEVICE_HEADER_SERVICE_ID_OFFSET 8u
#define LEAP_DEVICE_HEADER_MIN_PEEK_LENGTH   10u

/*
 * Fast header peek: service_id only, no CRC validation.
 * Use before full leap_frame_parse on hot RX paths (e.g. PD input refresh gate).
 * Returns 0 on success, -1 on invalid/short buffer.
 */
static inline int leap_device_frame_peek_service_id(
    const uint8_t* data,
    size_t         length,
    uint16_t*      service_id_out)
{
    if (data == NULL || service_id_out == NULL ||
        length < LEAP_DEVICE_HEADER_MIN_PEEK_LENGTH)
    {
        return -1;
    }

    if (data[0] != LEAP_MAGIC_B0 || data[1] != LEAP_MAGIC_B1 ||
        data[2] != LEAP_MAGIC_B2 || data[3] != LEAP_MAGIC_B3)
    {
        return -1;
    }

    if (data[6] != (uint8_t)sizeof(LeapHeader))
    {
        return -1;
    }

    *service_id_out =
        (uint16_t)((uint16_t)data[LEAP_DEVICE_HEADER_SERVICE_ID_OFFSET] |
                   ((uint16_t)data[LEAP_DEVICE_HEADER_SERVICE_ID_OFFSET + 1u] << 8));
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DEVICE_HOST_PERF_H */
