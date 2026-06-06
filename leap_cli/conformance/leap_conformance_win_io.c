/*
 * leap_conformance_win_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_conformance_win_io.h"

#include "../win_l2/leap_win_common.h"
#include "../win_smoke/leap_win_io.h"

#include "leap/conformance/leap_conformance_capabilities.h"
#include "leap/conformance/leap_conformance_raw_io.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_dir_controller.h"
#include "leap/leap_dir_controller_capabilities.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_diag_controller.h"
#include "leap/leap_disc_controller.h"
#include "leap/leap_frame.h"
#include "leap/leap_log.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_pd_common.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_protocol.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define LEAP_CONF_LEASE_DEMO_US            2000000u
#define LEAP_CONF_LEASE_DEMO_IDLE_S        3u
#define LEAP_CONF_DIAG_POLL_INTERVAL_US    2000000u
#define LEAP_CONF_DISCOVER_BROADCAST_MS    1000
#define LEAP_CONF_BOOTSTRAP_RECV_MS        1000
#define LEAP_CONF_BOOTSTRAP_RECV_MS_KNOWN  500
#define LEAP_CONF_VERIFY_IDENTIFY_RETRIES  3u
#define LEAP_CONF_VERIFY_PD_PROBE_RETRIES  3u
#define LEAP_CONF_BOOTSTRAP_LEASE_US       5000000u
#define LEAP_CONF_BOOTSTRAP_WATCHDOG_US    5000000u

struct LeapConformanceWinContext
{
    LeapRawWinpcapSocket           transport;
    LeapConformanceRawIo           raw_io;
    LeapControllerStack            stack;
    LeapControllerPeerTable        table;
    LeapConformanceIo              io;
    uint8_t                        peer_mac[6];
    int                            has_peer_mac;
    int                            transport_open;
    unsigned                       bootstrap_retries;
    unsigned                       retry_delay_ms;
    volatile int                   cancel_flag;
    char                           capture_path[LEAP_CONF_PCAP_PATH_MAX];
    LeapControllerStackDiagResult  cached_diag;
    int                            cached_diag_valid;
    uint64_t                       last_diag_poll_us;
    int                            force_diag_poll;
    char                           open_adapter_path[LEAP_CONF_PCAP_PATH_MAX];
    LeapConformanceProgressFn      progress_fn;
    void*                          progress_ctx;
#if defined(_WIN32)
    HANDLE                         metrics_poll_thread;
#endif
    LeapPdLatencyHistory           session_latency;
    uint32_t                       latency_last_merged_total;
    LeapPdLatencyHistory           session_network_rtt;
    uint32_t                       network_rtt_last_merged_total;
    uint32_t*                      soak_rtt_samples;
    uint32_t                       soak_rtt_count;
    uint32_t                       soak_rtt_capacity;
    LeapPdControllerStats          session_pd;
    LeapConformanceDeviceCaps      session_caps;
    int                            session_caps_valid;
    int                            soak_in_progress;
    int                            exchange_session_ready;
    uint64_t                       last_exchange_prepare_us;
};

static uint16_t leap_conf_win_bootstrap_outputs(const LeapConformanceWinContext* ctx)
{
    if (ctx != NULL && ctx->session_caps_valid)
    {
        return ctx->session_caps.bootstrap_outputs;
    }

    return 0x0001u;
}

static uint16_t leap_conf_win_soak_outputs(
    const LeapConformanceWinContext* ctx,
    uint16_t                           requested)
{
    (void)ctx;

    /*
     * requested!=0: fixed mask for conformance cyclic / mask-walk steps.
     * requested==0: rotating one-hot (controller use_fixed_outputs=0).
     */
    return requested;
}

static void leap_conf_win_normalize_pd_profile(LeapPdProfileMap* map)
{
    size_t nominal_pd;

    if (map == NULL || map->valid == 0)
    {
        return;
    }

    nominal_pd = leap_pd_endpoint_payload_size(
        map->profile_id,
        map->write_endpoint_id);
    if (nominal_pd == 0u)
    {
        nominal_pd = leap_pd_endpoint_payload_size(
            map->profile_id,
            map->read_endpoint_id);
    }
    if (nominal_pd > map->endpoint_payload_size)
    {
        map->endpoint_payload_size = nominal_pd;
    }
}

static void leap_conf_win_apply_session_profile(LeapConformanceWinContext* ctx)
{
    LeapPdProfileMap map;
    uint32_t         profile_id = 0u;

    if (ctx == NULL)
    {
        return;
    }

    if (ctx->session_caps_valid)
    {
        if (ctx->session_caps.dir.pd_map.profile_id != 0u)
        {
            profile_id = ctx->session_caps.dir.pd_map.profile_id;
        }
        else if (ctx->session_caps.dir.active_profile_id != 0u)
        {
            profile_id = ctx->session_caps.dir.active_profile_id;
        }
    }

    if (profile_id != 0u &&
        leap_pd_profile_map_from_profile_id(profile_id, &map) == LEAP_PD_COMMON_OK)
    {
        ctx->stack.config.pd.profile = map;
        ctx->stack.pd.config.profile = map;
        return;
    }

    if (!ctx->session_caps_valid ||
        ctx->session_caps.dir.pd_map.valid == 0)
    {
        return;
    }

    map = ctx->session_caps.dir.pd_map;
    leap_conf_win_normalize_pd_profile(&map);
    ctx->stack.config.pd.profile = map;
    ctx->stack.pd.config.profile = map;
}

static void leap_conf_win_apply_pd_outputs(
    LeapConformanceWinContext* ctx,
    uint16_t                     outputs)
{
    if (ctx == NULL)
    {
        return;
    }

    if (outputs == 0u)
    {
        /*
         * use_fixed_outputs=1 with fixed_digital_outputs=0 forces every
         * EXCHANGE to apply outputs=0 on the device. Fall back to the
         * controller's rotating output pattern instead.
         */
        ctx->stack.config.pd.use_fixed_outputs     = 0;
        ctx->stack.config.pd.fixed_digital_outputs = 0u;
        ctx->stack.pd.config.use_fixed_outputs     = 0;
        ctx->stack.pd.config.fixed_digital_outputs = 0u;
        return;
    }

    ctx->stack.config.pd.use_fixed_outputs     = 1;
    ctx->stack.config.pd.fixed_digital_outputs = outputs;
    ctx->stack.pd.config.use_fixed_outputs     = 1;
    ctx->stack.pd.config.fixed_digital_outputs = outputs;
}

static int leap_conf_win_bootstrap(
    LeapConformanceWinContext* ctx,
    uint16_t                   outputs,
    int*                       op_out);

static int leap_conf_win_send_identify_and_wait(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    LeapIdentifyReply*         reply_out,
    uint8_t*                   payload_out,
    size_t                     payload_capacity,
    size_t*                    payload_len_out);

static void leap_conf_win_discard_controller_stack(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL || !ctx->transport_open)
    {
        return;
    }

    if (ctx->stack.peer_bound != 0 &&
        leap_mgmt_controller_session_id(&ctx->stack.mgmt) != 0u)
    {
        (void)leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        (void)leap_raw_winpcap_drain_rx(&ctx->transport);
    }

    memset(&ctx->stack, 0, sizeof(ctx->stack));
    ctx->exchange_session_ready       = 0;
    ctx->last_exchange_prepare_us     = 0u;
}

static uint32_t leap_conf_win_fallback_profile_id(
    const LeapConformanceWinContext* ctx)
{
    if (ctx == NULL || !ctx->session_caps_valid)
    {
        return 0u;
    }

    if (ctx->session_caps.dir.pd_map.profile_id != 0u)
    {
        return ctx->session_caps.dir.pd_map.profile_id;
    }

    if (ctx->session_caps.dir.active_profile_id != 0u)
    {
        return ctx->session_caps.dir.active_profile_id;
    }

    return 0u;
}

static int leap_conf_win_hello_from_peer_entry(
    const LeapControllerPeerEntry* entry,
    LeapHelloReply*                hello_out)
{
    if (entry == NULL || hello_out == NULL)
    {
        return -1;
    }

    memset(hello_out, 0, sizeof(*hello_out));
    hello_out->active_profile_id  = entry->active_profile_id;
    hello_out->default_profile_id = entry->default_profile_id;
    hello_out->current_state      = entry->device_state;
    memcpy(hello_out->active_owner_mac, entry->active_owner_mac, 6);
    return 0;
}

