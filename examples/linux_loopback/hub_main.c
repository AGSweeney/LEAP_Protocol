/*
 * examples/linux_loopback/hub_main.c
 *
 * Multi-device controller: discover → session hub bootstrap → round-robin PD.
 *
 * Usage:
 *   sudo ./leap_linux_hub [interface]
 *   sudo ./leap_linux_hub --scan-ms 3000 --cyclic lo
 *   sudo ./leap_linux_hub --exchange lo
 *
 * Run one leap_linux_device per peer on the same interface (distinct MACs).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"
#include "leap_linux_controller_io.h"
#include "leap_linux_pd.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_protocol.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEAP_HUB_DEFAULT_SCAN_MS   3000
#define LEAP_HUB_DEFAULT_CYCLIC_MS 100u

typedef struct LeapLinuxHubOptions
{
    const char* ifname;
    int         scan_ms;
    unsigned    cyclic_period_ms;
    int         exchange;
    int         promiscuous;
} LeapLinuxHubOptions;

static volatile sig_atomic_t g_hub_stop = 0;

static void hub_on_sigint(int signo)
{
    (void)signo;
    g_hub_stop = 1;
}

static void hub_parse_args(int argc, char** argv, LeapLinuxHubOptions* options)
{
    int i;

    if (options == NULL)
    {
        return;
    }

    options->ifname           = "lo";
    options->scan_ms          = LEAP_HUB_DEFAULT_SCAN_MS;
    options->cyclic_period_ms = LEAP_HUB_DEFAULT_CYCLIC_MS;
    options->exchange         = 0;
    options->promiscuous      = 1;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--scan-ms") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->scan_ms = atoi(argv[i]);
                if (options->scan_ms <= 0)
                {
                    options->scan_ms = LEAP_HUB_DEFAULT_SCAN_MS;
                }
            }
        }
        else if (strcmp(argv[i], "--cyclic-ms") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->cyclic_period_ms = (unsigned)strtoul(argv[i], NULL, 10);
                if (options->cyclic_period_ms == 0u)
                {
                    options->cyclic_period_ms = LEAP_HUB_DEFAULT_CYCLIC_MS;
                }
            }
        }
        else if (strcmp(argv[i], "--exchange") == 0)
        {
            options->exchange = 1;
        }
        else if (strcmp(argv[i], "--promisc") == 0)
        {
            options->promiscuous = 1;
        }
        else if (argv[i][0] != '-')
        {
            options->ifname = argv[i];
        }
    }
}

int main(int argc, char** argv)
{
    LeapLinuxHubOptions            options;
    LeapRawLinuxSocket             transport;
    LeapRawLinuxOpenOptions        open_options;
    LeapControllerStackIo          stack_io;
    LeapPdControllerIo             pd_io;
    LeapLinuxPdTransport           pd_transport;
    LeapControllerPeerTable        table;
    LeapControllerSessionHub       hub;
    LeapControllerSessionHubConfig hub_config;
    unsigned                       boot_count = 0u;
    unsigned                       i;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_hub requires Linux AF_PACKET support.\n");
    return 1;
#else
    hub_parse_args(argc, argv, &options);

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

    leap_controller_peer_table_init(&table);
    leap_linux_controller_io_init(&stack_io, &transport);

    printf(
        "LEAP session hub on %s (scan %d ms, cyclic %u ms%s)\n",
        options.ifname,
        options.scan_ms,
        options.cyclic_period_ms,
        options.exchange != 0 ? ", exchange" : "");
    leap_linux_print_mac("  local MAC: ", transport.local_mac);

    if (leap_controller_peer_table_discover(
            &table, &stack_io, options.scan_ms) != LEAP_CTRL_PEER_OK)
    {
        fprintf(stderr, "discovery failed\n");
        leap_raw_linux_close(&transport);
        return 1;
    }

    printf("discovered %u peer(s)\n", table.count);
    for (i = 0u; i < table.count; i++)
    {
        const LeapControllerPeerEntry* entry =
            leap_controller_peer_table_get(&table, i);

        if (entry == NULL)
        {
            continue;
        }

        printf("  peer %u: ", i + 1u);
        leap_linux_print_mac(NULL, entry->mac);
        if (leap_controller_peer_owned_by_other(entry, transport.local_mac) != 0)
        {
            printf("    (foreign owner — hub bootstrap will skip)\n");
        }
    }

    if (table.count == 0u)
    {
        leap_raw_linux_close(&transport);
        return 2;
    }

    memset(&hub_config, 0, sizeof(hub_config));
    memcpy(hub_config.default_peer.mgmt.controller_mac, transport.local_mac, 6);
    hub_config.default_peer.bootstrap_lease_us = 5000000u;
    hub_config.default_peer.pd.cycle_period_ms = options.cyclic_period_ms;
    hub_config.default_peer.pd.use_exchange    = options.exchange;
    hub_config.default_peer.pd.heartbeat_every_n_cycles = 10u;
    hub_config.skip_foreign_owned_peers        = 1;
    leap_controller_session_hub_init(&hub, &hub_config);

    pd_transport.sock = &transport;
    leap_linux_pd_init_io(&pd_io, &pd_transport);

    if (leap_controller_session_hub_bootstrap_table(
            &hub, &stack_io, &table, &boot_count) != LEAP_CTRL_HUB_OK)
    {
        fprintf(stderr, "hub bootstrap failed (no OP peers)\n");
        leap_raw_linux_close(&transport);
        return 1;
    }

    printf("hub bootstrap: %u peer(s) in OP\n", boot_count);
    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        if (leap_controller_session_hub_is_op(&hub, (int)i) == 0)
        {
            continue;
        }

        printf("  slot %u: ", i);
        leap_linux_print_mac(
            NULL,
            leap_controller_session_hub_peer_mac(&hub, (int)i));
        printf(
            "    session_id=0x%08X\n",
            leap_mgmt_controller_session_id(
                &leap_controller_session_hub_stack(&hub, (int)i)->mgmt));
    }

    signal(SIGINT, hub_on_sigint);
    printf("round-robin PD started (Ctrl+C to stop)\n");

    if (leap_linux_hub_run_round_robin_with_link_watch(
            &hub,
            &pd_io,
            &transport,
            (volatile int*)&g_hub_stop) != LEAP_PD_CTRL_OK)
    {
        fprintf(stderr, "round-robin PD stopped with error\n");
    }

    leap_controller_session_hub_release_all(&hub, &stack_io);
    leap_linux_print_transport_stats(&transport);
    leap_raw_linux_close(&transport);
    return 0;
#endif
}
