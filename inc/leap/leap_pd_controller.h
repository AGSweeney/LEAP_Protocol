/*
 * leap_pd_controller.h
 *
 * Controller-side cyclic PD exchange with lease maintenance and statistics.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_PD_CONTROLLER_H
#define LEAP_PD_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_mgmt_controller.h"
#include "leap/leap_pd_common.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapPdControllerStats
{
    uint64_t cycles_completed;
    uint64_t pd_sent_ok;
    uint64_t pd_sent_fail;
    uint64_t heartbeats_sent;
    uint64_t exchange_replies;
    uint64_t recv_timeouts;
    uint64_t reply_rejects;
    uint64_t reply_sequence_mismatches;
    uint64_t last_latency_us;
    uint64_t max_latency_us;
    uint64_t total_latency_us;
    uint64_t last_cycle_period_us;
    uint64_t min_cycle_period_us;
    uint64_t max_cycle_period_us;
    uint64_t total_cycle_period_us;
    uint64_t last_cycle_work_us;
    uint64_t max_cycle_work_us;
    uint64_t cycle_overruns;
    uint16_t last_digital_inputs;
} LeapPdControllerStats;

typedef struct LeapPdControllerConfig
{
    unsigned         cycle_period_ms;
    unsigned         stats_log_interval;
    int              use_exchange;
    uint32_t         profile_id;
    uint32_t         heartbeat_every_n_cycles;
    LeapPdProfileMap profile;
    /*
     * When non-zero (default), validate EXCHANGE_REPLY profile, endpoints, and
     * process_sequence before accepting inputs (multi-peer safety).
     */
    int              validate_exchange_reply;
} LeapPdControllerConfig;

typedef enum LeapPdControllerStatus
{
    LEAP_PD_CTRL_OK = 0,
    LEAP_PD_CTRL_INVALID_ARG,
    LEAP_PD_CTRL_IO_MISSING,
    LEAP_PD_CTRL_BUILD_FAILED,
    LEAP_PD_CTRL_SEND_FAILED,
    LEAP_PD_CTRL_HEARTBEAT_FAILED,
    LEAP_PD_CTRL_EXCHANGE_TIMEOUT,
    LEAP_PD_CTRL_STOPPED
} LeapPdControllerStatus;

typedef struct LeapPdControllerContext
{
    LeapPdControllerConfig config;
    LeapPdControllerStats  stats;
    uint32_t               pd_sequence;
    uint32_t               cycle_index;
    uint64_t               last_cycle_start_us;
    int                    cycle_timing_active;
} LeapPdControllerContext;

typedef struct LeapPdControllerIo
{
    void* user_ctx;

    int (*send_pd)(
        void*          user_ctx,
        const uint8_t* peer_mac,
        uint16_t       message_type,
        const uint8_t* payload,
        size_t         payload_length,
        uint32_t       session_id,
        uint32_t       sequence);

    int (*send_heartbeat)(
        void*          user_ctx,
        const uint8_t* peer_mac,
        const uint8_t* payload,
        size_t         payload_length,
        uint32_t       session_id,
        uint32_t       sequence);

    int (*wait_exchange_reply)(
        void*          user_ctx,
        const uint8_t* peer_mac,
        uint8_t*       reply_payload,
        size_t         reply_capacity,
        size_t*        reply_length,
        int            timeout_ms);

    uint64_t (*monotonic_us)(void* user_ctx);
} LeapPdControllerIo;

void leap_pd_controller_init(
    LeapPdControllerContext*       ctx,
    const LeapPdControllerConfig* config);

void leap_pd_controller_reset_stats(LeapPdControllerContext* ctx);

const LeapPdControllerStats* leap_pd_controller_stats(
    const LeapPdControllerContext* ctx);

LeapPdControllerStatus leap_pd_controller_run_one_cycle(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext* mgmt,
    const LeapPdControllerIo*  io,
    const uint8_t*             peer_mac,
    volatile int*              stop_flag,
    int                        sleep_for_period);

LeapPdControllerStatus leap_pd_controller_run_cyclic(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext*   mgmt,
    const LeapPdControllerIo*    io,
    const uint8_t*               peer_mac,
    volatile int*                stop_flag);

LeapPdControllerStatus leap_pd_controller_send_single_write(
    LeapPdControllerContext*   pd,
    LeapMgmtControllerContext* mgmt,
    const LeapPdControllerIo*  io,
    const uint8_t*             peer_mac,
    uint16_t                   digital_outputs);

void leap_pd_controller_log_stats(const LeapPdControllerContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_PD_CONTROLLER_H */