static int leap_conf_win_hello_for_peer(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    LeapHelloReply*            hello_out)
{
    const LeapControllerPeerEntry* entry;
    int                            index;
    uint32_t                       profile_id;

    if (ctx == NULL || peer_mac == NULL || hello_out == NULL)
    {
        return -1;
    }

    index = leap_controller_peer_table_find(&ctx->table, peer_mac);
    entry = (index >= 0)
                ? leap_controller_peer_table_get(&ctx->table, (unsigned)index)
                : NULL;
    if (entry != NULL)
    {
        return leap_conf_win_hello_from_peer_entry(entry, hello_out);
    }

    if (leap_controller_peer_table_probe_peer(
            &ctx->table,
            &ctx->raw_io.stack_io,
            peer_mac,
            LEAP_CTRL_PEER_PROBE_TIMEOUT_MS) != LEAP_CTRL_PEER_OK)
    {
        return -1;
    }

    index = leap_controller_peer_table_find(&ctx->table, peer_mac);
    entry = (index >= 0)
                ? leap_controller_peer_table_get(&ctx->table, (unsigned)index)
                : NULL;
    if (entry != NULL)
    {
        return leap_conf_win_hello_from_peer_entry(entry, hello_out);
    }

    profile_id = leap_conf_win_fallback_profile_id(ctx);
    memset(hello_out, 0, sizeof(*hello_out));
    hello_out->current_state = (uint16_t)LEAP_STATE_CONFIGURED;
    if (profile_id != 0u)
    {
        hello_out->active_profile_id  = profile_id;
        hello_out->default_profile_id = profile_id;
    }

    return 0;
}

static void leap_conf_win_mark_exchange_prepared(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->last_exchange_prepare_us = leap_raw_winpcap_monotonic_us();
    ctx->exchange_session_ready   = 1;
}

static int leap_conf_win_probe_owner_session(
    LeapConformanceWinContext* ctx,
    uint16_t                     outputs)
{
    volatile int           stop_flag = 0;
    LeapPdControllerStatus status;
    uint64_t               replies_before;
    uint64_t               timeouts_before;
    int                    saved_use_exchange;
    unsigned               saved_period_ms;
    int                    saved_use_fixed;
    uint16_t               saved_fixed_outputs;
    unsigned               attempt;
    int                    ok = -1;

    if (ctx == NULL || leap_conformance_win_session_is_op(ctx) == 0)
    {
        return -1;
    }

    saved_use_exchange    = ctx->stack.pd.config.use_exchange;
    saved_period_ms       = ctx->stack.pd.config.cycle_period_ms;
    saved_use_fixed       = ctx->stack.pd.config.use_fixed_outputs;
    saved_fixed_outputs   = ctx->stack.pd.config.fixed_digital_outputs;

    ctx->stack.config.pd.use_exchange     = 1;
    ctx->stack.pd.config.use_exchange     = 1;
    ctx->stack.config.pd.cycle_period_ms  = 0u;
    ctx->stack.pd.config.cycle_period_ms  = 0u;

    leap_conf_win_apply_session_profile(ctx);
    leap_conf_win_apply_pd_outputs(ctx, outputs);

    for (attempt = 0u; attempt < LEAP_CONF_VERIFY_PD_PROBE_RETRIES; ++attempt)
    {
        replies_before  = ctx->stack.pd.stats.exchange_replies;
        timeouts_before = ctx->stack.pd.stats.recv_timeouts;
        (void)leap_raw_winpcap_drain_rx(&ctx->transport);

        status = leap_pd_controller_run_one_cycle(
            &ctx->stack.pd,
            &ctx->stack.mgmt,
            &ctx->raw_io.pd_io,
            ctx->stack.peer_mac,
            &stop_flag,
            0);
        if (status == LEAP_PD_CTRL_OK &&
            ctx->stack.pd.stats.exchange_replies > replies_before &&
            ctx->stack.pd.stats.recv_timeouts == timeouts_before)
        {
            ok = 0;
            break;
        }
    }

    if (ok != 0)
    {
        ctx->stack.config.pd.use_exchange          = saved_use_exchange;
        ctx->stack.pd.config.use_exchange          = saved_use_exchange;
        ctx->stack.config.pd.cycle_period_ms       = saved_period_ms;
        ctx->stack.pd.config.cycle_period_ms       = saved_period_ms;
        ctx->stack.config.pd.use_fixed_outputs     = saved_use_fixed;
        ctx->stack.pd.config.use_fixed_outputs     = saved_use_fixed;
        ctx->stack.config.pd.fixed_digital_outputs = saved_fixed_outputs;
        ctx->stack.pd.config.fixed_digital_outputs = saved_fixed_outputs;

        leap_log_printf(
            "I/O session: owner PD EXCHANGE probe failed "
            "(outputs=0x%04X session_id=%u "
            "local_mac=%02x:%02x:%02x:%02x:%02x:%02x)\n",
            (unsigned)outputs,
            (unsigned)leap_mgmt_controller_session_id(&ctx->stack.mgmt),
            ctx->transport.local_mac[0],
            ctx->transport.local_mac[1],
            ctx->transport.local_mac[2],
            ctx->transport.local_mac[3],
            ctx->transport.local_mac[4],
            ctx->transport.local_mac[5]);
        return -1;
    }

    /*
     * Leave the soak output mask configured — a successful EXCHANGE has
     * already driven GPIO on the device (ClearCore leaves safe on apply).
     */
    ctx->stack.config.pd.use_exchange    = saved_use_exchange;
    ctx->stack.pd.config.use_exchange    = saved_use_exchange;
    ctx->stack.config.pd.cycle_period_ms = saved_period_ms;
    ctx->stack.pd.config.cycle_period_ms = saved_period_ms;
    leap_log_printf(
        "I/O session: PD EXCHANGE probe ok outputs=0x%04X (session_id=%u)\n",
        (unsigned)outputs,
        (unsigned)leap_mgmt_controller_session_id(&ctx->stack.mgmt));
    return 0;
}

static int leap_conf_win_verify_device_session(LeapConformanceWinContext* ctx)
{
    LeapIdentifyReply    identify_reply;
    static const uint8_t zero_mac[6] = { 0u, 0u, 0u, 0u, 0u, 0u };
    unsigned             attempt;

    if (ctx == NULL || !ctx->has_peer_mac ||
        leap_conformance_win_session_is_op(ctx) == 0)
    {
        return -1;
    }

    /*
     * IDENTIFY confirms device OP state and owner MAC. A single owner PD
     * EXCHANGE confirms the controller session_id is accepted on the same
     * path as soak (DIAG alone can pass without owner PD rights).
     */
    for (attempt = 0u; attempt < LEAP_CONF_VERIFY_IDENTIFY_RETRIES; ++attempt)
    {
        (void)leap_raw_winpcap_drain_rx(&ctx->transport);

        if (leap_conf_win_send_identify_and_wait(
                ctx,
                ctx->peer_mac,
                &identify_reply,
                NULL,
                0u,
                NULL) != 0)
        {
            continue;
        }

        if (identify_reply.current_state != (uint16_t)LEAP_STATE_OP)
        {
            continue;
        }

        if (memcmp(identify_reply.active_owner_mac, zero_mac, 6) == 0 ||
            memcmp(
                identify_reply.active_owner_mac,
                ctx->transport.local_mac,
                6) != 0)
        {
            continue;
        }

        return leap_conf_win_probe_owner_session(
            ctx,
            leap_conf_win_soak_outputs(ctx, 0u));
    }

    return -1;
}

static int leap_conf_win_ensure_exchange_session(
    LeapConformanceWinContext* ctx,
    int                        force_bootstrap)
{
    uint16_t session_outputs = leap_conf_win_soak_outputs(ctx, 0u);
    int      op              = 0;

    if (ctx == NULL)
    {
        return -1;
    }

    if (session_outputs == 0u)
    {
        session_outputs = leap_conf_win_bootstrap_outputs(ctx);
    }

    (void)leap_raw_winpcap_drain_rx(&ctx->transport);

    if (force_bootstrap == 0 &&
        leap_conformance_win_session_is_op(ctx) != 0 &&
        leap_conf_win_verify_device_session(ctx) == 0)
    {
        leap_log_printf(
            "I/O session: verified owner session (session_id=%u)\n",
            (unsigned)leap_mgmt_controller_session_id(&ctx->stack.mgmt));
        leap_conf_win_mark_exchange_prepared(ctx);
        return 0;
    }

    leap_log_printf(
        "I/O session: re-bootstrapping owner session for EXCHANGE "
        "(outputs=0x%04X)\n",
        (unsigned)session_outputs);
    leap_conf_win_discard_controller_stack(ctx);

    if (ctx->has_peer_mac)
    {
        (void)leap_controller_peer_table_probe_peer(
            &ctx->table,
            &ctx->raw_io.stack_io,
            ctx->peer_mac,
            LEAP_CTRL_PEER_PROBE_TIMEOUT_MS);
    }

    if (leap_conf_win_bootstrap(ctx, session_outputs, &op) != 0 || !op)
    {
        leap_log_printf(
            "I/O session: EXCHANGE re-bootstrap failed (bootstrap to OP)\n");
        return -1;
    }

    leap_conf_win_apply_session_profile(ctx);
    leap_conf_win_apply_pd_outputs(ctx, session_outputs);
    if (leap_conf_win_probe_owner_session(ctx, session_outputs) != 0)
    {
        leap_log_printf(
            "I/O session: EXCHANGE re-bootstrap failed (owner PD EXCHANGE probe)\n");
        return -1;
    }

    ctx->cached_diag_valid = 0;
    ctx->force_diag_poll   = 1;
    leap_conf_win_mark_exchange_prepared(ctx);
    leap_log_printf(
        "I/O session: live OP on device (session_id=%u outputs=0x%04X)\n",
        (unsigned)leap_mgmt_controller_session_id(&ctx->stack.mgmt),
        (unsigned)session_outputs);
    return 0;
}

