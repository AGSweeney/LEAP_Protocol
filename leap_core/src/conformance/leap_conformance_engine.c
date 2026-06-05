/*
 * leap_conformance_engine.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
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

static LeapConformanceStatus leap_conf_run_one(
    const LeapConformanceRunConfig*      config,
    LeapConformanceRunResult*            result,
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

        sub = io->discover_peers(io->user_ctx, 3000, &peer_count);
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
        }
        else
        {
            ok = 0;
        }
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "DISC peer MAC",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            config->peer_mac_text != NULL ? config->peer_mac_text : "");
        break;
    }

    case LEAP_CONF_KIND_BOOTSTRAP:
    {
        int op = 0;

        sub = io->bootstrap(io->user_ctx, def->pd_outputs, &op);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "MGMT bootstrap to OP",
            (sub == 0 && op) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            config->peer_mac_text != NULL ? config->peer_mac_text : "");

        sub = io->pd_write(io->user_ctx, def->pd_outputs, &ok);
        row = leap_conf_add_step(result);
        (void)snprintf(detail, sizeof(detail), "outputs=0x%04X", def->pd_outputs);
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
        leap_conf_fill_step(
            row,
            def,
            "DIAG readback",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            "");
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

        if (config->cyclic_seconds > 0u)
        {
            seconds = config->cyclic_seconds;
        }

        memset(&stats, 0, sizeof(stats));
        sub = io->cyclic_pd(
            io->user_ctx,
            def->pd_outputs,
            0,
            seconds,
            100u,
            &stats,
            &ok);

        row = leap_conf_add_step(result);
        (void)snprintf(detail, sizeof(detail), "ran %us", seconds);
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

        if (config->cyclic_seconds > 0u)
        {
            seconds = config->cyclic_seconds;
        }

        memset(&stats, 0, sizeof(stats));
        sub = io->cyclic_pd(
            io->user_ctx,
            def->pd_outputs,
            1,
            seconds,
            100u,
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
            "inputs=0x0000 (no exposed DI)");
        break;
    }

    case LEAP_CONF_KIND_PD_MASK_WALK:
    {
        static const struct
        {
            uint16_t    mask;
            const char* label;
        } k_masks[] = {
            { 0x0002u, "CH0 green" },
            { 0x0004u, "CH0 blue" },
            { 0x0008u, "CH0 white" },
            { 0x0010u, "CH1 red" },
        };
        size_t m;
        int    op = 0;

        for (m = 0u; m < sizeof(k_masks) / sizeof(k_masks[0]); m++)
        {
            char name[LEAP_CONF_NAME_MAX];

            sub = io->bootstrap(io->user_ctx, k_masks[m].mask, &op);
            if (sub != 0)
            {
                sub = io->pd_write(io->user_ctx, k_masks[m].mask, &ok);
            }
            else
            {
                sub = io->pd_write(io->user_ctx, k_masks[m].mask, &ok);
            }

            (void)snprintf(
                name,
                sizeof(name),
                "PD mask 0x%04X %s",
                k_masks[m].mask,
                k_masks[m].label);
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
        if (!config->has_peer_mac)
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(row, def, "DISC IDENTIFY", LEAP_CONF_STEP_FAIL,
                                0u, "peer MAC required");
            break;
        }
        sub = io->identify(io->user_ctx, config->peer_mac, &ok);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "DISC IDENTIFY",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            "releases stale OP first; no LED");
        break;

    case LEAP_CONF_KIND_LOCATE:
        if (!config->has_peer_mac)
        {
            row = leap_conf_add_step(result);
            leap_conf_fill_step(row, def, "DISC LOCATE_DEVICE", LEAP_CONF_STEP_FAIL,
                                0u, "peer MAC required");
            break;
        }
        sub = io->locate(
            io->user_ctx,
            config->peer_mac,
            def->locate_duration_ms > 0u ? def->locate_duration_ms : 1500u,
            &ok);
        row = leap_conf_add_step(result);
        leap_conf_fill_step(
            row,
            def,
            "DISC LOCATE_DEVICE",
            (sub == 0 && ok) ? LEAP_CONF_STEP_PASS : LEAP_CONF_STEP_FAIL,
            (uint32_t)(leap_conf_now_ms() - step_start),
            "GPIO13 blink ~1.5s");
        break;

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

    scenario = leap_conformance_scenario_by_id(
        config->scenario_id != NULL ? config->scenario_id : "glc618wl_bench_v1");
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

    for (i = 0u; i < (unsigned)scenario->step_count; i++)
    {
        status = leap_conf_run_one(
            config,
            result_out,
            &scenario->steps[i],
            start_ms,
            i,
            (unsigned)scenario->step_count);
        if (status != LEAP_CONF_OK)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(500);
#else
        usleep(500000);
#endif
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
