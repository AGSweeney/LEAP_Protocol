/*
 * examples/win_l2/controller_main.c
 *
 * Windows Npcap LEAP controller - transport + leap_controller_stack only.
 *
 * Usage:
 *   leap_win_controller.exe [options] [adapter]
 *   leap_win_controller.exe --list
 *   leap_win_controller.exe --cyclic \Device\NPF_{GUID}
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../win_smoke/leap_win_io.h"
#include "leap_win_common.h"

#include "leap/leap_controller_stack.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void leap_win_l2_unbuffer_stdout(void)
{
#if defined(_WIN32)
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

#define LEAP_LEASE_DEMO_US     2000000u
#define LEAP_LEASE_DEMO_IDLE_S 3u

static volatile int g_controller_stop = 0;

static void controller_shutdown(
    LeapControllerStack*    stack,
    LeapControllerStackIo*  stack_io,
    LeapRawWinpcapSocket*   transport)
{
    leap_controller_stack_release(stack, stack_io);
    leap_raw_winpcap_close(transport);
}

static void controller_print_usage(const char* prog)
{
    printf(
        "Usage: %s [options] [adapter]\n"
        "\n"
        "  adapter              Npcap device (default: \\\\Device\\\\NPF_Loopback)\n"
        "  --list               List Npcap adapters\n"
        "  --cyclic             Run cyclic PD until Ctrl+C\n"
        "  --cyclic-ms MS       Cyclic period (default 100)\n"
        "  --exchange           Use PD exchange mode in cyclic\n"
        "  --lease-demo         Short lease + idle without heartbeat\n"
        "  --diag               Read device DIAG after bootstrap\n"
        "  --promisc            Open adapter in promiscuous mode\n"
        "  --stats              Print transport stats on exit\n"
        "\n"
        "Example (Mellanox 10G):\n"
        "  leap_win_device.exe \"\\\\Device\\\\NPF_{6350838F-D1D5-407E-874E-8EBF642EE1DE}\"\n"
        "  leap_win_controller.exe \"\\\\Device\\\\NPF_{6350838F-D1D5-407E-874E-8EBF642EE1DE}\"\n",
        prog);
}

int main(int argc, char** argv)
{
    LeapWinControllerOptions  options;
    int                       i;
    LeapRawWinpcapSocket      transport;
    LeapRawWinpcapOpenOptions open_options;
    LeapControllerStack       stack;
    LeapControllerStackConfig stack_config;
    LeapControllerStackIo     stack_io;
    LeapPdControllerIo        pd_io;
    LeapControllerStackDiagResult diag_result;
    uint8_t                   peer_mac[6];
    uint32_t                  lease_us = 5000000u;
    const char*               adapter_label;

#if !defined(_WIN32)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_win_controller requires Windows and Npcap.\n");
    return 1;
#else
    leap_win_l2_unbuffer_stdout();
    leap_log_reset_origin();
    leap_win_controller_parse_args(argc, argv, &options);

    if (options.list_adapters != 0)
    {
        leap_raw_winpcap_list_devices();
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            controller_print_usage(argv[0]);
            return 0;
        }
    }

    if (options.lease_demo != 0)
    {
        lease_us = LEAP_LEASE_DEMO_US;
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous          = options.promiscuous;
    open_options.filter_leap_ethertype = 1;

    if (leap_raw_winpcap_open(
            &transport,
            options.adapter,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        leap_win_print_transport_error("open");
        return 1;
    }

    memset(&stack_config, 0, sizeof(stack_config));
    memcpy(stack_config.mgmt.controller_mac, transport.local_mac, 6);
    stack_config.bootstrap_lease_us = lease_us;
    stack_config.pd.cycle_period_ms = options.cyclic_period_ms;
    stack_config.pd.use_exchange    = options.exchange;
    stack_config.pd.stats_log_interval =
        options.stats != 0 ? options.stats_interval : 0u;
    stack_config.pd.heartbeat_every_n_cycles = 10u;
    leap_controller_stack_init(&stack, &stack_config);

    leap_win_controller_io_init(&stack_io, &transport);
    leap_win_pd_init_io(&pd_io, &transport);

    adapter_label = (options.adapter != NULL) ? options.adapter : transport.device_name;

    leap_log_printf("LEAP controller on %s", adapter_label);
    if (options.lease_demo != 0)
    {
        leap_log_printf(" (lease-demo)");
    }
    else if (options.cyclic != 0)
    {
        leap_log_printf(" (cyclic %u ms", options.cyclic_period_ms);
        if (options.exchange != 0)
        {
            leap_log_printf(", exchange");
        }
        leap_log_printf(")");
    }
    else if (options.diag != 0)
    {
        leap_log_printf(" (diag)");
    }
    if (options.promiscuous != 0)
    {
        leap_log_printf(" [promisc]");
    }
    leap_log_printf("\n");
    leap_log_printf("  local MAC: ");
    leap_win_print_mac(NULL, transport.local_mac);

    if (leap_controller_stack_bootstrap(&stack, &stack_io, peer_mac) !=
        LEAP_CTRL_STACK_OK)
    {
        leap_log_eprintf(
            "bootstrap failed (phase=%u)\n",
            (unsigned)leap_controller_stack_get_phase(&stack));
        if (leap_controller_stack_get_phase(&stack) ==
            LEAP_CTRL_STACK_SELECT_PROFILE)
        {
            leap_log_eprintf(
                "hint: device may be in SAFE/OP from a prior session; "
                "rebuild controller or power-cycle ClearCore\n");
        }
        leap_win_print_transport_stats(&transport);
        leap_log_eprintf(
            "hint: disable IPv4/IPv6 on bench NIC; leave Npcap enabled only\n");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    leap_log_printf("bootstrap complete - peer ");
    leap_win_print_mac(NULL, peer_mac);
    leap_log_printf("  session_id: 0x%08X  state: OP\n",
           leap_mgmt_controller_session_id(&stack.mgmt));

    if (options.lease_demo == 0)
    {
        if (options.cyclic != 0)
        {
            leap_win_install_ctrl_handler(&g_controller_stop);
            if (leap_win_controller_run_cyclic_pd_with_link_watch(
                    &stack,
                    &pd_io,
                    &transport,
                    &g_controller_stop) != LEAP_PD_CTRL_OK)
            {
                controller_shutdown(&stack, &stack_io, &transport);
                return 1;
            }
        }
        else if (options.diag == 0)
        {
            if (leap_controller_stack_pd_single_write(
                    &stack, &pd_io, 0x0015u) != LEAP_PD_CTRL_OK)
            {
                controller_shutdown(&stack, &stack_io, &transport);
                return 1;
            }

            leap_log_printf("sent PD WRITE (outputs=0x0015)\n");
        }

        if (options.diag != 0)
        {
            LeapControllerStackDiagStatus diag_status;

            diag_status = leap_controller_stack_read_diag(
                &stack, &stack_io, &diag_result);
            if (diag_status != LEAP_CTRL_STACK_DIAG_OK)
            {
                leap_log_eprintf(
                    "DIAG read failed (status=%d)\n",
                    (int)diag_status);
                controller_shutdown(&stack, &stack_io, &transport);
                return 1;
            }

            leap_controller_stack_log_diag(&diag_result);
        }
    }

    if (options.lease_demo != 0)
    {
        leap_log_printf(
            "lease-demo: idling %u s without heartbeat or PD...\n",
            LEAP_LEASE_DEMO_IDLE_S);
        Sleep(LEAP_LEASE_DEMO_IDLE_S * 1000u);
        leap_log_printf("lease-demo complete - device should show safe outputs\n");
        controller_shutdown(&stack, &stack_io, &transport);
        return 0;
    }

    if (options.cyclic == 0 && options.diag == 0)
    {
        leap_pd_controller_log_stats(&stack.pd);
    }

    if (options.stats != 0 || options.cyclic == 0)
    {
        leap_win_print_transport_stats(&transport);
    }

    controller_shutdown(&stack, &stack_io, &transport);
    return 0;
#endif
}
