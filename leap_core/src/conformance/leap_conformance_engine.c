/*
 * leap_conformance_engine.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "leap/conformance/leap_conformance.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

static uint64_t leap_conf_now_ms(void)
{
#if defined(_WIN32)
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

const char* leap_conformance_step_status_text(LeapConformanceStepStatus status)
{
    switch (status)
    {
    case LEAP_CONF_STEP_PASS:
        return "PASS";
    case LEAP_CONF_STEP_FAIL:
        return "FAIL";
    case LEAP_CONF_STEP_SKIP:
        return "SKIP";
    case LEAP_CONF_STEP_RUNNING:
        return "RUNNING";
    default:
        return "PENDING";
    }
}

static void leap_conf_emit_progress(
    const LeapConformanceRunConfig* config,
    LeapConformanceProgressPhase    phase,
    const char*                     step_id,
    const char*                     step_name,
    unsigned                        percent,
    uint32_t                        elapsed_ms,
    const LeapConformanceMetrics*   metrics)
{
    LeapConformanceProgress progress;

    if (config == NULL || config->progress_fn == NULL)
    {
        return;
    }

    memset(&progress, 0, sizeof(progress));
    progress.phase      = phase;
    progress.step_id    = step_id;
    progress.step_name  = step_name;
    progress.percent    = percent;
    progress.elapsed_ms = elapsed_ms;
    progress.metrics    = metrics;
    config->progress_fn(config->progress_ctx, &progress);
}

static int leap_conf_step_selected(
    const LeapConformanceRunConfig* config,
    const char*                     step_id)
{
    size_t i;

    if (config == NULL || step_id == NULL)
    {
        return 0;
    }

    if (config->step_filter == NULL || config->step_filter_count == 0u)
    {
        return 1;
    }

    for (i = 0u; i < config->step_filter_count; i++)
    {
        if (config->step_filter[i] != NULL &&
            strcmp(config->step_filter[i], step_id) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static LeapConformanceStepResult* leap_conf_add_step(
    LeapConformanceRunResult* result)
{
    if (result == NULL || result->step_count >= LEAP_CONF_MAX_STEPS)
    {
        return NULL;
    }

    return &result->steps[result->step_count++];
}

static void leap_conf_fill_step(
    LeapConformanceStepResult*      row,
    const LeapConformanceScenarioStep* def,
    const char*                     name,
    LeapConformanceStepStatus       status,
    uint32_t                        duration_ms,
    const char*                     detail)
{
    if (row == NULL || def == NULL)
    {
        return;
    }

    memset(row, 0, sizeof(*row));
    (void)snprintf(row->step_id, sizeof(row->step_id), "%s", def->id);
    (void)snprintf(row->phase, sizeof(row->phase), "%s", def->phase);
    (void)snprintf(row->name, sizeof(row->name), "%s", name != NULL ? name : def->name);
    row->status      = status;
    row->duration_ms = duration_ms;
    if (detail != NULL)
    {
        (void)snprintf(row->detail, sizeof(row->detail), "%s", detail);
    }
}

static void leap_conf_update_summary(LeapConformanceRunResult* result)
{
    size_t i;

    if (result == NULL)
    {
        return;
    }

    result->summary.passed  = 0u;
    result->summary.failed  = 0u;
    result->summary.skipped = 0u;

    for (i = 0u; i < result->step_count; i++)
    {
        switch (result->steps[i].status)
        {
        case LEAP_CONF_STEP_PASS:
            result->summary.passed++;
            break;
        case LEAP_CONF_STEP_FAIL:
            result->summary.failed++;
            break;
        case LEAP_CONF_STEP_SKIP:
            result->summary.skipped++;
            break;
        default:
            break;
        }
    }

    result->summary.total = result->summary.passed +
                            result->summary.failed +
                            result->summary.skipped;
}

typedef struct LeapConformanceRunState
{
    LeapConformanceDeviceCaps device_caps;
    int                       device_caps_valid;
    uint8_t                   resolved_peer_mac[6];
    int                       has_resolved_peer_mac;
} LeapConformanceRunState;

static int leap_conf_resolve_peer_mac(
    const LeapConformanceRunConfig* config,
    const LeapConformanceRunState*  run_state,
    const LeapConformanceIo*        io,
    uint8_t                         mac_out[6])
{
    if (config != NULL && config->has_peer_mac)
    {
        memcpy(mac_out, config->peer_mac, 6);
        return 1;
    }

    if (run_state != NULL && run_state->has_resolved_peer_mac)
    {
        memcpy(mac_out, run_state->resolved_peer_mac, 6);
        return 1;
    }

    if (io != NULL && io->get_session_peer_mac != NULL)
    {
        int has_session = 0;

        if (io->get_session_peer_mac(io->user_ctx, mac_out, &has_session) == 0 &&
            has_session)
        {
            return 1;
        }
    }

    return 0;
}

static uint16_t leap_conf_step_outputs(
    const LeapConformanceScenarioStep* def,
    const LeapConformanceRunState*       run_state,
    int                                  use_cyclic_outputs)
{
    if (run_state != NULL && run_state->device_caps_valid)
    {
        if (use_cyclic_outputs)
        {
            return run_state->device_caps.cyclic_outputs;
        }
        return run_state->device_caps.bootstrap_outputs;
    }

    if (def != NULL && def->pd_outputs != 0u)
    {
        return def->pd_outputs;
    }

    return use_cyclic_outputs ? (uint16_t)0x003Fu : (uint16_t)0x0001u;
}

static LeapConformanceStatus leap_conf_run_one(
    const LeapConformanceRunConfig*      config,
    LeapConformanceRunResult*            result,
    LeapConformanceRunState*             run_state,
    const LeapConformanceScenarioStep*   def,
    uint64_t                             run_start_ms,
    unsigned                             step_index,
    unsigned                             step_total)
{
    LeapConformanceStepResult* row;
    uint64_t                   step_start;
    unsigned                   percent;
    int                        ok;
    int                        sub;
    char                       detail[LEAP_CONF_DETAIL_MAX];
    const LeapConformanceIo*   io;

    if (config == NULL || result == NULL || def == NULL)
    {
        return LEAP_CONF_INVALID_ARG;
    }

    io = config->io;
    if (io == NULL)
    {
        return LEAP_CONF_INVALID_ARG;
    }

    if (!leap_conf_step_selected(config, def->id))
    {
        row = leap_conf_add_step(result);
        leap_conf_fill_step(row, def, def->name, LEAP_CONF_STEP_SKIP, 0u, "filtered");
        return LEAP_CONF_OK;
    }

    percent = step_total > 0u ?
        (unsigned)((step_index * 100u) / step_total) : 0u;
    leap_conf_emit_progress(
        config,
        LEAP_CONF_PROGRESS_STEP,
        def->id,
        def->name,
        percent,
        (uint32_t)(leap_conf_now_ms() - run_start_ms),
        NULL);

    step_start = leap_conf_now_ms();

    switch (def->kind)
    {
    case LEAP_CONF_KIND_PREFLIGHT:
        row = leap_conf_add_step(result);
        leap_conf_fill_step(row, def, "tools present", LEAP_CONF_STEP_PASS, 0u,
                            "leap_conformance engine ready");
        break;

    case LEAP_CONF_KIND_DISCOVER:
    {
        unsigned peer_count = 0u;

        sub = io->discover_peers(io->user_ctx, 1000, &peer_count);
        row = leap_conf_add_step(result);
        ok  = (sub == 0 && peer_count >= 1u);
        (void)snprintf(detail, sizeof(detail), "peers=%u", peer_count);
        leap_conf_fill_step(
            row,
            def,
            "DISC HELLO finds peers",
            ok ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);

        sub = 0;
        if (config->has_peer_mac)
        {
            sub = io->find_peer_mac(io->user_ctx, config->peer_mac, &ok);
            (void)snprintf(
                detail,
                sizeof(detail),
                "%s",
                config->peer_mac_text != NULL ? config->peer_mac_text : "");
        }
        else
        {
            ok = 0;
            if (peer_count > 1u)
            {
                (void)snprintf(
                    detail,
                    sizeof(detail),
                    "%u peers — set Expected peer MAC (Discovery tab, double-click row)",
                    peer_count);
            }
            else
            {
                detail[0] = '\0';
            }
        }
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "DISC peer MAC",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);
        break;
    }

    case LEAP_CONF_KIND_PROBE_CAPS:
    {
        LeapConformanceDeviceCaps caps;
        uint8_t                   peer_mac[6];

        leap_conformance_device_caps_init(&caps);
        if (!leap_conf_resolve_peer_mac(config, run_state, io, peer_mac))
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(
                row,
                def,
                "read device capabilities",
                LEAP_CONF_STEP_FAIL,
                (uint32_t)(leap_conf_now_ms() - step_start),
                "peer MAC or probe_capabilities missing");
            break;
        }
        if (io->probe_capabilities == NULL)
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(
                row,
                def,
                "read device capabilities",
                LEAP_CONF_STEP_FAIL,
                (uint32_t)(leap_conf_now_ms() - step_start),
                "peer MAC or probe_capabilities missing");
            break;
        }

        sub = io->probe_capabilities(io->user_ctx, peer_mac, &caps);
        row = leap_conf_add_step(result);
        ok  = (sub == 0 && caps.valid);
        if (ok && run_state != NULL)
        {
            run_state->device_caps       = caps;
            run_state->device_caps_valid = 1;
            (void)snprintf(
                detail,
                sizeof(detail),
                "profile=0x%08X outputs=%u inputs=%u masks=%u endpoints=%u",
                caps.dir.active_profile_id != 0u ?
                    caps.dir.active_profile_id : caps.dir.default_profile_id,
                caps.dir.output_bit_count,
                caps.dir.input_bit_count,
                (unsigned)caps.pd_mask_count,
                (unsigned)caps.dir.endpoint_count);
        }
        else if (caps.probe_detail[0] != '\0')
        {
            (void)snprintf(detail, sizeof(detail), "%s", caps.probe_detail);
        }
        else
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "LEAP-DIR probe failed (no endpoint descriptors)");
        }
        leap_conf_fill_step(
            row,
            def,
            "read device capabilities",
            ok ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);
        break;
    }

    case LEAP_CONF_KIND_BOOTSTRAP:
    {
        int      op = 0;
        uint16_t outputs =
            leap_conf_step_outputs(def, run_state, 0);

        sub = io->bootstrap(io->user_ctx, outputs, &op);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "MGMT bootstrap to OP",
            (sub == 0 && op) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            config->peer_mac_text != NULL ? config->peer_mac_text : "");

        if (sub == 0 && op && run_state != NULL && io->get_session_peer_mac != NULL)
        {
            int has_session = 0;

            if (io->get_session_peer_mac(
                    io->user_ctx,
                    run_state->resolved_peer_mac,
                    &has_session) == 0 &&
                has_session)
            {
                run_state->has_resolved_peer_mac = 1;
            }
        }

        if (outputs == 0u)
        {
            outputs = (uint16_t)0x0001u;
        }

        sub = io->pd_write(io->user_ctx, outputs, &ok);
        row = leap_conf_add_step(result);
        (void)snprintf(detail, sizeof(detail), "outputs=0x%04X", outputs);
        leap_conf_fill_step(
            row,
            def,
            "PD WRITE one-shot",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);
        break;
    }

    case LEAP_CONF_KIND_DIAG_READ:
        sub = io->read_diag(io->user_ctx, &ok);
        row = leap_conf_add_step(result);
        if (sub != 0 || !ok)
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "LEAP-DIAG counters/timing read failed (see activity log)");
        }
        else
        {
            detail[0] = '\0';
        }
        leap_conf_fill_step(
            row,
            def,
            "DIAG readback",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);
        break;

    case LEAP_CONF_KIND_LEASE_DEMO:
        sub = io->lease_demo(io->user_ctx, &ok);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "MGMT lease demo",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            "");
        break;

    case LEAP_CONF_KIND_CYCLIC_WRITE:
    {
        LeapPdControllerStats stats;
        unsigned              seconds = def->cyclic_seconds;
        uint16_t              outputs =
            leap_conf_step_outputs(def, run_state, 1);

        if (config->cyclic_seconds > 0u)
        {
            seconds = config->cyclic_seconds;
        }

        unsigned period = config->cyclic_period_ms;

        memset(&stats, 0, sizeof(stats));
        sub = io->cyclic_pd(
            io->user_ctx,
            outputs,
            0,
            seconds,
            period,
            &stats,
            &ok);

        row = leap_conf_add_step(result);
        if (period == 0u)
        {
            (void)snprintf(detail, sizeof(detail), "freerun ran %us", seconds);
        }
        else
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "period=%ums ran %us",
                period,
                seconds);
        }
        leap_conf_fill_step(
            row,
            def,
            "Cyclic PD session",
            sub == 0 ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);

        row = leap_conf_add_step(result);
        (void)snprintf(
            detail,
            sizeof(detail),
            "ok=%llu fail=%llu",
            (unsigned long long)stats.pd_sent_ok,
            (unsigned long long)stats.pd_sent_fail);
        leap_conf_fill_step(
            row,
            def,
            "Cyclic PD ok/fail",
            (sub == 0 && ok && stats.pd_sent_fail == 0u) ?
                LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            stats.pd_sent_fail == 0u ? "fail=0 expected" : detail);

        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "Cyclic heartbeat",
            (stats.heartbeats_sent >= 1u) ?
                LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            "");
        break;
    }

    case LEAP_CONF_KIND_CYCLIC_EXCHANGE:
    {
        LeapPdControllerStats stats;
        unsigned              seconds = def->cyclic_seconds;
        uint16_t              outputs =
            leap_conf_step_outputs(def, run_state, 1);
        const char*           exchange_detail = "PD EXCHANGE";

        if (run_state != NULL && run_state->device_caps_valid)
        {
            exchange_detail = run_state->device_caps.cyclic_exchange_detail;
            if (run_state->device_caps.skip_cyclic_exchange)
            {
                row = leap_conf_add_step(result);
                leap_conf_fill_step(
                    row,
                    def,
                    "PD EXCHANGE cyclic",
                    LEAP_CONF_STEP_SKIP,
                    (uint32_t)(leap_conf_now_ms() - step_start),
                    exchange_detail);
                break;
            }
        }

        if (config->cyclic_seconds > 0u)
        {
            seconds = config->cyclic_seconds;
        }

        unsigned period = config->cyclic_period_ms;

        memset(&stats, 0, sizeof(stats));
        sub = io->cyclic_pd(
            io->user_ctx,
            outputs,
            1,
            seconds,
            period,
            &stats,
            &ok);

        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "PD EXCHANGE cyclic",
            (sub == 0 && ok && stats.pd_sent_fail == 0u) ?
                LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            exchange_detail);
        break;
    }

    case LEAP_CONF_KIND_IO_EXCHANGE_BENCH:
    {
        LeapPdControllerStats stats;
        unsigned              seconds = def->cyclic_seconds;
        /*
         * outputs=0: rotating one-hot on the controller across the active
         * profile width (e.g. IO-0..IO-7 for 8x8).
         * Fixed cyclic_outputs (e.g. 0x00FF) holds every line high — no exercise.
         */
        uint16_t              outputs = 0u;
        const char*           exchange_detail = "PD EXCHANGE soak";
        uint64_t              avg_rtt_us      = 0u;
        double                exchanges_per_s = 0.0;
        int                   soak_pass;
        int                   timeout_pass;
        int                   rtt_pass;
        int                   reply_pass;

        if (run_state != NULL && run_state->device_caps_valid)
        {
            exchange_detail = run_state->device_caps.cyclic_exchange_detail;
            if (run_state->device_caps.skip_cyclic_exchange)
            {
                row = leap_conf_add_step(result);
                leap_conf_fill_step(
                    row,
                    def,
                    "I/O EXCHANGE bench",
                    LEAP_CONF_STEP_SKIP,
                    (uint32_t)(leap_conf_now_ms() - step_start),
                    exchange_detail);
                break;
            }
        }

        if (config->cyclic_seconds > 0u)
        {
            seconds = config->cyclic_seconds;
        }

        memset(&stats, 0, sizeof(stats));
        sub = io->cyclic_pd(
            io->user_ctx,
            outputs,
            1,
            seconds,
            config->cyclic_period_ms,
            &stats,
            &ok);

        if (seconds > 0u)
        {
            exchanges_per_s =
                (double)stats.exchange_replies / (double)seconds;
        }

        if (stats.network_rtt_samples > 0u)
        {
            avg_rtt_us =
                stats.total_network_rtt_us / stats.network_rtt_samples;
        }

        soak_pass =
            (sub == 0 && ok && stats.pd_sent_fail == 0u) ? 1 : 0;
        timeout_pass = (stats.recv_timeouts == 0u) ? 1 : 0;
        rtt_pass     = leap_conf_io_bench_wire_rtt_pass(
            &stats,
            config->cyclic_period_ms);
        reply_pass =
            (stats.exchange_replies > 0u &&
             stats.exchange_replies == stats.cycles_completed) ?
                1 :
                0;

        row = leap_conf_add_step(result);
        if (config->cyclic_period_ms == 0u)
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "freerun %us cycles=%llu exch/s=%.1f",
                seconds,
                (unsigned long long)stats.cycles_completed,
                exchanges_per_s);
        }
        else
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "period=%ums %us cycles=%llu exch/s=%.1f",
                config->cyclic_period_ms,
                seconds,
                (unsigned long long)stats.cycles_completed,
                exchanges_per_s);
        }
        leap_conf_fill_step(
            row,
            def,
            "I/O EXCHANGE soak",
            soak_pass ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);

        row = leap_conf_add_step(result);
        (void)snprintf(
            detail,
            sizeof(detail),
            "recv_timeouts=%llu",
            (unsigned long long)stats.recv_timeouts);
        leap_conf_fill_step(
            row,
            def,
            "Zero exchange timeouts",
            timeout_pass ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            stats.recv_timeouts == 0u ? "recv_timeouts=0 expected" : detail);

        row = leap_conf_add_step(result);
        if (stats.network_rtt_samples == 0u)
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "no wire RTT samples (transport recv timestamp missing)");
        }
        else if (config->cyclic_period_ms == 0u)
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "last/avg/p99/p99.9/max=%llu/%llu/%u/%u/%llu us "
                "(p99 limit %u us, max ceiling %u us)",
                (unsigned long long)stats.last_network_rtt_us,
                (unsigned long long)avg_rtt_us,
                (unsigned)leap_pd_stats_network_rtt_percentile_permille_us(
                    &stats, 990u),
                (unsigned)leap_pd_stats_network_rtt_percentile_permille_us(
                    &stats, 999u),
                (unsigned long long)stats.max_network_rtt_us,
                (unsigned)leap_conf_io_bench_p99_rtt_us(0u),
                (unsigned)leap_conf_io_bench_max_rtt_us(0u));
        }
        else
        {
            (void)snprintf(
                detail,
                sizeof(detail),
                "last/avg/p99/p99.9/max=%llu/%llu/%u/%u/%llu us "
                "(p99 limit %u us, max ceiling %u us)",
                (unsigned long long)stats.last_network_rtt_us,
                (unsigned long long)avg_rtt_us,
                (unsigned)leap_pd_stats_network_rtt_percentile_permille_us(
                    &stats, 990u),
                (unsigned)leap_pd_stats_network_rtt_percentile_permille_us(
                    &stats, 999u),
                (unsigned long long)stats.max_network_rtt_us,
                (unsigned)LEAP_CONF_IO_BENCH_MAX_RTT_US,
                (unsigned)LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US);
        }
        leap_conf_fill_step(
            row,
            def,
            "Wire RTT",
            rtt_pass ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);

        row = leap_conf_add_step(result);
        (void)snprintf(
            detail,
            sizeof(detail),
            "exchange_replies=%llu cycles=%llu lost=%llu rejects=%llu",
            (unsigned long long)stats.exchange_replies,
            (unsigned long long)stats.cycles_completed,
            (unsigned long long)stats.lost_frames,
            (unsigned long long)stats.reply_rejects);
        leap_conf_fill_step(
            row,
            def,
            "Exchange reliability",
            reply_pass ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            detail);
        break;
    }

    case LEAP_CONF_KIND_PD_MASK_WALK:
    {
        size_t m;
        int    op = 0;
        size_t mask_count = 0u;
        const LeapConformancePdMaskStep* masks = NULL;

        if (run_state != NULL && run_state->device_caps_valid)
        {
            masks      = run_state->device_caps.pd_masks;
            mask_count = run_state->device_caps.pd_mask_count;
        }

        if (masks == NULL || mask_count == 0u)
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(
                row,
                def,
                "PD output masks",
                LEAP_CONF_STEP_FAIL,
                (uint32_t)(leap_conf_now_ms() - step_start),
                "device capabilities not probed or no outputs");
            break;
        }

        for (m = 0u; m < mask_count; m++)
        {
            const LeapConformancePdMaskStep* mask_step = &masks[m];
            char                             name[LEAP_CONF_NAME_MAX];

            sub = io->bootstrap(io->user_ctx, mask_step->mask, &op);
            if (sub != 0)
            {
                sub = io->pd_write(io->user_ctx, mask_step->mask, &ok);
            }
            else
            {
                sub = io->pd_write(io->user_ctx, mask_step->mask, &ok);
            }

            (void)snprintf(
                name,
                sizeof(name),
                "PD mask 0x%04X %s",
                mask_step->mask,
                mask_step->label);
            row = leap_conf_add_step(result);
            leap_conf_fill_step(
                row,
                def,
                name,
                (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
                (uint32_t)(leap_conf_now_ms() - step_start),
                "");
#if defined(_WIN32)
            Sleep(400);
#else
            usleep(400000);
#endif
        }
        break;
    }

    case LEAP_CONF_KIND_IDENTIFY:
    {
        uint8_t peer_mac[6];

        if (!leap_conf_resolve_peer_mac(config, run_state, io, peer_mac))
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(row, def, "DISC IDENTIFY", LEAP_CONF_STEP_FAIL,
                                0u, "peer MAC required");
            break;
        }
        sub = io->identify(io->user_ctx, peer_mac, &ok);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "DISC IDENTIFY",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            (run_state != NULL && run_state->device_caps_valid) ?
                run_state->device_caps.identify_detail :
                "DISC IDENTIFY");
        break;
    }

    case LEAP_CONF_KIND_LOCATE:
    {
        uint8_t peer_mac[6];

        if (!leap_conf_resolve_peer_mac(config, run_state, io, peer_mac))
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(row, def, "DISC LOCATE_DEVICE", LEAP_CONF_STEP_FAIL,
                                0u, "peer MAC required");
            break;
        }
        if (run_state != NULL && run_state->device_caps_valid &&
            run_state->device_caps.skip_locate)
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(
                row,
                def,
                "DISC LOCATE_DEVICE",
                LEAP_CONF_STEP_SKIP,
                (uint32_t)(leap_conf_now_ms() - step_start),
                run_state->device_caps.locate_detail);
            break;
        }
        sub = io->locate(
            io->user_ctx,
            peer_mac,
            def->locate_duration_ms > 0u ? def->locate_duration_ms : 1500u,
            &ok);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "DISC LOCATE_DEVICE",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            (run_state != NULL && run_state->device_caps_valid) ?
                run_state->device_caps.locate_detail :
                "LOCATE_DEVICE");
        break;
    }

    default:
        row = leap_conf_add_step(result);
        leap_conf_fill_step(row, def, def->name, LEAP_CONF_STEP_FAIL, 0u,
                            "unknown step kind");
        break;
    }

    (void)step_index;
    return LEAP_CONF_OK;
}

