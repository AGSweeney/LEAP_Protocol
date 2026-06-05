/*
 * leap_conformance.h
 *
 * Shared conformance runner for CLI and Qt Studio.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_H
#define LEAP_CONFORMANCE_H

#include <stddef.h>
#include <stdint.h>

#include "leap/conformance/leap_conformance_metrics.h"
#include "leap/conformance/leap_conformance_result.h"
#include "leap/conformance/leap_conformance_scenario.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapConformanceProgressPhase
{
    LEAP_CONF_PROGRESS_START = 0,
    LEAP_CONF_PROGRESS_STEP,
    LEAP_CONF_PROGRESS_METRICS,
    LEAP_CONF_PROGRESS_DONE
} LeapConformanceProgressPhase;

typedef struct LeapConformanceProgress
{
    LeapConformanceProgressPhase phase;
    const char*                  step_id;
    const char*                  step_name;
    unsigned                     percent;
    uint32_t                     elapsed_ms;
    const LeapConformanceMetrics* metrics;
} LeapConformanceProgress;

typedef void (*LeapConformanceProgressFn)(void* ctx, const LeapConformanceProgress* progress);

typedef struct LeapConformanceIo
{
    void* user_ctx;

    int (*open_transport)(void* user_ctx, const char* adapter, const char* capture_pcap);
    void (*close_transport)(void* user_ctx);

    int (*discover_peers)(void* user_ctx, int scan_ms, unsigned* peer_count_out);
    int (*find_peer_mac)(void* user_ctx, const uint8_t* expected_mac, int* found_out);

    int (*bootstrap)(void* user_ctx, uint16_t outputs, int* op_out);
    int (*pd_write)(void* user_ctx, uint16_t outputs, int* sent_out);
    int (*read_diag)(void* user_ctx, int* ok_out);
    int (*lease_demo)(void* user_ctx, int* ok_out);
    int (*cyclic_pd)(void* user_ctx, uint16_t outputs, int exchange,
                     unsigned seconds, unsigned cyclic_ms,
                     LeapPdControllerStats* stats_out, int* ok_out);
    int (*identify)(void* user_ctx, const uint8_t* peer_mac, int* ok_out);
    int (*locate)(void* user_ctx, const uint8_t* peer_mac,
                  unsigned duration_ms, int* ok_out);

    int (*snapshot)(void* user_ctx, LeapConformanceMetrics* out);
    void (*cancel)(void* user_ctx);
} LeapConformanceIo;

typedef struct LeapConformanceRunConfig
{
    const char*               scenario_id;
    const char* const*        step_filter;
    size_t                    step_filter_count;
    const char*               adapter;
    const char*               peer_mac_text;
    uint8_t                   peer_mac[6];
    int                       has_peer_mac;
    unsigned                  cyclic_seconds;
    unsigned                  bootstrap_retries;
    unsigned                  retry_delay_ms;
    const char*               capture_pcap_path;
    int                       keep_session_open;
    LeapConformanceProgressFn progress_fn;
    void*                     progress_ctx;
    const LeapConformanceIo*  io;
} LeapConformanceRunConfig;

typedef enum LeapConformanceStatus
{
    LEAP_CONF_OK = 0,
    LEAP_CONF_INVALID_ARG,
    LEAP_CONF_SCENARIO_UNKNOWN,
    LEAP_CONF_TRANSPORT_ERROR,
    LEAP_CONF_CANCELLED,
    LEAP_CONF_STEP_FAILED
} LeapConformanceStatus;

LeapConformanceStatus leap_conformance_run(
    const LeapConformanceRunConfig* config,
    LeapConformanceRunResult*         result_out);

LeapConformanceStatus leap_conformance_run_steps(
    const LeapConformanceRunConfig* config,
    LeapConformanceRunResult*         result_out);

LeapConformanceStatus leap_conformance_snapshot(
    const LeapConformanceIo* io,
    LeapConformanceMetrics*  out);

LeapConformanceStatus leap_conformance_cyclic_monitor(
    const LeapConformanceIo* io,
    unsigned                 interval_ms,
    volatile int*            stop_flag,
    LeapConformanceProgressFn progress_fn,
    void*                    progress_ctx);

void leap_conformance_cancel(const LeapConformanceIo* io);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_H */
