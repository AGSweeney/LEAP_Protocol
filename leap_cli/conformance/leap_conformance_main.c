/*
 * leap_conformance_main.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_conformance_win_io.h"

#include "../win_l2/leap_win_common.h"

#include "leap/conformance/leap_conformance.h"
#include "leap/conformance/leap_conformance_export.h"
#include "leap/conformance/leap_conformance_scenario.h"
#include "leap/leap_build_info.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_log.h"
#include "leap/leap_raw_winpcap.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define LEAP_CONF_MAX_STEP_FILTERS 16u

typedef struct LeapConformanceCliOptions
{
    const char* adapter;
    const char* scenario;
    const char* peer_mac_text;
    const char* report_md;
    const char* report_csv;
    const char* report_json;
    const char* capture_pcap;
    const char* step_filters[LEAP_CONF_MAX_STEP_FILTERS];
    size_t      step_filter_count;
    unsigned    cyclic_seconds;
    unsigned    bootstrap_retries;
    unsigned    retry_delay_ms;
    int         list_adapters;
    int         list_scenarios;
} LeapConformanceCliOptions;

static void leap_conf_unbuffer_stdout(void)
{
#if defined(_WIN32)
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

static void leap_conf_print_usage(const char* prog)
{
    printf(
        "Usage: %s [options] [adapter]\n"
        "\n"
        "  --list-adapters          List Npcap adapters\n"
        "  --list-scenarios         List built-in scenarios\n"
        "  --scenario ID            Optional legacy plan id (default: device DIR)\n"
        "  --steps a,b,c            Run subset of scenario step ids\n"
        "  --peer-mac MAC           Expected peer MAC\n"
        "  --cyclic-seconds N       Cyclic PD duration (default 2)\n"
        "  --bootstrap-retries N    Bootstrap retries (default 3)\n"
        "  --retry-delay-ms N       Delay between retries (default 1000)\n"
        "  --report-md PATH         Write markdown report\n"
        "  --report-csv PATH        Write CSV report\n"
        "  --report-json PATH       Write JSON report\n"
        "  --capture-pcap PATH      PCAP capture path (optional)\n"
        "\n",
        prog != NULL ? prog : "leap_conformance");
}

static void leap_conf_parse_args(
    int                       argc,
    char**                    argv,
    LeapConformanceCliOptions* options)
{
    int i;

    if (options == NULL)
    {
        return;
    }

    memset(options, 0, sizeof(*options));
    options->scenario          = NULL;
    options->cyclic_seconds    = 2u;
    options->bootstrap_retries = 3u;
    options->retry_delay_ms    = 1000u;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--list-adapters") == 0)
        {
            options->list_adapters = 1;
        }
        else if (strcmp(argv[i], "--list-scenarios") == 0)
        {
            options->list_scenarios = 1;
        }
        else if (strcmp(argv[i], "--scenario") == 0 && (i + 1) < argc)
        {
            options->scenario = argv[++i];
        }
        else if (strcmp(argv[i], "--steps") == 0 && (i + 1) < argc)
        {
            char* copy = argv[++i];
            char* token;

            token = strtok(copy, ",");
            while (token != NULL &&
                   options->step_filter_count < LEAP_CONF_MAX_STEP_FILTERS)
            {
                options->step_filters[options->step_filter_count++] = token;
                token = strtok(NULL, ",");
            }
        }
        else if (strcmp(argv[i], "--peer-mac") == 0 && (i + 1) < argc)
        {
            options->peer_mac_text = argv[++i];
        }
        else if (strcmp(argv[i], "--cyclic-seconds") == 0 && (i + 1) < argc)
        {
            options->cyclic_seconds = (unsigned)strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--bootstrap-retries") == 0 && (i + 1) < argc)
        {
            options->bootstrap_retries = (unsigned)strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--retry-delay-ms") == 0 && (i + 1) < argc)
        {
            options->retry_delay_ms = (unsigned)strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--report-md") == 0 && (i + 1) < argc)
        {
            options->report_md = argv[++i];
        }
        else if (strcmp(argv[i], "--report-csv") == 0 && (i + 1) < argc)
        {
            options->report_csv = argv[++i];
        }
        else if (strcmp(argv[i], "--report-json") == 0 && (i + 1) < argc)
        {
            options->report_json = argv[++i];
        }
        else if (strcmp(argv[i], "--capture-pcap") == 0 && (i + 1) < argc)
        {
            options->capture_pcap = argv[++i];
        }
        else if (argv[i][0] != '-' && options->adapter == NULL)
        {
            options->adapter = argv[i];
        }
    }
}

static void leap_conf_progress_stdout(
    void*                         ctx,
    const LeapConformanceProgress* progress)
{
    (void)ctx;

    if (progress == NULL)
    {
        return;
    }

    if (progress->phase == LEAP_CONF_PROGRESS_STEP &&
        progress->step_name != NULL)
    {
        leap_log_printf(
            "[RUN] %s (%u%%)\n",
            progress->step_name,
            progress->percent);
    }
}

static void leap_conf_print_summary(const LeapConformanceRunResult* result)
{
    size_t i;

    if (result == NULL)
    {
        return;
    }

    for (i = 0u; i < result->step_count; i++)
    {
        const LeapConformanceStepResult* step = &result->steps[i];
        leap_log_printf(
            "[%s] %s",
            leap_conformance_step_status_text(step->status),
            step->name);
        if (step->detail[0] != '\0')
        {
            leap_log_printf(" - %s", step->detail);
        }
        leap_log_printf("\n");
    }

    leap_log_printf(
        "\nPassed: %u  Failed: %u  Skipped: %u  Total: %u\n",
        result->summary.passed,
        result->summary.failed,
        result->summary.skipped,
        result->summary.total);
}

int main(int argc, char** argv)
{
    LeapConformanceCliOptions   options;
    LeapConformanceWinContext*  win_ctx;
    LeapConformanceRunConfig    run_config;
    LeapConformanceRunResult  run_result;
    LeapConformanceExportMeta   export_meta;
    LeapConformanceStatus     status;
    char                      started_local[64];
    time_t                    now;
    struct tm                 local_tm;
    uint8_t                   peer_mac[6];
    int                       has_peer_mac = 0;
    int                       exit_code = 0;

#if !defined(_WIN32)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_conformance requires Windows and Npcap.\n");
    return 1;
#else
    leap_conf_unbuffer_stdout();
    leap_log_reset_origin();
    leap_conf_parse_args(argc, argv, &options);

    if (leap_win_handle_version_arg(argc, argv, "leap_conformance") != 0)
    {
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            leap_conf_print_usage(argv[0]);
            return 0;
        }
    }

    leap_build_info_print(stdout, "leap_conformance");

    if (options.list_adapters != 0)
    {
        leap_raw_winpcap_list_devices();
        return 0;
    }

    if (options.list_scenarios != 0)
    {
        size_t n;

        for (n = 0u; n < leap_conformance_scenario_count(); n++)
        {
            const LeapConformanceScenario* scenario =
                leap_conformance_scenario_at(n);
            if (scenario != NULL)
            {
                printf("%s - %s\n", scenario->id, scenario->title);
            }
        }
        return 0;
    }

    if (options.adapter == NULL)
    {
        leap_conf_print_usage(argv[0]);
        return 1;
    }

    win_ctx = leap_conformance_win_create();
    if (win_ctx == NULL)
    {
        return 1;
    }

    leap_conformance_win_set_retries(
        win_ctx,
        options.bootstrap_retries,
        options.retry_delay_ms);

    if (options.peer_mac_text != NULL)
    {
        if (leap_controller_peer_parse_mac(options.peer_mac_text, peer_mac) == 0)
        {
            leap_log_eprintf("invalid --peer-mac\n");
            leap_conformance_win_destroy(win_ctx);
            return 1;
        }
        has_peer_mac = 1;
        leap_conformance_win_set_peer_mac(win_ctx, peer_mac, 1);
    }

    memset(&run_config, 0, sizeof(run_config));
    run_config.scenario_id         = options.scenario;
    run_config.adapter             = options.adapter;
    run_config.peer_mac_text       = options.peer_mac_text;
    run_config.has_peer_mac        = has_peer_mac;
    if (has_peer_mac)
    {
        memcpy(run_config.peer_mac, peer_mac, 6);
    }
    run_config.cyclic_seconds      = options.cyclic_seconds;
    run_config.cyclic_period_ms    = 100u;
    run_config.bootstrap_retries   = options.bootstrap_retries;
    run_config.retry_delay_ms      = options.retry_delay_ms;
    run_config.capture_pcap_path   = options.capture_pcap;
    run_config.step_filter         = options.step_filters;
    run_config.step_filter_count   = options.step_filter_count;
    run_config.progress_fn         = leap_conf_progress_stdout;
    run_config.io                  = leap_conformance_win_io(win_ctx);

    memset(&run_result, 0, sizeof(run_result));
    status = leap_conformance_run(&run_config, &run_result);
    if (status != LEAP_CONF_OK && status != LEAP_CONF_STEP_FAILED)
    {
        leap_log_eprintf("conformance run failed (status=%d)\n", (int)status);
        leap_conformance_win_destroy(win_ctx);
        return 1;
    }

    leap_conf_print_summary(&run_result);

    now = time(NULL);
#if defined(_WIN32)
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif
    (void)strftime(started_local, sizeof(started_local), "%Y-%m-%d %H:%M:%S", &local_tm);

    memset(&export_meta, 0, sizeof(export_meta));
    export_meta.started_local  = started_local;
    export_meta.cyclic_seconds = options.cyclic_seconds;
    export_meta.tool_version   = "leap_conformance";

    if (options.report_md != NULL)
    {
        leap_conformance_export_markdown_path(
            options.report_md, &run_result, &export_meta);
    }
    if (options.report_csv != NULL)
    {
        leap_conformance_export_csv_path(options.report_csv, &run_result);
    }
    if (options.report_json != NULL)
    {
        leap_conformance_export_json_path(
            options.report_json, &run_result, &export_meta);
    }

    exit_code = (run_result.summary.failed > 0u) ? 1 : 0;
    leap_log_printf("OVERALL: %s\n", exit_code == 0 ? "PASS" : "FAIL");
    leap_conformance_win_destroy(win_ctx);
    return exit_code;
#endif
}