LeapConformanceStatus leap_conformance_run_steps(
    const LeapConformanceRunConfig* config,
    LeapConformanceRunResult*         result_out)
{
    const LeapConformanceScenario* scenario;
    uint64_t                       start_ms;
    unsigned                       i;
    LeapConformanceStatus          status = LEAP_CONF_OK;

    if (config == NULL || result_out == NULL || config->io == NULL)
    {
        return LEAP_CONF_INVALID_ARG;
    }

    memset(result_out, 0, sizeof(*result_out));

    scenario = leap_conformance_scenario_by_id(config->scenario_id);
    if (scenario == NULL)
    {
        scenario = leap_conformance_scenario_at(0u);
    }
    if (scenario == NULL)
    {
        return LEAP_CONF_SCENARIO_UNKNOWN;
    }

    (void)snprintf(
        result_out->summary.scenario_id,
        sizeof(result_out->summary.scenario_id),
        "%s",
        scenario->id);
    if (config->adapter != NULL)
    {
        (void)snprintf(
            result_out->summary.adapter,
            sizeof(result_out->summary.adapter),
            "%s",
            config->adapter);
    }
    if (config->peer_mac_text != NULL)
    {
        (void)snprintf(
            result_out->summary.peer_mac,
            sizeof(result_out->summary.peer_mac),
            "%s",
            config->peer_mac_text);
    }
    if (config->capture_pcap_path != NULL)
    {
        (void)snprintf(
            result_out->summary.pcap_path,
            sizeof(result_out->summary.pcap_path),
            "%s",
            config->capture_pcap_path);
    }

    start_ms = leap_conf_now_ms();
    leap_conf_emit_progress(config, LEAP_CONF_PROGRESS_START, NULL, NULL, 0u, 0u, NULL);

    if (config->io->open_transport(
            config->io->user_ctx,
            config->adapter,
            config->capture_pcap_path) != 0)
    {
        return LEAP_CONF_TRANSPORT_ERROR;
    }

    {
        LeapConformanceRunState run_state;

        memset(&run_state, 0, sizeof(run_state));

        for (i = 0u; i < (unsigned)scenario->step_count; i++)
        {
            if (config->io->is_cancelled != NULL &&
                config->io->is_cancelled(config->io->user_ctx))
            {
                status = LEAP_CONF_CANCELLED;
                break;
            }

            status = leap_conf_run_one(
                config,
                result_out,
                &run_state,
                &scenario->steps[i],
                start_ms,
                i,
                (unsigned)scenario->step_count);
            if (status != LEAP_CONF_OK)
            {
                break;
            }
            if (config->inter_step_delay_ms > 0u)
            {
#if defined(_WIN32)
                Sleep(config->inter_step_delay_ms);
#else
                usleep((useconds_t)config->inter_step_delay_ms * 1000u);
#endif
            }
        }
    }

    result_out->summary.elapsed_ms = (uint32_t)(leap_conf_now_ms() - start_ms);
    leap_conf_update_summary(result_out);

    leap_conf_emit_progress(
        config,
        LEAP_CONF_PROGRESS_DONE,
        NULL,
        NULL,
        100u,
        result_out->summary.elapsed_ms,
        NULL);

    if (config->keep_session_open == 0)
    {
        config->io->close_transport(config->io->user_ctx);
    }

    return status;
}

