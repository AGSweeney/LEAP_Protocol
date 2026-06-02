/*
 * examples/linux_loopback/controller_main.c
 *
 * Linux AF_PACKET LEAP controller — transport + leap_controller_stack only.
 *
 * Usage:
 *   sudo ./leap_linux_controller [interface]
 *   sudo ./leap_linux_controller --cyclic [interface]
 *   sudo ./leap_linux_controller --cyclic-ms 100 [interface]
 *   sudo ./leap_linux_controller --lease-demo [interface]
 *   sudo ./leap_linux_controller --diag [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"
#include "leap_linux_controller_io.h"
#include "leap_linux_pd.h"

#include "leap/leap_controller_stack.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LEAP_LEASE_DEMO_US     2000000u
#define LEAP_LEASE_DEMO_IDLE_S 3u

static volatile sig_atomic_t g_controller_stop = 0;

static void controller_on_sigint(int signo)
{
    (void)signo;
    g_controller_stop = 1;
}

static void controller_shutdown(
    LeapControllerStack*       stack,
    LeapControllerStackIo*     stack_io,
    LeapRawLinuxSocket*        transport)
{
    leap_controller_stack_release(stack, stack_io);
    leap_raw_linux_close(transport);
}

int main(int argc, char** argv)
{
    LeapLinuxControllerOptions options;
    LeapRawLinuxSocket         transport;
    LeapRawLinuxOpenOptions    open_options;
    LeapControllerStack        stack;
    LeapControllerStackConfig  stack_config;
    LeapControllerStackIo      stack_io;
    LeapPdControllerIo         pd_io;
    LeapLinuxPdTransport       pd_transport;
    LeapControllerStackDiagResult diag_result;
    uint8_t                    peer_mac[6];
    uint32_t                   lease_us = 5000000u;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_controller requires Linux AF_PACKET support.\n");
    return 1;
#else
    leap_linux_controller_parse_args(argc, argv, &options);
    leap_log_reset_origin();
    if (options.lease_demo != 0)
    {
        lease_us = LEAP_LEASE_DEMO_US;
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous     = options.promiscuous;
    open_options.filter_dest_mac = 1;

    if (leap_raw_linux_open_ex(
            &transport,
            options.ifname,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        leap_linux_print_transport_error("open");
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

    leap_linux_controller_io_init(&stack_io, &transport);
    pd_transport.sock = &transport;
    leap_linux_pd_init_io(&pd_io, &pd_transport);

    printf("LEAP controller on %s", options.ifname);
    if (options.lease_demo != 0)
    {
        printf(" (lease-demo)");
    }
    else if (options.cyclic != 0)
    {
        printf(" (cyclic %u ms", options.cyclic_period_ms);
        if (options.exchange != 0)
        {
            printf(", exchange");
        }
        printf(")");
    }
    else if (options.diag != 0)
    {
        printf(" (diag)");
    }
    if (options.promiscuous != 0)
    {
        printf(" [promisc]");
    }
    printf("\n");
    leap_linux_print_mac("  local MAC: ", transport.local_mac);

    if (leap_controller_stack_bootstrap(&stack, &stack_io, peer_mac) !=
        LEAP_CTRL_STACK_OK)
    {
        fprintf(
            stderr,
            "bootstrap failed (phase=%u)\n",
            (unsigned)leap_controller_stack_get_phase(&stack));
        leap_raw_linux_close(&transport);
        return 1;
    }

    printf("bootstrap complete — peer ");
    leap_linux_print_mac(NULL, peer_mac);
    printf("  session_id: 0x%08X  state: OP\n",
           leap_mgmt_controller_session_id(&stack.mgmt));

    if (options.lease_demo == 0)
    {
        if (options.cyclic != 0)
        {
            signal(SIGINT, controller_on_sigint);
            if (leap_linux_controller_run_cyclic_pd_with_link_watch(
                    &stack,
                    &pd_io,
                    &transport,
                    (volatile int*)&g_controller_stop) != LEAP_PD_CTRL_OK)
            {
                controller_shutdown(&stack, &stack_io, &transport);
                return 1;
            }
        }
        else if (options.diag == 0)
        {
            if (leap_controller_stack_pd_single_write(
                    &stack, &pd_io, options.outputs) != LEAP_PD_CTRL_OK)
            {
                controller_shutdown(&stack, &stack_io, &transport);
                return 1;
            }

            printf("sent PD WRITE (outputs=0x%04X)\n", options.outputs);
        }

        if (options.diag != 0)
        {
            LeapControllerStackDiagStatus diag_status;

            diag_status = leap_controller_stack_read_diag(
                &stack, &stack_io, &diag_result);
            if (diag_status != LEAP_CTRL_STACK_DIAG_OK)
            {
                fprintf(
                    stderr,
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
        printf(
            "lease-demo: idling %u s without heartbeat or PD...\n",
            LEAP_LEASE_DEMO_IDLE_S);
        sleep(LEAP_LEASE_DEMO_IDLE_S);
        printf("lease-demo complete — device should show safe outputs\n");
        controller_shutdown(&stack, &stack_io, &transport);
        return 0;
    }

    if (options.cyclic == 0 && options.diag == 0)
    {
        leap_pd_controller_log_stats(&stack.pd, peer_mac);
    }

    if (options.stats != 0 || options.cyclic == 0)
    {
        leap_linux_print_transport_stats(&transport);
        printf(
            "send retries (app): %llu\n",
            (unsigned long long)leap_linux_send_retry_count());
    }

    controller_shutdown(&stack, &stack_io, &transport);
    return 0;
#endif
}
