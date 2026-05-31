/*
 * examples/win_l2/discover_main.c
 *
 * Broadcast HELLO and print discovered LEAP devices on an Npcap adapter.
 *
 * Usage:
 *   leap_win_discover.exe [options] [adapter]
 *   leap_win_discover.exe --scan-ms 5000 '\Device\NPF_{GUID}'
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../win_smoke/leap_win_io.h"
#include "leap_win_common.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static void discover_unbuffer_stdout(void)
{
#if defined(_WIN32)
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

static void discover_print_usage(const char* prog)
{
    printf(
        "Usage: %s [options] [adapter]\n"
        "\n"
        "  adapter        Npcap device\n"
        "  --list         List Npcap adapters\n"
        "  --scan-ms MS   HELLO scan duration (default 3000)\n"
        "  --promisc      Promiscuous capture (default on)\n"
        "  --no-promisc   Disable promiscuous capture\n",
        prog != NULL ? prog : "leap_win_discover");
}

int main(int argc, char** argv)
{
    LeapWinDiscoverOptions    options;
    LeapRawWinpcapSocket      transport;
    LeapRawWinpcapOpenOptions open_options;
    LeapControllerStackIo     io;
    LeapControllerPeerTable   table;
    unsigned                  i;
    const char*               adapter_label;

#if !defined(_WIN32)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_win_discover requires Windows and Npcap.\n");
    return 1;
#else
    discover_unbuffer_stdout();
    leap_log_reset_origin();
    leap_win_discover_parse_args(argc, argv, &options);

    if (options.list_adapters != 0)
    {
        leap_raw_winpcap_list_devices();
        return 0;
    }

    for (i = 1u; i < (unsigned)argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            discover_print_usage(argv[0]);
            return 0;
        }
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = options.promiscuous;
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

    leap_controller_peer_table_init(&table);
    leap_win_controller_io_init(&io, &transport);

    adapter_label =
        (options.adapter != NULL) ? options.adapter : transport.device_name;

    leap_log_printf(
        "LEAP discovery on %s (scan %d ms",
        adapter_label,
        options.scan_ms);
    if (options.min_peers > 0u)
    {
        leap_log_printf(", min-peers %u", options.min_peers);
    }
    leap_log_printf(")\n");
    leap_log_printf("  local MAC: ");
    leap_win_print_mac(NULL, transport.local_mac);

    {
        LeapControllerPeerDiscoverConfig disc_config;

        memset(&disc_config, 0, sizeof(disc_config));
        disc_config.scan_duration_ms = options.scan_ms;
        disc_config.min_peers        = options.min_peers;

        if (leap_controller_peer_table_discover_ex(
                &table, &io, &disc_config) != LEAP_CTRL_PEER_OK)
        {
            leap_log_eprintf("discovery failed\n");
            leap_raw_winpcap_close(&transport);
            return 1;
        }
    }

    leap_log_printf("discovered %u peer(s)\n", table.count);
    for (i = 0u; i < table.count; i++)
    {
        const LeapControllerPeerEntry* entry =
            leap_controller_peer_table_get(&table, i);

        if (entry == NULL)
        {
            continue;
        }

        leap_log_printf("  peer %u: ", i + 1u);
        leap_win_print_mac(NULL, entry->mac);
        leap_log_printf(
            "    profile=0x%08X state=%u",
            entry->active_profile_id,
            (unsigned)entry->device_state);
        if (entry->active_owner_mac[0] != 0u ||
            entry->active_owner_mac[1] != 0u ||
            entry->active_owner_mac[2] != 0u ||
            entry->active_owner_mac[3] != 0u ||
            entry->active_owner_mac[4] != 0u ||
            entry->active_owner_mac[5] != 0u)
        {
            leap_log_printf(" owner=");
            leap_win_print_mac(NULL, entry->active_owner_mac);
        }
        else
        {
            leap_log_printf("\n");
        }
    }

    leap_raw_winpcap_close(&transport);
    return (table.count > 0u) ? 0 : 2;
#endif
}
