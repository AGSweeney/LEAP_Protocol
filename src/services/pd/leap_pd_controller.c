/*
 * leap_pd_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_pd_controller.h"

#include "leap/leap_pd_common.h"

#include "leap/leap_log.h"

#if defined(__linux__)
#include <unistd.h>
#endif

#if defined(_WIN32)
#include "leap/leap_win_time.h"
#endif

#define LEAP_PD_CTRL_DEFAULT_PERIOD_MS 100u
#define LEAP_PD_CTRL_DEFAULT_STATS_LOG 100u
#define LEAP_PD_CTRL_DEFAULT_HB_CYCLES 10u
#define LEAP_PD_CTRL_RX_BUF            256u

static void leap_pd_ctrl_sleep_us(uint64_t sleep_us)
{
    if (sleep_us == 0u)
    {
        return;
    }

#if defined(__linux__)
    usleep((useconds_t)sleep_us);
#elif defined(_WIN32)
    leap_win_sleep_us(sleep_us);
#endif
}

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

static void leap_pd_ctrl_update_cycle_period(
    LeapPdControllerContext* ctx,
    uint64_t                 period_us)
{
    uint64_t target_us;
    uint64_t jitter_us;

    if (ctx == NULL || period_us == 0u)
    {
        return;
    }

    ctx->stats.last_cycle_period_us = period_us;
    ctx->stats.total_cycle_period_us += period_us;

    if (ctx->stats.min_cycle_period_us == 0u || period_us < ctx->stats.min_cycle_period_us)
    {
        ctx->stats.min_cycle_period_us = period_us;
    }

    if (period_us > ctx->stats.max_cycle_period_us)
    {
        ctx->stats.max_cycle_period_us = period_us;
    }

    target_us = (uint64_t)ctx->config.cycle_period_ms * 1000u;
    if (target_us > 0u)
    {
        jitter_us = (period_us > target_us) ? (period_us - target_us)
                                          : (target_us - period_us);
        ctx->stats.last_cycle_jitter_us = jitter_us;
        ctx->stats.total_cycle_jitter_us += jitter_us;
        if (jitter_us > ctx->stats.max_cycle_jitter_us)
        {
            ctx->stats.max_cycle_jitter_us = jitter_us;
        }
    }
}

static void leap_pd_ctrl_update_work_time(
    LeapPdControllerContext* ctx,
    uint64_t                 work_us,
    uint64_t                 period_us)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->stats.last_cycle_work_us = work_us;
    if (work_us > ctx->stats.max_cycle_work_us)
    {
        ctx->stats.max_cycle_work_us = work_us;
    }

    if (period_us > 0u && work_us > period_us)
    {
        ctx->stats.cycle_overruns++;
    }
}

static void leap_pd_ctrl_record_cycle_timing(
    LeapPdControllerContext* ctx,
    uint64_t                 cycle_start_us,
    uint64_t                 cycle_end_us)
{
    uint64_t period_us;
    uint64_t work_us;

    if (ctx == NULL)
    {
        return;
    }

    period_us = ctx->config.cycle_period_ms * 1000u;
    work_us   = (cycle_end_us > cycle_start_us) ? (cycle_end_us - cycle_start_us) : 0u;

    leap_pd_ctrl_update_work_time(ctx, work_us, period_us);
    leap_pd_ctrl_update_latency(ctx, cycle_start_us, cycle_end_us);

    if (ctx->cycle_timing_active != 0 &&
        ctx->last_cycle_start_us > 0u &&
        cycle_start_us > ctx->last_cycle_start_us)
    {
        leap_pd_ctrl_update_cycle_period(
            ctx,
            cycle_start_us - ctx->last_cycle_start_us);
    }

    ctx->last_cycle_start_us = cycle_start_us;
    ctx->cycle_timing_active = 1;
}

static LeapPdControllerStatus leap_pd_ctrl_maintain_lease(
    LeapPdControllerContext*   pd,
    LeapMgmtControllerContext* mgmt,
    const LeapPdControllerIo*  io,
    const uint8_t*             peer_mac,
    uint64_t                   now_us,
    int                        force_send)
{
    uint32_t session_id;
    uint32_t sequence;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (io->send_heartbeat == NULL)
    {
        return LEAP_PD_CTRL_IO_MISSING;
    }

    if (force_send == 0 &&
        leap_mgmt_controller_should_send_heartbeat(mgmt, now_us) == 0)
    {
        return LEAP_PD_CTRL_OK;
    }

    session_id = leap_mgmt_controller_session_id(mgmt);
    sequence   = leap_mgmt_controller_next_sequence(mgmt);

    {
        uint8_t hb_payload[sizeof(LeapHeartbeatPayload)];
        size_t  hb_length;

        hb_length = leap_mgmt_controller_build_heartbeat(
            mgmt,
            hb_payload,
            sizeof(hb_payload));
        if (hb_length == 0u)
        {
            return LEAP_PD_CTRL_HEARTBEAT_FAILED;
        }

        if (io->send_heartbeat(
                io->user_ctx,
                peer_mac,
                hb_payload,
                hb_length,
                session_id,
                sequence) != 0)
        {
            return LEAP_PD_CTRL_HEARTBEAT_FAILED;
        }
    }

    leap_mgmt_controller_on_heartbeat_sent(mgmt, now_us);
    pd->stats.heartbeats_sent++;
    return LEAP_PD_CTRL_OK;
}

static const LeapPdProfileMap* leap_pd_ctrl_profile(
    const LeapPdControllerContext* ctx)
{
    static LeapPdProfileMap k_default;

    if (ctx != NULL && ctx->config.profile.valid != 0)
    {
        return &ctx->config.profile;
    }

    leap_pd_profile_map_init_default(&k_default);
    return &k_default;
}

static uint32_t leap_pd_ctrl_max_frame_age_us(
    const LeapPdControllerContext* ctx)
{
    if (ctx == NULL)
    {
        return 0u;
    }

    if (ctx->config.max_frame_age_us != 0u)
    {
        return ctx->config.max_frame_age_us;
    }

    return ctx->config.cycle_period_ms * 2000u;
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

    if (ctx->config.profile.valid == 0)
    {
        if (ctx->config.profile_id != 0u)
        {
            (void)leap_pd_profile_map_from_profile_id(
                ctx->config.profile_id,
                &ctx->config.profile);
        }
        else
        {
            leap_pd_profile_map_init_default(&ctx->config.profile);
        }
    }

    if (config == NULL)
    {
        ctx->config.validate_exchange_reply  = 1;
        ctx->config.enforce_reply_frame_age  = 1;
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
    ctx->last_cycle_start_us = 0u;
    ctx->cycle_timing_active = 0;
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
    uint64_t avg_period  = 0u;
    uint64_t avg_jitter  = 0u;

    if (ctx == NULL)
    {
        return;
    }

    if (ctx->stats.cycles_completed > 0u)
    {
        avg_latency = ctx->stats.total_latency_us / ctx->stats.cycles_completed;
        if (ctx->stats.total_cycle_period_us > 0u)
        {
            avg_period = ctx->stats.total_cycle_period_us / ctx->stats.cycles_completed;
        }
        if (ctx->stats.total_cycle_jitter_us > 0u)
        {
            avg_jitter = ctx->stats.total_cycle_jitter_us / ctx->stats.cycles_completed;
        }
    }

    leap_log_printf(
        "PD stats: cycles=%llu ok=%llu fail=%llu hb=%llu lost=%llu timeouts=%llu "
        "replies=%llu reject=%llu stale=%llu seq_mismatch=%llu "
        "latency last=%llu avg=%llu max=%llu us "
        "period last=%llu avg=%llu min=%llu max=%llu us "
        "jitter last=%llu avg=%llu max=%llu us target=%u ms "
        "work last=%llu max=%llu overruns=%llu inputs=0x%04X\n",
        (unsigned long long)ctx->stats.cycles_completed,
        (unsigned long long)ctx->stats.pd_sent_ok,
        (unsigned long long)ctx->stats.pd_sent_fail,
        (unsigned long long)ctx->stats.heartbeats_sent,
        (unsigned long long)ctx->stats.lost_frames,
        (unsigned long long)ctx->stats.recv_timeouts,
        (unsigned long long)ctx->stats.exchange_replies,
        (unsigned long long)ctx->stats.reply_rejects,
        (unsigned long long)ctx->stats.reply_stale_rejects,
        (unsigned long long)ctx->stats.reply_sequence_mismatches,
        (unsigned long long)ctx->stats.last_latency_us,
        (unsigned long long)avg_latency,
        (unsigned long long)ctx->stats.max_latency_us,
        (unsigned long long)ctx->stats.last_cycle_period_us,
        (unsigned long long)avg_period,
        (unsigned long long)ctx->stats.min_cycle_period_us,
        (unsigned long long)ctx->stats.max_cycle_period_us,
        (unsigned long long)ctx->stats.last_cycle_jitter_us,
        (unsigned long long)avg_jitter,
        (unsigned long long)ctx->stats.max_cycle_jitter_us,
        ctx->config.cycle_period_ms,
        (unsigned long long)ctx->stats.last_cycle_work_us,
        (unsigned long long)ctx->stats.max_cycle_work_us,
        (unsigned long long)ctx->stats.cycle_overruns,
        ctx->stats.last_digital_inputs);
}

LeapPdControllerStatus leap_pd_controller_send_single_write(
    LeapPdControllerContext*   pd,
    LeapMgmtControllerContext* mgmt,
    const LeapPdControllerIo*  io,
    const uint8_t*             peer_mac,
    uint16_t                   digital_outputs)
{
    LeapPdBuildParams         params;
    const LeapPdProfileMap*   profile;
    uint8_t                   payload[LEAP_PD_CTRL_RX_BUF];
    size_t                    payload_length;
    uint64_t                  now_us;
    uint32_t                  session_id;
    uint32_t                  sequence;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (io->send_pd == NULL)
    {
        return LEAP_PD_CTRL_IO_MISSING;
    }

    now_us = (io->monotonic_us != NULL) ? io->monotonic_us(io->user_ctx) : 0u;
    profile = leap_pd_ctrl_profile(pd);

    memset(&params, 0, sizeof(params));
    params.profile_id       = profile->profile_id;
    params.endpoint_id      = profile->write_endpoint_id;
    params.process_sequence = pd->pd_sequence++;
    params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;

    payload_length = leap_pd_build_digital_write(
        payload,
        sizeof(payload),
        &params,
        digital_outputs);
    if (payload_length == 0u)
    {
        return LEAP_PD_CTRL_BUILD_FAILED;
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
        return LEAP_PD_CTRL_SEND_FAILED;
    }

    leap_mgmt_controller_on_pd_sent(mgmt, params.process_sequence, now_us);
    pd->stats.pd_sent_ok++;
    pd->stats.cycles_completed++;
    return LEAP_PD_CTRL_OK;
}

LeapPdControllerStatus leap_pd_controller_run_one_cycle(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext*   mgmt,
    const LeapPdControllerIo*    io,
    const uint8_t*               peer_mac,
    volatile int*                stop_flag,
    int                          sleep_for_period)
{
    uint8_t                 payload[LEAP_PD_CTRL_RX_BUF];
    uint8_t                 reply[LEAP_PD_CTRL_RX_BUF];
    size_t                  payload_length;
    size_t                  reply_length;
    size_t                  read_payload_size;
    uint16_t                outputs;
    uint64_t                cycle_start_us;
    uint64_t                now_us;
    uint64_t                period_us;
    uint32_t                session_id;
    uint32_t                sequence;
    const LeapPdProfileMap* profile;
    LeapPdControllerStatus  status;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL ||
        stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (*stop_flag != 0)
    {
        return LEAP_PD_CTRL_STOPPED;
    }

    if (io->send_pd == NULL)
    {
        return LEAP_PD_CTRL_IO_MISSING;
    }

    profile         = leap_pd_ctrl_profile(pd);
    read_payload_size = profile->endpoint_payload_size;
    if (read_payload_size == 0u)
    {
        read_payload_size = sizeof(LeapProfileDigital16x16);
    }

    cycle_start_us = (io->monotonic_us != NULL) ? io->monotonic_us(io->user_ctx) : 0u;
    outputs        = (uint16_t)(0x0001u << (pd->cycle_index % 6u));
    session_id     = leap_mgmt_controller_session_id(mgmt);
    period_us      = (uint64_t)pd->config.cycle_period_ms * 1000u;

    if (pd->config.use_exchange != 0)
    {
        uint32_t sent_process_sequence;
        uint32_t max_frame_age_us;
        uint64_t recv_now_us;

        max_frame_age_us = leap_pd_ctrl_max_frame_age_us(pd);

        payload_length = leap_pd_build_digital_exchange_mapped(
            payload,
            sizeof(payload),
            pd->pd_sequence++,
            pd->config.cycle_period_ms * 1000u,
            profile,
            outputs,
            cycle_start_us,
            (pd->config.enforce_reply_frame_age != 0) ? max_frame_age_us : 0u);

        if (payload_length == 0u)
        {
            pd->stats.pd_sent_fail++;
            return LEAP_PD_CTRL_BUILD_FAILED;
        }

        sent_process_sequence = pd->pd_sequence - 1u;

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
            return LEAP_PD_CTRL_SEND_FAILED;
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
                pd->stats.lost_frames++;
            }
            else
            {
                LeapPdExchangeView   reply_view;
                LeapExchangeStatus   reply_status;
                LeapPdCommonStatus   validate_status;
                const uint8_t*       read_data;
                const LeapProfileDigital16x16* inputs;

                if (pd->config.validate_exchange_reply != 0)
                {
                    recv_now_us = (io->monotonic_us != NULL)
                                      ? io->monotonic_us(io->user_ctx)
                                      : 0u;
                    if (pd->config.enforce_reply_frame_age != 0 && recv_now_us == 0u)
                    {
                        recv_now_us = cycle_start_us;
                    }

                    validate_status = leap_pd_validate_exchange_reply_at(
                        reply,
                        reply_length,
                        profile,
                        sent_process_sequence,
                        (pd->config.enforce_reply_frame_age != 0) ? recv_now_us
                                                                    : 0u,
                        pd->config.reply_jitter_margin_us,
                        &reply_view,
                        &reply_status);
                }
                else
                {
                    validate_status = leap_pd_exchange_view(
                        reply,
                        reply_length,
                        &reply_view);
                    if (validate_status == LEAP_PD_COMMON_OK)
                    {
                        memset(&reply_status, 0, sizeof(reply_status));
                    }
                }

                if (validate_status == LEAP_PD_COMMON_SEQUENCE_MISMATCH)
                {
                    pd->stats.reply_sequence_mismatches++;
                    pd->stats.reply_rejects++;
                }
                else if (validate_status == LEAP_PD_COMMON_STALE_FRAME)
                {
                    pd->stats.reply_stale_rejects++;
                    pd->stats.reply_rejects++;
                }
                else if (validate_status != LEAP_PD_COMMON_OK)
                {
                    pd->stats.reply_rejects++;
                }
                else if (reply_length >=
                         sizeof(LeapExchangeHeader) + read_payload_size +
                             read_payload_size)
                {
                    read_data = reply + sizeof(LeapExchangeHeader) +
                                read_payload_size;
                    inputs =
                        (const LeapProfileDigital16x16*)read_data;

                    pd->stats.exchange_replies++;
                    pd->stats.last_digital_inputs = inputs->digital_inputs;
                }
            }
        }
    }
    else
    {
        LeapPdBuildParams params;

        memset(&params, 0, sizeof(params));
        params.profile_id       = profile->profile_id;
        params.endpoint_id      = profile->write_endpoint_id;
        params.process_sequence = pd->pd_sequence++;
        params.cycle_time_us    = pd->config.cycle_period_ms * 1000u;
        params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;
        if (pd->config.enforce_reply_frame_age != 0)
        {
            params.controller_timestamp_us = cycle_start_us;
            params.max_frame_age_us          = leap_pd_ctrl_max_frame_age_us(pd);
        }

        payload_length = leap_pd_build_digital_write(
            payload,
            sizeof(payload),
            &params,
            outputs);
        if (payload_length == 0u)
        {
            pd->stats.pd_sent_fail++;
            return LEAP_PD_CTRL_BUILD_FAILED;
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
            return LEAP_PD_CTRL_SEND_FAILED;
        }

        leap_mgmt_controller_on_pd_sent(mgmt, params.process_sequence, cycle_start_us);
    }

    pd->stats.pd_sent_ok++;
    pd->stats.cycles_completed++;

    now_us = (io->monotonic_us != NULL) ? io->monotonic_us(io->user_ctx) : cycle_start_us;
    leap_pd_ctrl_record_cycle_timing(pd, cycle_start_us, now_us);

    pd->cycle_index++;

    if ((pd->cycle_index % pd->config.heartbeat_every_n_cycles) == 0u)
    {
        status = leap_pd_ctrl_maintain_lease(
            pd, mgmt, io, peer_mac, now_us, 1);
    }
    else if (leap_mgmt_controller_should_send_heartbeat(mgmt, now_us) != 0)
    {
        status = leap_pd_ctrl_maintain_lease(
            pd, mgmt, io, peer_mac, now_us, 0);
    }
    else
    {
        status = LEAP_PD_CTRL_OK;
    }

    if (status != LEAP_PD_CTRL_OK && status != LEAP_PD_CTRL_IO_MISSING)
    {
        return status;
    }

    if (pd->config.stats_log_interval > 0u &&
        (pd->cycle_index % pd->config.stats_log_interval) == 0u)
    {
        leap_pd_controller_log_stats(pd);
    }

#if defined(__linux__) || defined(_WIN32)
    if (sleep_for_period != 0 && period_us > 0u)
    {
        uint64_t work_us = (now_us > cycle_start_us) ? (now_us - cycle_start_us) : 0u;
        uint64_t sleep_us;

        if (work_us >= period_us)
        {
            sleep_us = 0u;
        }
        else
        {
            sleep_us = period_us - work_us;
        }

        leap_pd_ctrl_sleep_us(sleep_us);
    }
#else
    (void)sleep_for_period;
    (void)period_us;
#endif

    return LEAP_PD_CTRL_OK;
}

LeapPdControllerStatus leap_pd_controller_run_cyclic(
    LeapPdControllerContext*     pd,
    LeapMgmtControllerContext*   mgmt,
    const LeapPdControllerIo*    io,
    const uint8_t*               peer_mac,
    volatile int*                stop_flag)
{
    LeapPdControllerStatus status;

    if (pd == NULL || mgmt == NULL || io == NULL || peer_mac == NULL ||
        stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    leap_log_printf(
        "cyclic PD (%u ms%s) - Ctrl+C to stop\n",
        pd->config.cycle_period_ms,
        pd->config.use_exchange != 0 ? ", exchange" : "");

    while (*stop_flag == 0)
    {
        status = leap_pd_controller_run_one_cycle(
            pd,
            mgmt,
            io,
            peer_mac,
            stop_flag,
            1);
        if (status == LEAP_PD_CTRL_STOPPED)
        {
            break;
        }
        if (status != LEAP_PD_CTRL_OK)
        {
            return status;
        }
    }

    leap_log_printf("cyclic PD stopped\n");
    leap_pd_controller_log_stats(pd);
    return LEAP_PD_CTRL_OK;
}
