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

#define LEAP_PD_LATENCY_HISTORY_MAX 1000u
#define LEAP_PD_NETWORK_RTT_HIST_BUCKETS 7u

typedef struct LeapPdLatencyHistory
{
    uint32_t samples[LEAP_PD_LATENCY_HISTORY_MAX];
    uint32_t write_index;
    uint32_t total_samples;
} LeapPdLatencyHistory;

typedef struct LeapPdControllerStats
{
    uint64_t cycles_completed;
    uint64_t pd_sent_ok;
    uint64_t pd_sent_fail;
    uint64_t heartbeats_sent;
    uint64_t exchange_replies;
    uint64_t recv_timeouts;
    uint64_t lost_frames;
    uint64_t reply_rejects;
    uint64_t reply_sequence_mismatches;
    uint64_t reply_stale_rejects;
    uint64_t last_latency_us;
    uint64_t max_latency_us;
    uint64_t total_latency_us;
    uint64_t last_network_rtt_us;
    uint64_t max_network_rtt_us;
    uint64_t total_network_rtt_us;
    uint64_t network_rtt_samples;
    uint64_t network_rtt_hist[LEAP_PD_NETWORK_RTT_HIST_BUCKETS];
    uint64_t last_queue_wait_us;
    uint64_t max_queue_wait_us;
    uint64_t total_queue_wait_us;
    uint64_t last_cycle_period_us;
    uint64_t min_cycle_period_us;
    uint64_t max_cycle_period_us;
    uint64_t total_cycle_period_us;
    uint64_t last_cycle_jitter_us;
    uint64_t max_cycle_jitter_us;
    uint64_t total_cycle_jitter_us;
    uint64_t last_cycle_work_us;
    uint64_t max_cycle_work_us;
    uint64_t cycle_overruns;
    uint16_t last_digital_inputs;
} LeapPdControllerStats;

typedef struct LeapPdControllerConfig
{
    unsigned         cycle_period_ms; /* 0 = no inter-cycle delay (freerun) */
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
    /*
     * Stale-frame guard on inbound EXCHANGE_REPLY (§13.4 echoed timestamp).
     * max_frame_age_us == 0 derives 2 * cycle_period_ms from cycle config.
     */
    int              enforce_reply_frame_age;
    uint32_t         max_frame_age_us;
    uint32_t         reply_jitter_margin_us;
    /*
     * When non-zero, stats logs split apparent latency (send to finish) from
     * network RTT (send to wire reply) and queue wait (reply to finish start).
     */
    int              hub_parallel_finish;
    unsigned         hub_finish_slot;
    /*
     * When non-zero, each cycle drives a single random output bit (0..15)
     * instead of walking bits 0..5 via cycle_index.
     */
    int              random_output;
    /*
     * When non-zero, cyclic PD uses fixed_digital_outputs instead of the
     * rotating demo pattern in leap_pd_ctrl_pick_outputs().
     */
    int              use_fixed_outputs;
    uint16_t         fixed_digital_outputs;
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
    LeapPdLatencyHistory   latency_history;
    LeapPdLatencyHistory   network_rtt_history;
    uint32_t               pd_sequence;
    uint32_t               cycle_index;
    uint64_t               last_cycle_start_us;
    int                    cycle_timing_active;
    int                    cycle_send_pending;
    uint32_t               pending_process_sequence;
    uint64_t               pending_cycle_start_us;
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
        int            timeout_ms,
        uint64_t*      reply_recv_us_out);

    /*
     * Optional: pull buffered inbound PD replies into a mailbox before finish
     * slots run (parallel hub). NULL on transports without buffering.
     */
    void (*drain_pending_replies)(void* user_ctx);

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

LeapPdControllerStatus leap_pd_controller_run_one_cycle_send(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext*   mgmt,
    const LeapPdControllerIo*    io,
    const uint8_t*               peer_mac,
    volatile int*                stop_flag);

LeapPdControllerStatus leap_pd_controller_run_one_cycle_finish(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext*   mgmt,
    const LeapPdControllerIo*    io,
    const uint8_t*               peer_mac,
    volatile int*                stop_flag,
    int                          sleep_for_period);

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

void leap_pd_controller_log_stats(
    const LeapPdControllerContext* ctx,
    const uint8_t*                 peer_mac);

void leap_pd_controller_sleep_us(uint64_t sleep_us);

uint32_t leap_pd_controller_rand_u32(void);

void leap_pd_controller_seed_rand(uint32_t seed);

void leap_pd_latency_history_push(
    LeapPdLatencyHistory* history,
    uint32_t              sample_us);

int leap_pd_latency_history_sample_at(
    const LeapPdLatencyHistory* history,
    uint32_t                    absolute_index,
    uint32_t*                   sample_out);

void leap_pd_latency_history_export(
    const LeapPdLatencyHistory* history,
    uint32_t*                   out_samples,
    uint32_t                    out_cap,
    uint32_t*                   out_count,
    uint32_t*                   out_base_exchange);

uint32_t leap_pd_stats_network_rtt_percentile_us(
    const LeapPdControllerStats* stats,
    unsigned                     percentile);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_PD_CONTROLLER_H */
