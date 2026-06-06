/*
 * leap_conformance_metrics.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_METRICS_H
#define LEAP_CONFORMANCE_METRICS_H

#include <stdint.h>

#include "leap/leap_controller_sequence.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_protocol.h"
#include "leap/leap_raw_winpcap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapConformanceLatencyTrend
{
    uint32_t samples[LEAP_PD_LATENCY_HISTORY_MAX];
    uint32_t count;
    uint32_t base_exchange;
} LeapConformanceLatencyTrend;

typedef struct LeapConformanceMetrics
{
    LeapRawWinpcapStats              transport;
    LeapRawWinpcapLinkState          link;
    LeapPdControllerStats            pd;
    LeapControllerFrameSequenceState frame_seq;
    LeapTimingReply                  timing;
    uint16_t                         diag_counter_count;
    uint16_t                         diag_counters[16];
    uint32_t                         stack_phase;
    uint32_t                         stack_flags;
    uint16_t                         device_state;
    uint8_t                          session_owner_mac[6];
    int                              has_session_owner;
    uint32_t                         lease_remaining_us;
    uint32_t                         watchdog_remaining_us;
    int                              has_lease_watchdog;
    uint64_t                         rx_frames;
    uint64_t                         tx_frames;
    uint64_t                         duplicate_frames;
    uint64_t                         stale_frames;
    int                              frames_from_device;
    uint32_t                         last_cycle_time_us;
    uint32_t                         worst_cycle_time_us;
    int                              has_cycle_timing;
    int                              has_device_diag;
    LeapConformanceLatencyTrend      reply_latency_trend;
    LeapConformanceLatencyTrend      network_rtt_trend;
} LeapConformanceMetrics;

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_METRICS_H */
