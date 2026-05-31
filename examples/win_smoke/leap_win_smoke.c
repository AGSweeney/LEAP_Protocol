/*
 * leap_win_smoke.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_win_smoke.h"

#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#define LEAP_SMOKE_COLOR_GREEN "\033[32m"
#define LEAP_SMOKE_COLOR_RED   "\033[31m"
#define LEAP_SMOKE_COLOR_RESET "\033[0m"

static int leap_win_smoke_env_no_color(void)
{
    const char* value = getenv("NO_COLOR");

    return (value != NULL && value[0] != '\0') ? 1 : 0;
}

void leap_win_smoke_console_init(LeapWinSmokeOptions* options)
{
    if (options == NULL)
    {
        return;
    }

    if (leap_win_smoke_env_no_color() != 0)
    {
        options->no_color = 1;
    }

#if defined(_WIN32)
    if (options->no_color == 0)
    {
        HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
        DWORD  mode          = 0;

        if (stdout_handle != INVALID_HANDLE_VALUE &&
            GetConsoleMode(stdout_handle, &mode) != 0)
        {
            (void)SetConsoleMode(
                stdout_handle,
                mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        if (stderr_handle != INVALID_HANDLE_VALUE &&
            GetConsoleMode(stderr_handle, &mode) != 0)
        {
            (void)SetConsoleMode(
                stderr_handle,
                mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

static unsigned leap_win_smoke_planned_total(const LeapWinSmokeOptions* options)
{
    unsigned total = 5u;

    if (options == NULL)
    {
        return total;
    }

    if (options->skip_cyclic == 0)
    {
        total += 1u;
    }

    if (options->skip_lease_test == 0)
    {
        total += 3u;
    }

    return total;
}

void leap_win_smoke_report_init(
    LeapWinSmokeReport*            report,
    const LeapWinSmokeOptions*     options)
{
    if (report == NULL)
    {
        return;
    }

    memset(report, 0, sizeof(*report));
    report->planned = leap_win_smoke_planned_total(options);

    if (options != NULL && options->no_color == 0 &&
        leap_win_smoke_env_no_color() == 0)
    {
        report->color_enabled = 1;
    }
}

void leap_win_smoke_pass(LeapWinSmokeReport* report, const char* label)
{
    if (report == NULL)
    {
        return;
    }

    report->passed++;

    if (report->color_enabled != 0)
    {
        printf(
            LEAP_SMOKE_COLOR_GREEN "[PASS]" LEAP_SMOKE_COLOR_RESET " %s\n",
            (label != NULL) ? label : "(unnamed)");
    }
    else
    {
        printf("[PASS] %s\n", (label != NULL) ? label : "(unnamed)");
    }
}

void leap_win_smoke_fail(LeapWinSmokeReport* report, const char* label)
{
    if (report == NULL)
    {
        return;
    }

    if (report->color_enabled != 0)
    {
        fprintf(
            stderr,
            LEAP_SMOKE_COLOR_RED "[FAIL]" LEAP_SMOKE_COLOR_RESET " %s\n",
            (label != NULL) ? label : "(unnamed)");
    }
    else
    {
        fprintf(stderr, "[FAIL] %s\n", (label != NULL) ? label : "(unnamed)");
    }
}

void leap_win_smoke_print_summary(const LeapWinSmokeReport* report)
{
    if (report == NULL)
    {
        return;
    }

    if (report->passed == report->planned)
    {
        if (report->color_enabled != 0)
        {
            printf(
                LEAP_SMOKE_COLOR_GREEN
                "All validations passed (%u/%u)." LEAP_SMOKE_COLOR_RESET "\n",
                report->passed,
                report->planned);
        }
        else
        {
            printf(
                "All validations passed (%u/%u).\n",
                report->passed,
                report->planned);
        }
    }
    else
    {
        if (report->color_enabled != 0)
        {
            fprintf(
                stderr,
                LEAP_SMOKE_COLOR_RED
                "Validations incomplete (%u/%u)." LEAP_SMOKE_COLOR_RESET "\n",
                report->passed,
                report->planned);
        }
        else
        {
            fprintf(
                stderr,
                "Validations incomplete (%u/%u).\n",
                report->passed,
                report->planned);
        }
    }
}

void leap_win_smoke_reset_hello_gate(int* hello_reply_gate)
{
    if (hello_reply_gate != NULL)
    {
        *hello_reply_gate = 0;
    }
}

void leap_win_smoke_options_defaults(LeapWinSmokeOptions* options)
{
    if (options == NULL)
    {
        return;
    }

    memset(options, 0, sizeof(*options));
    options->cycles      = LEAP_WIN_SMOKE_DEFAULT_CYCLES;
    options->cycle_ms    = LEAP_WIN_SMOKE_DEFAULT_CYCLE_MS;
    options->pd_outputs  = LEAP_WIN_SMOKE_DEFAULT_OUTPUTS;
}

static int leap_win_smoke_parse_u16_hex(const char* text, uint16_t* out)
{
    char* end = NULL;
    unsigned long value;

    if (text == NULL || out == NULL)
    {
        return -1;
    }

    value = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > 0xFFFFu)
    {
        return -1;
    }

    *out = (uint16_t)value;
    return 0;
}

int leap_win_smoke_parse_args(
    int                      argc,
    char**                   argv,
    LeapWinSmokeOptions*     options)
{
    int i;

    if (options == NULL)
    {
        return -1;
    }

    leap_win_smoke_options_defaults(options);

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            return 1;
        }
        else if (strcmp(argv[i], "--list") == 0)
        {
            options->list_adapters = 1;
        }
        else if (strcmp(argv[i], "--cycles") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "error: --cycles requires a value\n");
                return -1;
            }

            i++;
            options->cycles = (unsigned)strtoul(argv[i], NULL, 10);
            if (options->cycles == 0u)
            {
                fprintf(stderr, "error: --cycles must be > 0\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--cycle-ms") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "error: --cycle-ms requires a value\n");
                return -1;
            }

            i++;
            options->cycle_ms = (unsigned)strtoul(argv[i], NULL, 10);
            if (options->cycle_ms == 0u)
            {
                fprintf(stderr, "error: --cycle-ms must be > 0\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "--outputs") == 0)
        {
            if (i + 1 >= argc)
            {
                fprintf(stderr, "error: --outputs requires a hex value\n");
                return -1;
            }

            i++;
            if (leap_win_smoke_parse_u16_hex(argv[i], &options->pd_outputs) != 0)
            {
                fprintf(stderr, "error: invalid --outputs value '%s'\n", argv[i]);
                return -1;
            }
        }
        else if (strcmp(argv[i], "--skip-cyclic") == 0)
        {
            options->skip_cyclic = 1;
        }
        else if (strcmp(argv[i], "--skip-lease-test") == 0)
        {
            options->skip_lease_test = 1;
        }
        else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0)
        {
            options->verbose = 1;
        }
        else if (strcmp(argv[i], "--no-color") == 0)
        {
            options->no_color = 1;
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
            return -1;
        }
        else if (options->adapter[0] == '\0')
        {
            (void)snprintf(options->adapter, sizeof(options->adapter), "%s", argv[i]);
        }
        else
        {
            fprintf(stderr, "error: unexpected argument '%s'\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

void leap_win_smoke_print_usage(const char* program)
{
    const char* name = (program != NULL && program[0] != '\0') ? program : "leap_win_smoke";

    printf(
        "Usage: %s [options] [adapter]\n"
        "\n"
        "Windows LEAP wire smoke: bootstrap, PD validation, cyclic exchange, lease expiry.\n"
        "\n"
        "Options:\n"
        "  adapter              Npcap device name (default: \\\\Device\\\\NPF_Loopback)\n"
        "  --list               List known adapter hints and exit\n"
        "  --cycles N           Cyclic PD exchange count (default: %u)\n"
        "  --cycle-ms MS        Cycle period in ms (default: %u)\n"
        "  --outputs HEX        Digital outputs for initial write (default: 0x%04X)\n"
        "  --skip-cyclic        Skip cyclic PD phase\n"
        "  --skip-lease-test    Skip lease expiry phase\n"
        "  --verbose, -v        Extra bootstrap / ownership logging\n"
        "  --no-color           Disable ANSI color output\n"
        "  --help, -h           Show this help\n"
        "\n"
        "Examples:\n"
        "  %s\n"
        "  %s --cycles 100 --cycle-ms 50\n"
        "  %s --cycles 20 --cycle-ms 40\n"
        "  %s \\\\Device\\NPF_{GUID}\n",
        name,
        (unsigned)LEAP_WIN_SMOKE_DEFAULT_CYCLES,
        (unsigned)LEAP_WIN_SMOKE_DEFAULT_CYCLE_MS,
        (unsigned)LEAP_WIN_SMOKE_DEFAULT_OUTPUTS,
        name,
        name,
        name,
        name);
}

void leap_win_smoke_print_adapter_hint(void)
{
    printf("Known adapters / hints:\n");
    leap_raw_winpcap_list_devices();
    printf(
        "\n"
        "Use a real adapter GUID from Wireshark or 'get-netadapter' + Npcap.\n"
        "Loopback smoke tests default to \\\\Device\\NPF_Loopback.\n");
}

static void leap_win_smoke_print_open_help(const char* adapter)
{
    fprintf(
        stderr,
        "\n"
        "Troubleshooting:\n"
        "  1. Install Npcap from https://npcap.com/ (include Loopback support).\n"
        "  2. Confirm wpcap.dll exists: %%SystemRoot%%\\System32\\Npcap\\wpcap.dll\n"
        "  3. Run as Administrator if capture fails with permission errors.\n"
        "  4. List adapters: %s --list\n",
        "leap_win_smoke");

    if (adapter != NULL && adapter[0] != '\0')
    {
        fprintf(
            stderr,
            "  5. Verify adapter name '%s' matches an Npcap interface.\n",
            adapter);
    }
    else
    {
        fprintf(
            stderr,
            "  5. Default loopback adapter is \\\\Device\\NPF_Loopback.\n");
    }
}

int leap_win_smoke_open_transport(
    LeapRawWinpcapSocket*       transport,
    const LeapWinSmokeOptions*  options)
{
    LeapRawWinpcapOpenOptions open_options;
    const char*               adapter = NULL;

    if (transport == NULL || options == NULL)
    {
        return -1;
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = 1;
    open_options.filter_leap_ethertype = 0;

    adapter = (options->adapter[0] != '\0') ? options->adapter : NULL;

    if (leap_raw_winpcap_open(
            transport,
            adapter,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        fprintf(stderr, "error: failed to open Npcap adapter");
        if (adapter != NULL)
        {
            fprintf(stderr, " '%s'", adapter);
        }

        fprintf(stderr, "\n");
        fprintf(stderr, "  driver: %s\n", leap_raw_winpcap_last_error());

        if (leap_raw_winpcap_last_errno() != 0)
        {
            fprintf(stderr, "  win32 err: %d\n", leap_raw_winpcap_last_errno());
        }

        leap_win_smoke_print_open_help(adapter);
        leap_win_smoke_print_adapter_hint();
        return -1;
    }

    return 0;
}

void leap_win_smoke_print_transport_stats(const LeapRawWinpcapSocket* transport)
{
    LeapRawWinpcapStats stats;

    if (transport == NULL)
    {
        return;
    }

    leap_raw_winpcap_get_stats(transport, &stats);
    printf(
        "transport: tx_ok=%llu tx_err=%llu rx_ok=%llu rx_timeout=%llu rx_err=%llu\n",
        (unsigned long long)stats.tx_frames_ok,
        (unsigned long long)stats.tx_errors,
        (unsigned long long)stats.rx_frames_ok,
        (unsigned long long)stats.rx_timeouts,
        (unsigned long long)stats.rx_errors);
}

static void leap_win_smoke_print_mac_stderr(const char* label, const uint8_t* mac)
{
    if (label != NULL)
    {
        fprintf(stderr, "%s", label);
    }

    if (mac == NULL)
    {
        fprintf(stderr, "(null)\n");
        return;
    }

    fprintf(
        stderr,
        "%02x:%02x:%02x:%02x:%02x:%02x\n",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

int leap_win_smoke_validate_bootstrap(
    const LeapControllerStack* stack,
    const LeapDeviceStack*     device,
    const uint8_t*             controller_mac)
{
    LeapState_u16 device_state;

    if (stack == NULL || device == NULL || controller_mac == NULL)
    {
        fprintf(stderr, "error: bootstrap validation - invalid arguments\n");
        return -1;
    }

    if (leap_controller_stack_get_phase(stack) != LEAP_CTRL_STACK_OP)
    {
        fprintf(
            stderr,
            "error: controller not in OP phase (phase=%u status=%d)\n",
            (unsigned)leap_controller_stack_get_phase(stack),
            (int)stack->last_status);
        return -1;
    }

    if (stack->mgmt.session_id == 0u)
    {
        fprintf(stderr, "error: controller session not opened\n");
        return -1;
    }

    if (stack->mgmt.granted_lease_us == 0u)
    {
        fprintf(stderr, "error: controller granted lease is zero\n");
        return -1;
    }

    device_state = leap_mgmt_device_get_state(&device->mgmt);

    if (device_state != LEAP_STATE_OP)
    {
        fprintf(
            stderr,
            "error: device not in OP state (state=%u)\n",
            (unsigned)device_state);
        return -1;
    }

    if (device->mgmt.owner_active == 0u)
    {
        fprintf(stderr, "error: device has no active owner after bootstrap\n");
        return -1;
    }

    if (memcmp(device->mgmt.owner_mac, controller_mac, 6) != 0)
    {
        fprintf(stderr, "error: device owner MAC does not match controller\n");
        leap_win_smoke_print_mac_stderr("  expected: ", controller_mac);
        leap_win_smoke_print_mac_stderr("  actual:   ", device->mgmt.owner_mac);
        return -1;
    }

    if (device->mgmt.owner_session_id != stack->mgmt.session_id)
    {
        fprintf(
            stderr,
            "error: session mismatch (ctrl=%u device=%u)\n",
            (unsigned)stack->mgmt.session_id,
            (unsigned)device->mgmt.owner_session_id);
        return -1;
    }

    printf(
        "bootstrap validation: OP session=%u lease=%u us watchdog=%u us\n",
        (unsigned)stack->mgmt.session_id,
        (unsigned)stack->mgmt.granted_lease_us,
        (unsigned)stack->mgmt.granted_watchdog_us);
    printf(
        "  owner MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        device->mgmt.owner_mac[0],
        device->mgmt.owner_mac[1],
        device->mgmt.owner_mac[2],
        device->mgmt.owner_mac[3],
        device->mgmt.owner_mac[4],
        device->mgmt.owner_mac[5]);

    return 0;
}

int leap_win_smoke_validate_outputs(uint16_t actual, uint16_t expected)
{
    if (actual != expected)
    {
        fprintf(
            stderr,
            "error: device outputs mismatch (expected=0x%04X actual=0x%04X)\n",
            (unsigned)expected,
            (unsigned)actual);
        return -1;
    }

    printf("outputs validation: digital_outputs=0x%04X\n", (unsigned)actual);
    return 0;
}

int leap_win_smoke_run_cyclic(
    LeapControllerStack*        stack,
    const LeapPdControllerIo*   pd_io,
    const LeapWinSmokeOptions*  options,
    LeapWinSmokeReport*         report)
{
    volatile int           stop_flag = 0;
    unsigned               i;
    LeapPdControllerStatus status;
    const LeapPdControllerStats* stats;

    if (stack == NULL || pd_io == NULL || options == NULL)
    {
        return -1;
    }

    stack->pd.config.cycle_period_ms     = options->cycle_ms;
    /*
     * Exchange RTT needs distinct TX/RX delivery; Npcap loopback cooperative
     * smoke uses write cycles and reports cycle work time instead.
     */
    stack->pd.config.use_exchange        = 0;
    stack->pd.config.stats_log_interval  = 0u;
    stack->pd.config.validate_exchange_reply = 0;
    leap_pd_controller_reset_stats(&stack->pd);

    printf(
        "cyclic PD: %u cycles @ %u ms (write mode)\n",
        options->cycles,
        options->cycle_ms);

    for (i = 0u; i < options->cycles; i++)
    {
        status = leap_pd_controller_run_one_cycle(
            &stack->pd,
            &stack->mgmt,
            pd_io,
            stack->peer_mac,
            &stop_flag,
            0);

        if (status == LEAP_PD_CTRL_STOPPED)
        {
            break;
        }

        if (status != LEAP_PD_CTRL_OK)
        {
            fprintf(
                stderr,
                "error: cyclic PD failed at cycle %u (status=%d)\n",
                i + 1u,
                (int)status);
            return -1;
        }

#if defined(_WIN32)
        if (options->cycle_ms > 0u)
        {
            Sleep((DWORD)options->cycle_ms);
        }
#endif
    }

    stats = leap_pd_controller_stats(&stack->pd);
    leap_pd_controller_log_stats(&stack->pd);

    if (stats == NULL)
    {
        return -1;
    }

    if (stats->cycles_completed < (uint64_t)options->cycles)
    {
        fprintf(
            stderr,
            "error: incomplete cyclic run (%llu / %u cycles)\n",
            (unsigned long long)stats->cycles_completed,
            options->cycles);
        return -1;
    }

    if (stats->lost_frames != 0u || stats->recv_timeouts != 0u)
    {
        if (stack->pd.config.use_exchange != 0)
        {
            fprintf(
                stderr,
                "error: cyclic PD lost=%llu timeouts=%llu\n",
                (unsigned long long)stats->lost_frames,
                (unsigned long long)stats->recv_timeouts);
            return -1;
        }
    }

    if (stats->pd_sent_fail != 0u)
    {
        fprintf(
            stderr,
            "error: cyclic PD send failures=%llu\n",
            (unsigned long long)stats->pd_sent_fail);
        return -1;
    }

    if (stats->reply_rejects != 0u || stats->reply_sequence_mismatches != 0u)
    {
        fprintf(
            stderr,
            "error: cyclic reply rejects=%llu seq_mismatch=%llu\n",
            (unsigned long long)stats->reply_rejects,
            (unsigned long long)stats->reply_sequence_mismatches);
        return -1;
    }

    printf(
        "cyclic validation: ok=%llu max_work=%llu us overruns=%llu\n",
        (unsigned long long)stats->pd_sent_ok,
        (unsigned long long)stats->max_cycle_work_us,
        (unsigned long long)stats->cycle_overruns);

    if (stats->cycle_overruns > 0u)
    {
        printf(
            "note: cycle overruns are expected on Windows Npcap loopback "
            "(not real-time scheduling)\n");
    }

    if (report != NULL)
    {
        leap_win_smoke_pass(report, "cyclic PD");
    }

    return 0;
}