static int leap_conf_win_configure_soak_outputs(
    LeapConformanceWinContext* ctx,
    uint16_t                     outputs)
{
    const LeapPdProfileMap* profile;

    if (ctx == NULL)
    {
        return -1;
    }

    leap_conf_win_apply_session_profile(ctx);
    leap_conf_win_apply_pd_outputs(ctx, outputs);
    profile = &ctx->stack.pd.config.profile;
    if (outputs == 0u)
    {
        leap_log_printf(
            "PD outputs configured: rotating one-hot pattern "
            "(physical I/O exercise, EXCHANGE soak, profile=0x%08X "
            "write=0x%04X read=0x%04X payload=%u)\n",
            (unsigned)profile->profile_id,
            (unsigned)profile->write_endpoint_id,
            (unsigned)profile->read_endpoint_id,
            (unsigned)profile->endpoint_payload_size);
    }
    else
    {
        leap_log_printf(
            "PD outputs configured: 0x%04X (EXCHANGE soak, profile=0x%08X "
            "write=0x%04X read=0x%04X payload=%u)\n",
            (unsigned)outputs,
            (unsigned)profile->profile_id,
            (unsigned)profile->write_endpoint_id,
            (unsigned)profile->read_endpoint_id,
            (unsigned)profile->endpoint_payload_size);
    }
    return 0;
}

static void leap_conf_win_free_soak_rtt_capture(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    free(ctx->soak_rtt_samples);
    ctx->soak_rtt_samples   = NULL;
    ctx->soak_rtt_count     = 0u;
    ctx->soak_rtt_capacity  = 0u;
}

static int leap_conf_win_append_soak_rtt_sample(
    LeapConformanceWinContext* ctx,
    uint32_t                   sample)
{
    uint32_t* grown;
    uint32_t  new_capacity;

    if (ctx == NULL)
    {
        return -1;
    }

    if (ctx->soak_rtt_capacity == 0u)
    {
        ctx->soak_rtt_capacity = 8192u;
        ctx->soak_rtt_samples =
            (uint32_t*)malloc((size_t)ctx->soak_rtt_capacity * sizeof(uint32_t));
        if (ctx->soak_rtt_samples == NULL)
        {
            ctx->soak_rtt_capacity = 0u;
            return -1;
        }
    }

    if (ctx->soak_rtt_count >= ctx->soak_rtt_capacity)
    {
        new_capacity = ctx->soak_rtt_capacity * 2u;
        grown = (uint32_t*)realloc(
            ctx->soak_rtt_samples,
            (size_t)new_capacity * sizeof(uint32_t));
        if (grown == NULL)
        {
            return -1;
        }

        ctx->soak_rtt_samples  = grown;
        ctx->soak_rtt_capacity = new_capacity;
    }

    ctx->soak_rtt_samples[ctx->soak_rtt_count++] = sample;
    return 0;
}

static void leap_conf_win_export_downsampled_trend(
    const uint32_t*              raw,
    uint32_t                     raw_count,
    LeapConformanceLatencyTrend* out)
{
    uint32_t cap;
    uint32_t bucket;

    if (out == NULL)
    {
        return;
    }

    out->base_exchange = 0u;
    out->count         = 0u;
    if (raw == NULL || raw_count == 0u)
    {
        return;
    }

    cap = LEAP_PD_LATENCY_HISTORY_MAX;
    if (raw_count <= cap)
    {
        memcpy(out->samples, raw, (size_t)raw_count * sizeof(uint32_t));
        out->count = raw_count;
        return;
    }

    out->count = cap;
    for (bucket = 0u; bucket < cap; bucket++)
    {
        uint64_t start;
        uint64_t end;
        uint64_t index;
        uint32_t bucket_max = 0u;

        start = ((uint64_t)bucket * (uint64_t)raw_count) / (uint64_t)cap;
        end =
            ((uint64_t)(bucket + 1u) * (uint64_t)raw_count) / (uint64_t)cap;
        if (end <= start)
        {
            end = start + 1u;
        }

        for (index = start; index < end && index < raw_count; index++)
        {
            if (raw[index] > bucket_max)
            {
                bucket_max = raw[index];
            }
        }

        out->samples[bucket] = bucket_max;
    }
}

static void leap_conf_win_reset_latency_trend(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(&ctx->session_latency, 0, sizeof(ctx->session_latency));
    ctx->latency_last_merged_total = 0u;
    memset(&ctx->session_network_rtt, 0, sizeof(ctx->session_network_rtt));
    ctx->network_rtt_last_merged_total = 0u;
    memset(&ctx->session_pd, 0, sizeof(ctx->session_pd));
    leap_conf_win_free_soak_rtt_capture(ctx);
}

static void leap_conf_win_sync_session_pd(LeapConformanceWinContext* ctx)
{
    const LeapPdControllerStats* pd;

    if (ctx == NULL)
    {
        return;
    }

    pd = &ctx->stack.pd.stats;
    if (pd->cycles_completed > 0u)
    {
        ctx->session_pd = *pd;
    }
}

static void leap_conf_win_sync_session_latency(LeapConformanceWinContext* ctx)
{
    const LeapPdLatencyHistory* pd_history;
    uint32_t                    pd_total;
    uint32_t                    sample;

    if (ctx == NULL)
    {
        return;
    }

    pd_history = &ctx->stack.pd.latency_history;
    pd_total   = pd_history->total_samples;
    if (pd_total < ctx->latency_last_merged_total)
    {
        ctx->latency_last_merged_total = 0u;
    }

    while (ctx->latency_last_merged_total < pd_total)
    {
        if (leap_pd_latency_history_sample_at(
                pd_history,
                ctx->latency_last_merged_total,
                &sample) != 0)
        {
            break;
        }

        leap_pd_latency_history_push(&ctx->session_latency, sample);
        ctx->latency_last_merged_total++;
    }
}

static void leap_conf_win_sync_session_network_rtt(LeapConformanceWinContext* ctx)
{
    const LeapPdLatencyHistory* pd_history;
    uint32_t                    pd_total;
    uint32_t                    sample;

    if (ctx == NULL)
    {
        return;
    }

    pd_history = &ctx->stack.pd.network_rtt_history;
    pd_total   = pd_history->total_samples;
    if (pd_total < ctx->network_rtt_last_merged_total)
    {
        ctx->network_rtt_last_merged_total = 0u;
    }

    while (ctx->network_rtt_last_merged_total < pd_total)
    {
        if (leap_pd_latency_history_sample_at(
                pd_history,
                ctx->network_rtt_last_merged_total,
                &sample) != 0)
        {
            break;
        }

        leap_pd_latency_history_push(&ctx->session_network_rtt, sample);
        if (ctx->soak_in_progress != 0)
        {
            (void)leap_conf_win_append_soak_rtt_sample(ctx, sample);
        }
        ctx->network_rtt_last_merged_total++;
    }
}

void leap_conformance_win_reset_latency_trend(LeapConformanceWinContext* ctx)
{
    leap_conf_win_reset_latency_trend(ctx);
}

static int leap_conf_win_wait_service_reply(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    uint16_t                   service_id,
    uint16_t                   expect_type,
    uint8_t*                   reply,
    size_t                     reply_cap,
    size_t*                    reply_len,
    int                        timeout_ms)
{
    uint64_t deadline;

    if (ctx == NULL || peer_mac == NULL || reply == NULL || reply_len == NULL)
    {
        return -1;
    }

    deadline = leap_raw_winpcap_monotonic_us() + (uint64_t)timeout_ms * 1000u;

    for (;;)
    {
        uint8_t       src_mac[6];
        uint8_t       frame_buf[1600];
        size_t        frame_length = 0u;
        LeapFrameView view;
        int           wait_ms;
        uint64_t      now_us;

        now_us = leap_raw_winpcap_monotonic_us();
        if (now_us >= deadline)
        {
            return -1;
        }

        wait_ms = (int)((deadline - now_us + 999u) / 1000u);
        if (wait_ms <= 0)
        {
            wait_ms = 1;
        }

        if (leap_raw_winpcap_recv(
                &ctx->transport,
                src_mac,
                frame_buf,
                sizeof(frame_buf),
                &frame_length,
                wait_ms,
                NULL) != 0)
        {
            return -1;
        }

        if (memcmp(src_mac, peer_mac, 6) != 0)
        {
            continue;
        }

        if (leap_frame_parse(frame_buf, frame_length, &view) != LEAP_FRAME_OK)
        {
            continue;
        }

        if (view.header.service_id != service_id ||
            view.header.message_type != expect_type)
        {
            continue;
        }

        if (view.payload_length > reply_cap)
        {
            return -1;
        }

        if (view.payload_length > 0u && view.payload != NULL)
        {
            memcpy(reply, view.payload, view.payload_length);
        }

        *reply_len = view.payload_length;
        return 0;
    }
}