LeapConformanceStatus leap_conformance_run(
    const LeapConformanceRunConfig* config,
    LeapConformanceRunResult*         result_out)
{
    return leap_conformance_run_steps(config, result_out);
}

LeapConformanceStatus leap_conformance_snapshot(
    const LeapConformanceIo* io,
    LeapConformanceMetrics*  out)
{
    if (io == NULL || out == NULL || io->snapshot == NULL)
    {
        return LEAP_CONF_INVALID_ARG;
    }

    if (io->snapshot(io->user_ctx, out) != 0)
    {
        return LEAP_CONF_TRANSPORT_ERROR;
    }

    return LEAP_CONF_OK;
}

LeapConformanceStatus leap_conformance_cyclic_monitor(
    const LeapConformanceIo*  io,
    unsigned                  interval_ms,
    volatile int*             stop_flag,
    LeapConformanceProgressFn progress_fn,
    void*                     progress_ctx)
{
    LeapConformanceMetrics metrics;
    LeapConformanceProgress progress;

    if (io == NULL || stop_flag == NULL || io->snapshot == NULL)
    {
        return LEAP_CONF_INVALID_ARG;
    }

    if (interval_ms == 0u)
    {
        interval_ms = 100u;
    }

    while (*stop_flag == 0)
    {
        memset(&metrics, 0, sizeof(metrics));
        if (io->snapshot(io->user_ctx, &metrics) == 0 && progress_fn != NULL)
        {
            memset(&progress, 0, sizeof(progress));
            progress.phase  = LEAP_CONF_PROGRESS_METRICS;
            progress.metrics = &metrics;
            progress_fn(progress_ctx, &progress);
        }

#if defined(_WIN32)
        Sleep(interval_ms);
#else
        usleep((useconds_t)interval_ms * 1000u);
#endif
    }

    return LEAP_CONF_OK;
}

void leap_conformance_cancel(const LeapConformanceIo* io)
{
    if (io != NULL && io->cancel != NULL)
    {
        io->cancel(io->user_ctx);
    }
}
