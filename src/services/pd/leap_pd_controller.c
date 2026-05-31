/*
 * leap_pd_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_pd_controller.h"

#include "leap/leap_pd_common.h"

#include <stdio.h>
#include <string.h>

#if defined(__linux__)
#include <unistd.h>
#endif

#define LEAP_PD_CTRL_DEFAULT_PERIOD_MS 100u
#define LEAP_PD_CTRL_DEFAULT_STATS_LOG 100u
#define LEAP_PD_CTRL_DEFAULT_HB_CYCLES 10u
#define LEAP_PD_CTRL_RX_BUF            256u

static void leap_pd_ctrl_update_latency(
    LeapPdControllerContext* ctx,
    uint64_t                 start_us,
    uint64_t                 end_us)
{
    uint64_t latency;

    if (ctx == NULL || end_us <= start_us)
    {
        return;
    }

    latency = end_us - start_us;
    ctx->stats.last_latency_us = latency;
    ctx->stats.total_latency_us += latency;
    if (latency > ctx->stats.max_latency_us)
    {
        ctx->stats.max_latency_us = latency;
    }
}

static int leap_pd_ctrl_maintain_lease(
    LeapPdControllerContext*   pd,
    LeapMgmtControllerContext* mgmt,
    const LeapPdControllerIo*  io,
    const uint8_t*               peer_mac,
    uint64_t                     now_us)
{
    uint32_t session_id;
    uint32_t sequence;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL)
    {
        return -1;
    }

    if (leap_mgmt_controller_should_send_heartbeat(mgmt, now_us) == 0)
    {
        return 0;
    }

    session_id = leap_mgmt_controller_session_id(mgmt);
    sequence   = leap_mgmt_controller_next_sequence(mgmt);

    if (io->send_heartbeat(io->user_ctx, peer_mac, session_id, sequence) != 0)
    {
        return -1;
    }

    leap_mgmt_controller_on_heartbeat_sent(mgmt, now_us);
    pd->stats.heartbeats_sent++;
    return 0;
}

void leap_pd_controller_init(
    LeapPdControllerContext*       ctx,
    const LeapPdControllerConfig* config)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    if (config != NULL)
    {
        ctx->config = *config;
    }

    if (ctx->config.cycle_period_ms == 0u)
    {
        ctx->config.cycle_period_ms = LEAP_PD_CTRL_DEFAULT_PERIOD_MS;
    }

    if (ctx->config.stats_log_interval == 0u)
    {
        ctx->config.stats_log_interval = LEAP_PD_CTRL_DEFAULT_STATS_LOG;
    }

    if (ctx->config.heartbeat_every_n_cycles == 0u)
    {
        ctx->config.heartbeat_every_n_cycles = LEAP_PD_CTRL_DEFAULT_HB_CYCLES;
    }

    if (ctx->config.profile_id == 0u)
    {
        ctx->config.profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    }

    ctx->pd_sequence = 1000u;
}

void leap_pd_controller_reset_stats(LeapPdControllerContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(&ctx->stats, 0, sizeof(ctx->stats));
}

const LeapPdControllerStats* leap_pd_controller_stats(
    const LeapPdControllerContext* ctx)
{
    if (ctx == NULL)
    {
        return NULL;
    }

    return &ctx->stats;
}

void leap_pd_controller_log_stats(const LeapPdControllerContext* ctx)
{
    uint64_t avg_latency = 0u;

    if (ctx == NULL)
    {
        return;
    }

    if (ctx->stats.cycles_completed > 0u)
    {
        avg_latency = ctx->stats.total_latency_us / ctx->stats.cycles_completed;
    }

    printf(
        "PD stats: cycles=%llu ok=%llu fail=%llu hb=%llu timeouts=%llu "
        "latency last=%llu avg=%llu max=%llu us inputs=0x%04X\n",
        (unsigned long long)ctx->stats.cycles_completed,
        (unsigned long long)ctx->stats.pd_sent_ok,
        (unsigned long long)ctx->stats.pd_sent_fail,
        (unsigned long long)ctx->stats.heartbeats_sent,
        (unsigned long long)ctx->stats.recv_timeouts,
        (unsigned long long)ctx->stats.last_latency_us,
        (unsigned long long)avg_latency,
        (unsigned long long)ctx->stats.max_latency_us,
        ctx->stats.last_digital_inputs);
}

int leap_pd_controller_send_single_write(
    LeapPdControllerContext*   pd,
    LeapMgmtControllerContext* mgmt,
    const LeapPdControllerIo*  io,
    const uint8_t*               peer_mac,
    uint16_t                     digital_outputs)
{
    LeapPdBuildParams params;
    uint8_t           payload[LEAP_PD_CTRL_RX_BUF];
    size_t            payload_length;
    uint64_t          now_us;
    uint32_t          session_id;
    uint32_t          sequence;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL)
    {
        return -1;
    }

    now_us = (io->monotonic_us != NULL) ? io->monotonic_us(io->user_ctx) : 0u;

    memset(&params, 0, sizeof(params));
    params.profile_id       = pd->config.profile_id;
    params.process_sequence = pd->pd_sequence++;
    params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;

    payload_length = leap_pd_build_digital_write(
        payload,
        sizeof(payload),
        &params,
        digital_outputs);
    if (payload_length == 0u)
    {
        return -1;
    }

    session_id = leap_mgmt_controller_session_id(mgmt);
    sequence   = leap_mgmt_controller_next_sequence(mgmt);

    if (io->send_pd(
            io->user_ctx,
            peer_mac,
            LEAP_PD_WRITE_ENDPOINT,
            payload,
            payload_length,
            session_id,
            sequence) != 0)
    {
        pd->stats.pd_sent_fail++;
        return -1;
    }

    leap_mgmt_controller_on_pd_sent(mgmt, params.process_sequence, now_us);
    pd->stats.pd_sent_ok++;
    pd->stats.cycles_completed++;
    return 0;
}

int leap_pd_controller_run_cyclic(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext*   mgmt,
    const LeapPdControllerIo*    io,
    const uint8_t*               peer_mac,
    volatile int*                stop_flag)
{
    uint8_t  payload[LEAP_PD_CTRL_RX_BUF];
    uint8_t  reply[LEAP_PD_CTRL_RX_BUF];
    size_t   payload_length;
    size_t   reply_length;
    uint16_t outputs;
    uint64_t cycle_start_us;
    uint64_t now_us;
    uint32_t session_id;
    uint32_t sequence;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL ||
        stop_flag == NULL)
    {
        return -1;
    }

    printf(
        "cyclic PD (%u ms%s) — Ctrl+C to stop\n",
        pd->config.cycle_period_ms,
        pd->config.use_exchange != 0 ? ", exchange" : "");

    session_id = leap_mgmt_controller_session_id(mgmt);

    while (*stop_flag == 0)
    {
        cycle_start_us = (io->monotonic_us != NULL) ? io->monotonic_us(io->user_ctx) : 0u;
        outputs        = (uint16_t)(0x0001u << (pd->cycle_index % 5u));

        if (pd->config.use_exchange != 0)
        {
            payload_length = leap_pd_build_digital_exchange(
                payload,
                sizeof(payload),
                pd->pd_sequence++,
                pd->config.cycle_period_ms * 1000u,
                pd->config.profile_id,
                outputs);

            if (payload_length == 0u)
            {
                pd->stats.pd_sent_fail++;
                return -1;
            }

            sequence = leap_mgmt_controller_next_sequence(mgmt);
            if (io->send_pd(
                    io->user_ctx,
                    peer_mac,
                    LEAP_PD_EXCHANGE_ENDPOINTS,
                    payload,
                    payload_length,
                    session_id,
                    sequence) != 0)
            {
                pd->stats.pd_sent_fail++;
                return -1;
            }

            if (io->wait_exchange_reply != NULL)
            {
                reply_length = 0u;
                if (io->wait_exchange_reply(
                        io->user_ctx,
                        peer_mac,
                        reply,
                        sizeof(reply),
                        &reply_length,
                        500) != 0)
                {
                    pd->stats.recv_timeouts++;
                }
                else if (reply_length >=
                         sizeof(LeapExchangeHeader) + sizeof(LeapProfileDigital16x16))
                {
                    const uint8_t* read_data =
                        reply + sizeof(LeapExchangeHeader) +
                        sizeof(LeapProfileDigital16x16);
                    const LeapProfileDigital16x16* inputs =
                        (const LeapProfileDigital16x16*)read_data;

                    pd->stats.exchange_replies++;
                    pd->stats.last_digital_inputs = inputs->digital_inputs;
                }
            }
        }
        else
        {
            LeapPdBuildParams params;

            memset(&params, 0, sizeof(params));
            params.profile_id       = pd->config.profile_id;
            params.process_sequence = pd->pd_sequence++;
            params.cycle_time_us    = pd->config.cycle_period_ms * 1000u;
            params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;

            payload_length = leap_pd_build_digital_write(
                payload,
                sizeof(payload),
                &params,
                outputs);
            if (payload_length == 0u)
            {
                pd->stats.pd_sent_fail++;
                return -1;
            }

            sequence = leap_mgmt_controller_next_sequence(mgmt);
            if (io->send_pd(
                    io->user_ctx,
                    peer_mac,
                    LEAP_PD_WRITE_ENDPOINT,
                    payload,
                    payload_length,
                    session_id,
                    sequence) != 0)
            {
                pd->stats.pd_sent_fail++;
                return -1;
            }

            leap_mgmt_controller_on_pd_sent(mgmt, params.process_sequence, cycle_start_us);
        }

        pd->stats.pd_sent_ok++;
        pd->stats.cycles_completed++;

        now_us = (io->monotonic_us != NULL) ? io->monotonic_us(io->user_ctx) : cycle_start_us;
        leap_pd_ctrl_update_latency(pd, cycle_start_us, now_us);

        if ((pd->cycle_index % 20u) == 0u)
        {
            printf(
                "cyclic PD seq=%u outputs=0x%04X latency=%llu us\n",
                pd->pd_sequence - 1u,
                outputs,
                (unsigned long long)pd->stats.last_latency_us);
        }

        pd->cycle_index++;

        if ((pd->cycle_index % pd->config.heartbeat_every_n_cycles) == 0u ||
            leap_mgmt_controller_should_send_heartbeat(mgmt, now_us) != 0)
        {
            (void)leap_pd_ctrl_maintain_lease(pd, mgmt, io, peer_mac, now_us);
        }

        if (pd->config.stats_log_interval > 0u &&
            (pd->cycle_index % pd->config.stats_log_interval) == 0u)
        {
            leap_pd_controller_log_stats(pd);
        }

#if defined(__linux__)
        {
            unsigned int sleep_us = pd->config.cycle_period_ms * 1000u;
            if (sleep_us > 0u)
            {
                usleep(sleep_us);
            }
        }
#endif
    }

    printf("cyclic PD stopped\n");
    leap_pd_controller_log_stats(pd);
    return 0;
}