static int leap_conf_win_wait_disc_reply(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    uint16_t                   expect_type,
    uint8_t*                   reply,
    size_t                     reply_cap,
    size_t*                    reply_len,
    int                        timeout_ms)
{
    return leap_conf_win_wait_service_reply(
        ctx,
        peer_mac,
        (uint16_t)LEAP_SERVICE_DISC,
        expect_type,
        reply,
        reply_cap,
        reply_len,
        timeout_ms);
}

static void leap_conf_win_release_before_disc(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac)
{
    const LeapControllerPeerEntry* entry;
    int                            index;

    if (ctx == NULL || peer_mac == NULL || !ctx->transport_open)
    {
        return;
    }

    (void)leap_raw_winpcap_drain_rx(&ctx->transport);

    if (ctx->stack.peer_bound != 0 && ctx->stack.mgmt.session_id != 0u)
    {
        (void)leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        (void)leap_raw_winpcap_drain_rx(&ctx->transport);
        return;
    }

    index = leap_controller_peer_table_find(&ctx->table, peer_mac);
    entry = (index >= 0)
                ? leap_controller_peer_table_get(&ctx->table, (unsigned)index)
                : NULL;

    if (entry != NULL &&
        entry->device_state == (uint16_t)LEAP_STATE_OP &&
        leap_controller_peer_owned_by_other(
            entry, ctx->transport.local_mac) == 0)
    {
        LeapControllerStackConfig stack_config;
        LeapHelloReply             hello;

        memset(&hello, 0, sizeof(hello));
        hello.active_profile_id  = entry->active_profile_id;
        hello.default_profile_id = entry->default_profile_id;
        hello.current_state      = entry->device_state;
        memcpy(hello.active_owner_mac, entry->active_owner_mac, 6);

        memset(&stack_config, 0, sizeof(stack_config));
        memcpy(stack_config.mgmt.controller_mac, ctx->transport.local_mac, 6);
        memcpy(stack_config.target_peer_mac, peer_mac, 6);
        stack_config.bootstrap_lease_us    = LEAP_CONF_BOOTSTRAP_LEASE_US;
        stack_config.bootstrap_watchdog_us = LEAP_CONF_BOOTSTRAP_WATCHDOG_US;
        stack_config.recv_timeout_ms       = 2000;
        leap_controller_stack_init(&ctx->stack, &stack_config);

        if (leap_controller_stack_bootstrap_peer(
                &ctx->stack,
                &ctx->raw_io.stack_io,
                peer_mac,
                &hello) == LEAP_CTRL_STACK_OK)
        {
            (void)leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        }
        else
        {
            leap_controller_stack_reset(&ctx->stack);
        }

        (void)leap_raw_winpcap_drain_rx(&ctx->transport);
    }
}

static int leap_conf_win_bootstrap_once(
    LeapConformanceWinContext* ctx,
    uint16_t                   outputs,
    int*                       op_out)
{
    uint8_t peer_mac[6];

    if (ctx == NULL || op_out == NULL)
    {
        return -1;
    }

    *op_out = 0;

    if (ctx->has_peer_mac)
    {
        memcpy(ctx->stack.config.target_peer_mac, ctx->peer_mac, 6);
        ctx->stack.config.single_peer_auto_select = 0;
    }
    else
    {
        ctx->stack.config.single_peer_auto_select = 1;
    }

    leap_conf_win_apply_pd_outputs(ctx, outputs);

    if (leap_controller_stack_bootstrap(
            &ctx->stack,
            &ctx->raw_io.stack_io,
            peer_mac) != LEAP_CTRL_STACK_OK)
    {
        return -1;
    }

    if (!ctx->has_peer_mac)
    {
        memcpy(ctx->peer_mac, peer_mac, 6);
        ctx->has_peer_mac = 1;
    }

    *op_out = (leap_controller_stack_get_phase(&ctx->stack) == LEAP_CTRL_STACK_OP);
    return 0;
}

int leap_conformance_win_session_is_op(const LeapConformanceWinContext* ctx)
{
    if (ctx == NULL || !ctx->transport_open || !ctx->has_peer_mac)
    {
        return 0;
    }

    if (ctx->stack.peer_bound == 0)
    {
        return 0;
    }

    if (memcmp(ctx->stack.peer_mac, ctx->peer_mac, 6) != 0)
    {
        return 0;
    }

    return (leap_controller_stack_get_phase(&ctx->stack) == LEAP_CTRL_STACK_OP &&
            leap_mgmt_controller_get_state(&ctx->stack.mgmt) == LEAP_MGMT_CTRL_OP) ?
               1 :
               0;
}

int leap_conformance_win_ensure_op(LeapConformanceWinContext* ctx, uint16_t outputs)
{
    int op = 0;

    if (ctx == NULL)
    {
        return -1;
    }

    if (leap_conformance_win_session_is_op(ctx) != 0)
    {
        (void)leap_raw_winpcap_drain_rx(&ctx->transport);
        return 0;
    }

    if (leap_conf_win_bootstrap(ctx, outputs, &op) != 0 || !op)
    {
        return -1;
    }

    return 0;
}

static int leap_conf_win_bootstrap(
    LeapConformanceWinContext* ctx,
    uint16_t                   outputs,
    int*                       op_out)
{
    unsigned attempt;

    if (ctx == NULL)
    {
        return -1;
    }

    leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);

    memset(&ctx->stack, 0, sizeof(ctx->stack));
    {
        LeapControllerStackConfig stack_config;

        memset(&stack_config, 0, sizeof(stack_config));
        memcpy(stack_config.mgmt.controller_mac, ctx->transport.local_mac, 6);
        stack_config.bootstrap_lease_us     = LEAP_CONF_BOOTSTRAP_LEASE_US;
        stack_config.bootstrap_watchdog_us  = LEAP_CONF_BOOTSTRAP_WATCHDOG_US;
        stack_config.recv_timeout_ms        = ctx->has_peer_mac ?
                                                LEAP_CONF_BOOTSTRAP_RECV_MS_KNOWN :
                                                LEAP_CONF_BOOTSTRAP_RECV_MS;
        stack_config.pd.cycle_period_ms     = 100u;
        stack_config.pd.heartbeat_every_n_cycles = 10u;
        if (outputs != 0u)
        {
            stack_config.pd.use_fixed_outputs     = 1;
            stack_config.pd.fixed_digital_outputs = outputs;
        }
        if (ctx->has_peer_mac)
        {
            memcpy(stack_config.target_peer_mac, ctx->peer_mac, 6);
            stack_config.default_profile_id =
                leap_conf_win_fallback_profile_id(ctx);
        }
        else
        {
            stack_config.single_peer_auto_select = 1;
        }
        leap_controller_stack_init(&ctx->stack, &stack_config);
    }

    for (attempt = 1u; attempt <= ctx->bootstrap_retries; attempt++)
    {
        if (attempt > 1u)
        {
#if defined(_WIN32)
            Sleep(ctx->retry_delay_ms);
#endif
        }

        if (leap_conf_win_bootstrap_once(ctx, outputs, op_out) == 0 && *op_out)
        {
            return 0;
        }
    }

    return -1;
}

static int leap_conf_win_open(
    void*       user_ctx,
    const char* adapter,
    const char* capture_pcap)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    LeapRawWinpcapOpenOptions    open_options;

    if (ctx == NULL)
    {
        return -1;
    }

    if (adapter != NULL && ctx->transport_open &&
        strcmp(ctx->open_adapter_path, adapter) == 0)
    {
        return 0;
    }

    if (ctx->transport_open)
    {
        leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        leap_raw_winpcap_close(&ctx->transport);
        ctx->transport_open = 0;
        ctx->open_adapter_path[0] = '\0';
        ctx->session_caps_valid = 0;
        memset(&ctx->session_caps, 0, sizeof(ctx->session_caps));
        ctx->exchange_session_ready       = 0;
        ctx->last_exchange_prepare_us     = 0u;
        leap_conf_win_reset_latency_trend(ctx);
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = 1;
    open_options.filter_leap_ethertype = 1;
    if (capture_pcap != NULL)
    {
        (void)snprintf(
            ctx->capture_path,
            sizeof(ctx->capture_path),
            "%s",
            capture_pcap);
        open_options.capture_path = ctx->capture_path;
    }

    if (leap_raw_winpcap_open(
            &ctx->transport,
            adapter,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        return -1;
    }

    (void)leap_raw_winpcap_drain_rx(&ctx->transport);
    leap_conformance_raw_io_bind(&ctx->raw_io, &ctx->transport);
    leap_controller_peer_table_init(&ctx->table);
    ctx->transport_open = 1;
    ctx->cancel_flag    = 0;
    if (adapter != NULL)
    {
        (void)snprintf(
            ctx->open_adapter_path,
            sizeof(ctx->open_adapter_path),
            "%s",
            adapter);
    }
    else
    {
        ctx->open_adapter_path[0] = '\0';
    }
    return 0;
}

static void leap_conf_win_close(void* user_ctx)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;

    if (ctx == NULL)
    {
        return;
    }

    if (ctx->transport_open)
    {
        leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        leap_raw_winpcap_close(&ctx->transport);
        ctx->transport_open = 0;
        ctx->open_adapter_path[0] = '\0';
        ctx->session_caps_valid = 0;
        memset(&ctx->session_caps, 0, sizeof(ctx->session_caps));
        ctx->exchange_session_ready       = 0;
        ctx->last_exchange_prepare_us     = 0u;
        leap_conf_win_reset_latency_trend(ctx);
    }
}