int leap_win_smoke_run_lease_expiry(
    LeapControllerStack*         stack,
    const LeapControllerStackIo* stack_io,
    const LeapPdControllerIo*    pd_io,
    LeapDeviceStack*             device,
    int*                         hello_reply_gate,
    const uint8_t*               controller_mac,
    uint16_t*                    digital_outputs,
    uint16_t                     pd_outputs,
    LeapWinSmokePumpFn           pump,
    void*                        pump_ctx,
    LeapWinSmokeReport*          report)
{
    LeapControllerStackConfig  config;
    uint8_t                    peer_mac[6];
    unsigned                   waited_ms;
    LeapState_u16              state;
    uint32_t                   tick_flags;

    if (stack == NULL || stack_io == NULL || pd_io == NULL || device == NULL ||
        controller_mac == NULL || digital_outputs == NULL)
    {
        return -1;
    }

    printf(
        "lease expiry test: short lease bootstrap (%u us), idle %u ms\n",
        (unsigned)LEAP_WIN_SMOKE_SHORT_LEASE_US,
        (unsigned)LEAP_WIN_SMOKE_LEASE_IDLE_MS);

    leap_win_smoke_reset_hello_gate(hello_reply_gate);

    config = stack->config;
    config.bootstrap_lease_us = LEAP_WIN_SMOKE_SHORT_LEASE_US;
    config.recv_timeout_ms    = 3000;
    leap_controller_stack_init(stack, &config);

    if (leap_controller_stack_bootstrap(stack, stack_io, peer_mac) !=
        LEAP_CTRL_STACK_OK)
    {
        fprintf(
            stderr,
            "error: lease-test bootstrap failed (phase=%u status=%d)\n",
            (unsigned)leap_controller_stack_get_phase(stack),
            (int)stack->last_status);
        return -1;
    }

    if (leap_win_smoke_validate_bootstrap(stack, device, controller_mac) != 0)
    {
        return -1;
    }

    if (report != NULL)
    {
        leap_win_smoke_pass(report, "lease bootstrap");
    }

    if (leap_controller_stack_pd_single_write(stack, pd_io, pd_outputs) !=
        LEAP_PD_CTRL_OK)
    {
        fprintf(stderr, "error: lease-test PD write failed\n");
        return -1;
    }

    if (pump != NULL)
    {
        pump(pump_ctx, 8u);
    }

    if (leap_win_smoke_validate_outputs(*digital_outputs, pd_outputs) != 0)
    {
        return -1;
    }

    if (report != NULL)
    {
        leap_win_smoke_pass(report, "lease PD outputs");
    }

    printf(
        "lease expiry test: idling %u ms without heartbeat or PD...\n",
        (unsigned)LEAP_WIN_SMOKE_LEASE_IDLE_MS);

    for (waited_ms = 0u; waited_ms < LEAP_WIN_SMOKE_LEASE_IDLE_MS; waited_ms += 100u)
    {
#if defined(_WIN32)
        Sleep(100);
#endif
        tick_flags = 0u;
        (void)leap_device_stack_tick(
            device,
            leap_raw_winpcap_monotonic_us(),
            &tick_flags);

        if ((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u)
        {
            printf("lease expiry: device entered SAFE during idle\n");
            break;
        }
    }

    state = leap_mgmt_device_get_state(&device->mgmt);

    if (state != LEAP_STATE_SAFE)
    {
        fprintf(
            stderr,
            "error: lease expiry - device still in state %u (expected SAFE=%u)\n",
            (unsigned)state,
            (unsigned)LEAP_STATE_SAFE);
        return -1;
    }

    if (device->mgmt.owner_active != 0u)
    {
        fprintf(stderr, "error: lease expiry - device owner still active\n");
        return -1;
    }

    printf("lease expiry validation: SAFE state, owner cleared\n");

    if (report != NULL)
    {
        leap_win_smoke_pass(report, "lease expiry");
    }

    return 0;
}