static int leap_conf_win_discover(void* user_ctx, int scan_ms, unsigned* peer_count_out)
{
    LeapConformanceWinContext*       ctx = (LeapConformanceWinContext*)user_ctx;
    LeapControllerPeerDiscoverConfig disc_config;
    LeapControllerPeerStatus         status;

    if (ctx == NULL || peer_count_out == NULL)
    {
        return -1;
    }

    if (!ctx->transport_open)
    {
        return -1;
    }

    leap_controller_peer_table_init(&ctx->table);

    /*
     * scan_ms > 0: explicit Discovery-tab / CLI broadcast scan — always use
     * discover_ex even when a peer MAC is configured for I/O bench.
     * scan_ms == 0: internal prepare — probe the known peer when possible.
     */
    if (scan_ms <= 0 && ctx->has_peer_mac)
    {
        status = leap_controller_peer_table_probe_peer(
            &ctx->table,
            &ctx->raw_io.stack_io,
            ctx->peer_mac,
            LEAP_CTRL_PEER_PROBE_TIMEOUT_MS);
        if (status != LEAP_CTRL_PEER_OK)
        {
            return -1;
        }

        *peer_count_out = ctx->table.count;
        return (ctx->table.count > 0u) ? 0 : -1;
    }

    memset(&disc_config, 0, sizeof(disc_config));
    disc_config.scan_duration_ms = scan_ms > 0 ?
                                       scan_ms :
                                       LEAP_CONF_DISCOVER_BROADCAST_MS;
    disc_config.min_peers        = 0u;

    status = leap_controller_peer_table_discover_ex(
        &ctx->table,
        &ctx->raw_io.stack_io,
        &disc_config);
    if (status != LEAP_CTRL_PEER_OK)
    {
        return -1;
    }

    *peer_count_out = ctx->table.count;
    return 0;
}

static int leap_conf_win_find_peer(
    void*            user_ctx,
    const uint8_t*   expected_mac,
    int*             found_out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    int                          index;

    if (ctx == NULL || expected_mac == NULL || found_out == NULL)
    {
        return -1;
    }

    index = leap_controller_peer_table_find(&ctx->table, expected_mac);
    *found_out = (index >= 0);
    return 0;
}

static int leap_conf_win_pd_write(
    void*    user_ctx,
    uint16_t outputs,
    int*     sent_out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;

    if (ctx == NULL || sent_out == NULL)
    {
        return -1;
    }

    *sent_out = 0;
    if (leap_controller_stack_pd_single_write(
            &ctx->stack,
            &ctx->raw_io.pd_io,
            outputs) == LEAP_PD_CTRL_OK)
    {
        *sent_out = 1;
        return 0;
    }

    return -1;
}

static const char* leap_conf_win_diag_status_text(
    LeapControllerStackDiagStatus status)
{
    switch (status)
    {
    case LEAP_CTRL_STACK_DIAG_NOT_OP:
        return "controller not OP";
    case LEAP_CTRL_STACK_DIAG_SEND_FAILED:
        return "send failed";
    case LEAP_CTRL_STACK_DIAG_RECV_TIMEOUT:
        return "recv timeout (stale RX or device error)";
    case LEAP_CTRL_STACK_DIAG_UNEXPECTED_REPLY:
        return "device error reply";
    case LEAP_CTRL_STACK_DIAG_PARSE_ERROR:
        return "reply parse error";
    default:
        return "unknown error";
    }
}

static int leap_conf_win_read_diag(void* user_ctx, int* ok_out)
{
    LeapConformanceWinContext*    ctx = (LeapConformanceWinContext*)user_ctx;
    LeapControllerStackDiagResult result;
    LeapControllerStackDiagStatus status;
    int                           op = 0;
    int                           saved_recv_timeout_ms = 0;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    *ok_out = 0;
    (void)leap_raw_winpcap_drain_rx(&ctx->transport);

    if (leap_conformance_win_session_is_op(ctx) == 0)
    {
        if (leap_conf_win_bootstrap(ctx, 0x0001u, &op) != 0 || !op)
        {
            return -1;
        }
    }
    else if (ctx->has_peer_mac &&
             leap_conf_win_verify_device_session(ctx) != 0)
    {
        leap_log_printf(
            "DIAG readback: owner session mismatch, re-bootstrapping\n");
        if (leap_conf_win_bootstrap(ctx, 0x0001u, &op) != 0 || !op)
        {
            return -1;
        }
    }

    (void)leap_raw_winpcap_drain_rx(&ctx->transport);

    saved_recv_timeout_ms = ctx->stack.config.recv_timeout_ms;
    if (ctx->stack.config.recv_timeout_ms < 2000)
    {
        ctx->stack.config.recv_timeout_ms = 2000;
    }

    status = leap_controller_stack_read_diag(
        &ctx->stack,
        &ctx->raw_io.stack_io,
        &result);

    ctx->stack.config.recv_timeout_ms = saved_recv_timeout_ms;
    if (status != LEAP_CTRL_STACK_DIAG_OK)
    {
        leap_log_printf(
            "DIAG readback failed: %s (session_id=%u)\n",
            leap_conf_win_diag_status_text(status),
            (unsigned)leap_mgmt_controller_session_id(&ctx->stack.mgmt));
    }

    *ok_out = (status == LEAP_CTRL_STACK_DIAG_OK);
    return (*ok_out) ? 0 : -1;
}

static int leap_conf_win_lease_demo(void* user_ctx, int* ok_out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    int                          op = 0;

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    ctx->stack.config.bootstrap_lease_us = LEAP_CONF_LEASE_DEMO_US;
    if (leap_conf_win_bootstrap(ctx, 0x0001u, &op) != 0 || !op)
    {
        return -1;
    }

#if defined(_WIN32)
    Sleep(LEAP_CONF_LEASE_DEMO_IDLE_S * 1000u);
#endif

    *ok_out = 1;
    return 0;
}

typedef struct LeapConfCyclicTimerArgs
{
    volatile int* stop_flag;
    unsigned      seconds;
} LeapConfCyclicTimerArgs;

typedef struct LeapConfMetricsPollArgs
{
    LeapConformanceWinContext* ctx;
    volatile int*            stop_flag;
} LeapConfMetricsPollArgs;

static void leap_conf_win_stop_metrics_poll(LeapConformanceWinContext* ctx);
static void leap_conf_win_start_metrics_poll(LeapConformanceWinContext* ctx);

#if defined(_WIN32)
static DWORD WINAPI leap_conf_metrics_poll_thread(LPVOID param)
{
    LeapConfMetricsPollArgs* args = (LeapConfMetricsPollArgs*)param;
    LeapConformanceMetrics   metrics;
    LeapConformanceProgress  progress;

    if (args == NULL || args->ctx == NULL || args->stop_flag == NULL)
    {
        return 0;
    }

    while (*args->stop_flag == 0)
    {
        if (args->ctx->progress_fn != NULL && args->ctx->io.snapshot != NULL)
        {
            memset(&metrics, 0, sizeof(metrics));
            if (args->ctx->io.snapshot(args->ctx->io.user_ctx, &metrics) == 0)
            {
                memset(&progress, 0, sizeof(progress));
                progress.phase   = LEAP_CONF_PROGRESS_METRICS;
                progress.metrics = &metrics;
                args->ctx->progress_fn(args->ctx->progress_ctx, &progress);
            }
        }
        Sleep(100);
    }

    free(args);
    return 0;
}

static void leap_conf_win_start_metrics_poll(LeapConformanceWinContext* ctx)
{
    LeapConfMetricsPollArgs* args;

    if (ctx == NULL || ctx->progress_fn == NULL)
    {
        return;
    }

    leap_conf_win_stop_metrics_poll(ctx);
    args = (LeapConfMetricsPollArgs*)calloc(1, sizeof(*args));
    if (args == NULL)
    {
        return;
    }

    args->ctx       = ctx;
    args->stop_flag = &ctx->cancel_flag;
    ctx->metrics_poll_thread = CreateThread(
        NULL,
        0,
        leap_conf_metrics_poll_thread,
        args,
        0,
        NULL);
}

static void leap_conf_win_stop_metrics_poll(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL || ctx->metrics_poll_thread == NULL)
    {
        return;
    }

    WaitForSingleObject(ctx->metrics_poll_thread, INFINITE);
    CloseHandle(ctx->metrics_poll_thread);
    ctx->metrics_poll_thread = NULL;
}
#else
static void leap_conf_win_start_metrics_poll(LeapConformanceWinContext* ctx)
{
    (void)ctx;
}

static void leap_conf_win_stop_metrics_poll(LeapConformanceWinContext* ctx)
{
    (void)ctx;
}
#endif

#if defined(_WIN32)
static DWORD WINAPI leap_conf_cyclic_timer_thread(LPVOID param)
{
    LeapConfCyclicTimerArgs* args = (LeapConfCyclicTimerArgs*)param;

    if (args != NULL && args->stop_flag != NULL && args->seconds > 0u)
    {
        Sleep(args->seconds * 1000u);
        *args->stop_flag = 1;
    }

    return 0;
}
#endif

static int leap_conf_win_cyclic(
    void*                  user_ctx,
    uint16_t               outputs,
    int                    exchange,
    unsigned               seconds,
    unsigned               cyclic_ms,
    LeapPdControllerStats* stats_out,
    int*                   ok_out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    int                        op = 0;
    LeapPdControllerStatus     pd_status;

#if defined(_WIN32)
    LeapConfCyclicTimerArgs    timer_args;
    HANDLE                     timer_thread = NULL;
#endif

    if (ctx == NULL || ok_out == NULL)
    {
        return -1;
    }

    if (leap_conf_win_bootstrap(ctx, outputs, &op) != 0 || !op)
    {
        return -1;
    }

    leap_conf_win_reset_latency_trend(ctx);
    memset(&ctx->stack.pd.latency_history, 0, sizeof(ctx->stack.pd.latency_history));
    memset(&ctx->stack.pd.network_rtt_history, 0,
           sizeof(ctx->stack.pd.network_rtt_history));
    leap_pd_controller_reset_stats(&ctx->stack.pd);

    {
        unsigned period = cyclic_ms;

        ctx->stack.config.pd.cycle_period_ms    = period;
        ctx->stack.config.pd.use_exchange       = exchange;
        ctx->stack.config.pd.stats_log_interval = 0u;
        ctx->stack.pd.config.cycle_period_ms    = period;
        ctx->stack.pd.config.use_exchange         = exchange;
        ctx->stack.pd.config.stats_log_interval = 0u;
    }

    leap_conf_win_apply_session_profile(ctx);
    leap_conf_win_apply_pd_outputs(
        ctx, leap_conf_win_soak_outputs(ctx, outputs));

    ctx->cancel_flag      = 0;
    ctx->soak_in_progress = 1;
    leap_win_install_ctrl_handler(&ctx->cancel_flag);
    leap_conf_win_start_metrics_poll(ctx);

#if defined(_WIN32)
    timer_args.stop_flag = &ctx->cancel_flag;
    timer_args.seconds   = seconds > 0u ? seconds : 2u;
    timer_thread = CreateThread(
        NULL,
        0,
        leap_conf_cyclic_timer_thread,
        &timer_args,
        0,
        NULL);

    pd_status = leap_win_controller_run_cyclic_pd_with_link_watch(
        &ctx->stack,
        &ctx->raw_io.pd_io,
        &ctx->transport,
        &ctx->cancel_flag);

    if (timer_thread != NULL)
    {
        WaitForSingleObject(timer_thread, INFINITE);
        CloseHandle(timer_thread);
    }
#else
    (void)seconds;
    pd_status = LEAP_PD_CTRL_STOPPED;
#endif

    leap_conf_win_stop_metrics_poll(ctx);
    ctx->soak_in_progress = 0;

    if (stats_out != NULL)
    {
        *stats_out = ctx->stack.pd.stats;
    }

    *ok_out = (pd_status == LEAP_PD_CTRL_OK || pd_status == LEAP_PD_CTRL_STOPPED) &&
              ctx->stack.pd.stats.pd_sent_fail == 0u;
    return 0;
}

static int leap_conf_win_send_identify_and_wait(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    LeapIdentifyReply*         reply_out,
    uint8_t*                   payload_out,
    size_t                     payload_cap,
    size_t*                    payload_len_out)
{
    uint8_t         payload[64];
    size_t          payload_length;
    uint8_t         reply[256];
    size_t          reply_length = 0u;
    LeapIdentifyReply identify_reply;

    if (ctx == NULL || peer_mac == NULL || !ctx->transport_open)
    {
        return -1;
    }

    payload_length = leap_disc_controller_build_identify(
        NULL, 0u, payload, sizeof(payload));
    if (payload_length == 0u)
    {
        return -1;
    }

    if (leap_conformance_raw_send_disc(
            &ctx->raw_io,
            peer_mac,
            LEAP_DISC_IDENTIFY,
            payload,
            payload_length) != 0)
    {
        return -1;
    }

    if (leap_conf_win_wait_disc_reply(
            ctx,
            peer_mac,
            LEAP_DISC_IDENTIFY_REPLY,
            reply,
            sizeof(reply),
            &reply_length,
            2000) != 0)
    {
        return -1;
    }

    if (leap_disc_controller_on_identify_reply(
            reply, reply_length, &identify_reply) != LEAP_DISC_CTRL_OK)
    {
        return -1;
    }

    if (reply_out != NULL)
    {
        *reply_out = identify_reply;
    }

    if (payload_out != NULL && payload_len_out != NULL)
    {
        if (reply_length > payload_cap)
        {
            return -1;
        }

        if (reply_length > 0u)
        {
            memcpy(payload_out, reply, reply_length);
        }
        *payload_len_out = reply_length;
    }

    return 0;
}

static int leap_conf_win_probe_capabilities(
    void*                        user_ctx,
    const uint8_t*               peer_mac,
    LeapConformanceDeviceCaps*     caps_out)
{
    LeapConformanceWinContext*     ctx = (LeapConformanceWinContext*)user_ctx;
    uint8_t                        identify_buf[256];
    size_t                         identify_len = 0u;
    LeapIdentifyReply              identify_reply;
    LeapDirControllerCapabilities  dir_caps;

    if (ctx == NULL || peer_mac == NULL || caps_out == NULL || !ctx->transport_open)
    {
        return -1;
    }

    leap_conformance_device_caps_init(caps_out);
    leap_dir_controller_capabilities_init(&dir_caps);

    leap_conf_win_release_before_disc(ctx, peer_mac);

    if (leap_conf_win_send_identify_and_wait(
            ctx,
            peer_mac,
            &identify_reply,
            identify_buf,
            sizeof(identify_buf),
            &identify_len) != 0)
    {
        return -1;
    }

    {
        LeapControllerStackConfig stack_config;

        leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        memset(&ctx->stack, 0, sizeof(ctx->stack));
        memset(&stack_config, 0, sizeof(stack_config));
        memcpy(stack_config.mgmt.controller_mac, ctx->transport.local_mac, 6);
        stack_config.bootstrap_lease_us    = LEAP_CONF_BOOTSTRAP_LEASE_US;
        stack_config.bootstrap_watchdog_us = LEAP_CONF_BOOTSTRAP_WATCHDOG_US;
        stack_config.recv_timeout_ms       = 3000;
        leap_controller_stack_init(&ctx->stack, &stack_config);

        (void)leap_raw_winpcap_drain_rx(&ctx->transport);

        {
            LeapControllerStackStatus probe_status;
            uint32_t                  profile_id;

            probe_status = leap_controller_stack_probe_directory(
                &ctx->stack,
                &ctx->raw_io.stack_io,
                peer_mac,
                &dir_caps);

            profile_id = identify_reply.active_profile_id;
            if (profile_id == 0u)
            {
                profile_id = identify_reply.default_profile_id;
            }

            if (probe_status != LEAP_CTRL_STACK_OK ||
                dir_caps.endpoint_count == 0u ||
                !dir_caps.has_digital_outputs ||
                dir_caps.output_bit_count == 0u)
            {
                char ep_list[96];
                size_t ep_pos = 0u;
                ep_list[0] = '\0';
                for (size_t k = 0u;
                     k < dir_caps.endpoint_count && ep_pos < sizeof(ep_list) - 8u;
                     k++)
                {
                    if (k > 0u)
                    {
                        ep_pos += (size_t)snprintf(ep_list + ep_pos,
                                                   sizeof(ep_list) - ep_pos,
                                                   ",");
                    }
                    ep_pos += (size_t)snprintf(ep_list + ep_pos,
                                               sizeof(ep_list) - ep_pos,
                                               "0x%04X",
                                               (unsigned)dir_caps.endpoints[k].endpoint_id);
                }
                (void)snprintf(
                    caps_out->probe_detail,
                    sizeof(caps_out->probe_detail),
                    "LEAP-DIR failed (state=0x%04X profile=0x%08X status=%d "
                    "count=%u has_out=%d bit=%u eps=%s); "
                    "device must reply to READ_DIRECTORY or READ_OBJECT with "
                    "endpoint descriptors",
                    identify_reply.current_state,
                    profile_id,
                    (int)probe_status,
                    (unsigned)dir_caps.endpoint_count,
                    dir_caps.has_digital_outputs ? 1 : 0,
                    (unsigned)dir_caps.output_bit_count,
                    ep_list[0] ? ep_list : "none");
                (void)leap_controller_stack_release(
                    &ctx->stack, &ctx->raw_io.stack_io);
                leap_controller_stack_reset(&ctx->stack);
                return -1;
            }
        }

        if (dir_caps.locate_capability_flags == 0u)
        {
            dir_caps.locate_capability_flags =
                identify_reply.locate_capability_flags;
        }

        (void)leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        leap_controller_stack_reset(&ctx->stack);
        leap_conformance_device_caps_from_dir(&dir_caps, caps_out);

        if (!caps_out->valid || caps_out->pd_mask_count == 0u)
        {
            return -1;
        }

        return 0;
    }
}

int leap_conformance_win_query_identify(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    LeapIdentifyReply*         reply_out)
{
    if (ctx == NULL || peer_mac == NULL || reply_out == NULL)
    {
        return -1;
    }

    return leap_conf_win_send_identify_and_wait(
        ctx, peer_mac, reply_out, NULL, 0u, NULL);
}

static int leap_conf_win_identify(
    void*          user_ctx,
    const uint8_t* peer_mac,
    int*           ok_out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    LeapIdentifyReply            identify_reply;

    if (ctx == NULL || peer_mac == NULL || ok_out == NULL || !ctx->transport_open)
    {
        return -1;
    }

    *ok_out = 0;
    leap_conf_win_release_before_disc(ctx, peer_mac);

    if (leap_conf_win_send_identify_and_wait(
            ctx, peer_mac, &identify_reply, NULL, 0u, NULL) == 0)
    {
        *ok_out = 1;
        return 0;
    }

    return -1;
}

static int leap_conf_win_locate(
    void*          user_ctx,
    const uint8_t* peer_mac,
    unsigned       duration_ms,
    int*           ok_out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    uint8_t                    payload[64];
    size_t                     payload_length;
    uint8_t                    reply[64];
    size_t                     reply_length = 0u;
    LeapLocateDeviceReply        locate_reply;

    if (ctx == NULL || peer_mac == NULL || ok_out == NULL || !ctx->transport_open)
    {
        return -1;
    }

    *ok_out = 0;
    leap_conf_win_release_before_disc(ctx, peer_mac);

    payload_length = leap_disc_controller_build_locate_device(
        (uint16_t)duration_ms,
        LEAP_LOCATE_PATTERN_SLOW_BLINK,
        LEAP_LOCATE_FLAG_LED,
        payload,
        sizeof(payload));
    if (payload_length == 0u)
    {
        return -1;
    }

    if (leap_conformance_raw_send_disc(
            &ctx->raw_io,
            peer_mac,
            LEAP_DISC_LOCATE_DEVICE,
            payload,
            payload_length) != 0)
    {
        return -1;
    }

    if (leap_conf_win_wait_disc_reply(
            ctx,
            peer_mac,
            LEAP_DISC_LOCATE_DEVICE_REPLY,
            reply,
            sizeof(reply),
            &reply_length,
            2000) != 0)
    {
        return -1;
    }

    if (leap_disc_controller_on_locate_device_reply(
            reply, reply_length, &locate_reply) == LEAP_DISC_CTRL_OK)
    {
        *ok_out = 1;
    }

    return (*ok_out) ? 0 : -1;
}

static int leap_conf_mac_is_zero(const uint8_t mac[6])
{
    static const uint8_t zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };

    if (mac == NULL)
    {
        return 1;
    }

    return memcmp(mac, zero, 6) == 0;
}

static const uint8_t* leap_conf_win_target_mac(const LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    if (ctx->has_peer_mac)
    {
        return ctx->peer_mac;
    }

    if (ctx->stack.peer_bound)
    {
        return ctx->stack.peer_mac;
    }

    return NULL;
}

static const LeapControllerPeerEntry* leap_conf_win_target_peer(
    const LeapConformanceWinContext* ctx)
{
    const uint8_t* peer_mac;

    if (ctx == NULL)
    {
        return NULL;
    }

    peer_mac = leap_conf_win_target_mac(ctx);
    if (peer_mac == NULL)
    {
        return NULL;
    }

    {
        int index = leap_controller_peer_table_find(&ctx->table, peer_mac);

        if (index < 0)
        {
            return NULL;
        }

        return leap_controller_peer_table_get(&ctx->table, (unsigned)index);
    }
}

static uint64_t leap_conf_diag_counter_value(
    const LeapCounterEntry* entries,
    uint16_t                count,
    uint16_t                counter_id)
{
    uint16_t i;

    if (entries == NULL)
    {
        return 0u;
    }

    for (i = 0u; i < count; i++)
    {
        if (entries[i].counter_id == counter_id)
        {
            return entries[i].value;
        }
    }

    return 0u;
}

static void leap_conf_win_estimate_lease_watchdog(
    const LeapControllerStack* stack,
    LeapConformanceMetrics*    out,
    uint64_t                   now_us)
{
    uint64_t elapsed_us;

    if (stack == NULL || out == NULL)
    {
        return;
    }

    if (leap_mgmt_controller_get_state(&stack->mgmt) != LEAP_MGMT_CTRL_OP)
    {
        return;
    }

    if (stack->mgmt.granted_lease_us == 0u || stack->mgmt.last_lease_refresh_us == 0u)
    {
        return;
    }

    elapsed_us = now_us - stack->mgmt.last_lease_refresh_us;
    if (stack->mgmt.granted_lease_us > elapsed_us)
    {
        out->lease_remaining_us =
            (uint32_t)(stack->mgmt.granted_lease_us - elapsed_us);
    }
    else
    {
        out->lease_remaining_us = 0u;
    }

    out->watchdog_remaining_us = stack->mgmt.granted_watchdog_us;
    out->has_lease_watchdog    = 1;
}

static void leap_conf_win_poll_device_diag(LeapConformanceWinContext* ctx)
{
    LeapControllerStackDiagResult result;
    uint64_t                      now_us;

    if (ctx == NULL)
    {
        return;
    }

    if (ctx->soak_in_progress != 0)
    {
        return;
    }

    if (leap_controller_stack_get_phase(&ctx->stack) != LEAP_CTRL_STACK_OP)
    {
        return;
    }

    now_us = leap_raw_winpcap_monotonic_us();
    if (ctx->force_diag_poll == 0 && ctx->cached_diag_valid != 0 &&
        now_us >= ctx->last_diag_poll_us &&
        (now_us - ctx->last_diag_poll_us) < LEAP_CONF_DIAG_POLL_INTERVAL_US)
    {
        return;
    }

    memset(&result, 0, sizeof(result));
    if (leap_controller_stack_read_diag(
            &ctx->stack,
            &ctx->raw_io.stack_io,
            &result) != LEAP_CTRL_STACK_DIAG_OK)
    {
        return;
    }

    ctx->cached_diag           = result;
    ctx->cached_diag_valid     = 1;
    ctx->last_diag_poll_us     = now_us;
    ctx->force_diag_poll       = 0;
}

static void leap_conf_win_apply_device_diag(
    LeapConformanceWinContext* ctx,
    LeapConformanceMetrics*      out)
{
    const LeapControllerStackDiagResult* diag;

    if (ctx == NULL || out == NULL || ctx->cached_diag_valid == 0)
    {
        return;
    }

    diag = &ctx->cached_diag;
    out->has_device_diag       = 1;
    out->diag_counter_count    = diag->counter_count;
    memcpy(
        out->diag_counters,
        diag->counters,
        sizeof(out->diag_counters[0]) * diag->counter_count);

    if (diag->has_timing != 0)
    {
        out->timing                = diag->timing;
        out->lease_remaining_us    = diag->timing.owner_lease_remaining_us;
        out->watchdog_remaining_us = diag->timing.process_watchdog_remaining_us;
        out->has_lease_watchdog    = 1;
        out->last_cycle_time_us    = diag->timing.last_cycle_time_us;
        out->worst_cycle_time_us   = diag->timing.max_cycle_time_us;
        out->has_cycle_timing      = 1;
    }

    out->rx_frames = leap_conf_diag_counter_value(
        diag->counters,
        diag->counter_count,
        (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED);
    out->tx_frames = leap_conf_diag_counter_value(
        diag->counters,
        diag->counter_count,
        (uint16_t)LEAP_COUNTER_TX_FRAMES_ACCEPTED);
    out->duplicate_frames = leap_conf_diag_counter_value(
        diag->counters,
        diag->counter_count,
        (uint16_t)LEAP_COUNTER_DUPLICATE_SEQUENCES);
    out->stale_frames = leap_conf_diag_counter_value(
        diag->counters,
        diag->counter_count,
        (uint16_t)LEAP_COUNTER_STALE_PROCESS_FRAMES);
    out->frames_from_device = 1;
}

static void leap_conf_win_fill_session_fields(
    LeapConformanceWinContext* ctx,
    LeapConformanceMetrics*      out,
    uint64_t                     now_us)
{
    const LeapControllerPeerEntry* peer;
    const uint8_t*                 peer_mac;

    if (ctx == NULL || out == NULL)
    {
        return;
    }

    peer     = leap_conf_win_target_peer(ctx);
    peer_mac = leap_conf_win_target_mac(ctx);

    out->device_state = ctx->stack.mgmt.peer_device_state;
    if (out->device_state == 0u && peer != NULL)
    {
        out->device_state = peer->device_state;
    }

    if (leap_mgmt_controller_get_state(&ctx->stack.mgmt) == LEAP_MGMT_CTRL_OP)
    {
        memcpy(out->session_owner_mac, ctx->stack.config.mgmt.controller_mac, 6);
        out->has_session_owner = 1;
    }
    else if (peer != NULL && !leap_conf_mac_is_zero(peer->active_owner_mac))
    {
        memcpy(out->session_owner_mac, peer->active_owner_mac, 6);
        out->has_session_owner = 1;
    }
    else
    {
        (void)peer_mac;
    }

    if (out->has_lease_watchdog == 0)
    {
        leap_conf_win_estimate_lease_watchdog(&ctx->stack, out, now_us);
    }
}

static int leap_conf_win_snapshot(void* user_ctx, LeapConformanceMetrics* out)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;
    uint64_t                   now_us;

    if (ctx == NULL || out == NULL)
    {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    leap_raw_winpcap_get_stats(&ctx->transport, &out->transport);
    (void)leap_raw_winpcap_query_link(&ctx->transport, &out->link);
    leap_conf_win_sync_session_pd(ctx);
    if (ctx->stack.pd.stats.cycles_completed > 0u)
    {
        out->pd = ctx->stack.pd.stats;
    }
    else if (ctx->session_pd.cycles_completed > 0u)
    {
        out->pd = ctx->session_pd;
    }
    else
    {
        out->pd = ctx->stack.pd.stats;
    }
    leap_conf_win_sync_session_latency(ctx);
    leap_conf_win_sync_session_network_rtt(ctx);
    leap_pd_latency_history_export(
        &ctx->session_latency,
        out->reply_latency_trend.samples,
        LEAP_PD_LATENCY_HISTORY_MAX,
        &out->reply_latency_trend.count,
        &out->reply_latency_trend.base_exchange);
    if (ctx->soak_rtt_count > 0u)
    {
        leap_conf_win_export_downsampled_trend(
            ctx->soak_rtt_samples,
            ctx->soak_rtt_count,
            &out->network_rtt_trend);
    }
    else
    {
        leap_pd_latency_history_export(
            &ctx->session_network_rtt,
            out->network_rtt_trend.samples,
            LEAP_PD_LATENCY_HISTORY_MAX,
            &out->network_rtt_trend.count,
            &out->network_rtt_trend.base_exchange);
    }
    out->frame_seq   = ctx->stack.frame_seq;
    out->stack_phase = (uint32_t)leap_controller_stack_get_phase(&ctx->stack);

    now_us = leap_raw_winpcap_monotonic_us();
    leap_conf_win_fill_session_fields(ctx, out, now_us);
    leap_conf_win_poll_device_diag(ctx);
    leap_conf_win_apply_device_diag(ctx, out);

    if (out->frames_from_device == 0)
    {
        out->rx_frames         = out->transport.rx_frames_ok;
        out->tx_frames         = out->transport.tx_frames_ok;
        out->duplicate_frames  = out->frame_seq.duplicate_frames;
        out->stale_frames      = out->pd.reply_stale_rejects;
    }

    return 0;
}

void leap_conformance_win_invalidate_diag_cache(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->cached_diag_valid = 0;
    ctx->force_diag_poll   = 1;
}

int leap_conformance_win_transport_is_open(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return 0;
    }

    return ctx->transport_open;
}

int leap_conformance_win_prepare_io_session(LeapConformanceWinContext* ctx)
{
    unsigned peer_count = 0u;
    uint16_t bootstrap_outputs;
    uint16_t soak_outputs;

    if (ctx == NULL || !ctx->transport_open || !ctx->has_peer_mac)
    {
        return -1;
    }

    if (leap_conformance_win_session_is_op(ctx) != 0)
    {
        soak_outputs = leap_conf_win_soak_outputs(ctx, 0u);
        if (leap_conf_win_ensure_exchange_session(ctx, 0) != 0)
        {
            return -1;
        }
        return leap_conf_win_configure_soak_outputs(ctx, soak_outputs);
    }

    if (ctx->table.count == 0u)
    {
        if (leap_conf_win_discover(ctx, 0, &peer_count) != 0 ||
            peer_count == 0u)
        {
            return -1;
        }
    }

    if (!ctx->session_caps_valid)
    {
        LeapConformanceDeviceCaps caps;

        memset(&caps, 0, sizeof(caps));
        if (leap_conf_win_probe_capabilities(ctx, ctx->peer_mac, &caps) == 0 &&
            caps.valid)
        {
            ctx->session_caps       = caps;
            ctx->session_caps_valid = 1;
        }
    }

    bootstrap_outputs = leap_conf_win_bootstrap_outputs(ctx);
    soak_outputs      = leap_conf_win_soak_outputs(ctx, 0u);

    if (leap_conf_win_ensure_exchange_session(ctx, 0) != 0)
    {
        return -1;
    }

    return leap_conf_win_configure_soak_outputs(ctx, soak_outputs);
}

int leap_conformance_win_io_session_prepared(const LeapConformanceWinContext* ctx)
{
    return (ctx != NULL && ctx->exchange_session_ready != 0) ? 1 : 0;
}

int leap_conformance_win_prepare_diagnostics(LeapConformanceWinContext* ctx)
{
    unsigned peer_count = 0u;

    if (ctx == NULL || !ctx->transport_open)
    {
        return -1;
    }

    if (leap_conformance_win_session_is_op(ctx) != 0)
    {
        return 0;
    }

    if (ctx->table.count == 0u)
    {
        if (leap_conf_win_discover(ctx, 0, &peer_count) != 0 ||
            peer_count == 0u)
        {
            return -1;
        }
    }

    if (leap_conformance_win_ensure_op(ctx, 0x0001u) != 0)
    {
        return -1;
    }

    leap_conformance_win_invalidate_diag_cache(ctx);
    return 0;
}

static void leap_conf_win_cancel(void* user_ctx)
{
    LeapConformanceWinContext* ctx = (LeapConformanceWinContext*)user_ctx;

    if (ctx != NULL)
    {
        ctx->cancel_flag = 1;
    }
}

LeapConformanceWinContext* leap_conformance_win_create(void)
{
    LeapConformanceWinContext* ctx =
        (LeapConformanceWinContext*)calloc(1, sizeof(*ctx));

    if (ctx == NULL)
    {
        return NULL;
    }

    ctx->bootstrap_retries = 3u;
    ctx->retry_delay_ms    = 1000u;

    memset(&ctx->io, 0, sizeof(ctx->io));
    ctx->io.user_ctx         = ctx;
    ctx->io.open_transport   = leap_conf_win_open;
    ctx->io.close_transport  = leap_conf_win_close;
    ctx->io.discover_peers   = leap_conf_win_discover;
    ctx->io.find_peer_mac    = leap_conf_win_find_peer;
    ctx->io.probe_capabilities = leap_conf_win_probe_capabilities;
    ctx->io.bootstrap        = leap_conf_win_bootstrap;
    ctx->io.pd_write         = leap_conf_win_pd_write;
    ctx->io.read_diag        = leap_conf_win_read_diag;
    ctx->io.lease_demo       = leap_conf_win_lease_demo;
    ctx->io.cyclic_pd        = leap_conf_win_cyclic;
    ctx->io.identify         = leap_conf_win_identify;
    ctx->io.locate           = leap_conf_win_locate;
    ctx->io.snapshot         = leap_conf_win_snapshot;
    ctx->io.cancel           = leap_conf_win_cancel;

    return ctx;
}

void leap_conformance_win_destroy(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    leap_conf_win_close(ctx);
    leap_conf_win_free_soak_rtt_capture(ctx);
    free(ctx);
}

void leap_conformance_win_set_retries(
    LeapConformanceWinContext* ctx,
    unsigned                   bootstrap_retries,
    unsigned                   retry_delay_ms)
{
    if (ctx == NULL)
    {
        return;
    }

    if (bootstrap_retries > 0u)
    {
        ctx->bootstrap_retries = bootstrap_retries;
    }
    if (retry_delay_ms > 0u)
    {
        ctx->retry_delay_ms = retry_delay_ms;
    }
}

void leap_conformance_win_set_progress(
    LeapConformanceWinContext* ctx,
    LeapConformanceProgressFn    progress_fn,
    void*                      progress_ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->progress_fn  = progress_fn;
    ctx->progress_ctx = progress_ctx;
}

void leap_conformance_win_set_peer_mac(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    int                        has_peer_mac)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->has_peer_mac = has_peer_mac;
    if (has_peer_mac && peer_mac != NULL)
    {
        memcpy(ctx->peer_mac, peer_mac, 6);
    }
}

const LeapConformanceIo* leap_conformance_win_io(LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return &ctx->io;
}

const LeapControllerPeerTable* leap_conformance_win_peer_table(
    LeapConformanceWinContext* ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return &ctx->table;
}
