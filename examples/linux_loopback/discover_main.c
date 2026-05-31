/*
 * examples/linux_loopback/discover_main.c
 *
 * Broadcast HELLO and print discovered LEAP devices on an interface.
 *
 * Usage:
 *   sudo ./leap_linux_discover [interface]
 *   sudo ./leap_linux_discover --scan-ms 3000 lo
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"
#include "leap_linux_controller_io.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEAP_DISCOVER_DEFAULT_SCAN_MS 3000

static void discover_print_usage(const char* prog)
{
    fprintf(
        stderr,
        "Usage: %s [--scan-ms N] [interface]\n",
        prog != NULL ? prog : "leap_linux_discover");
}

static int discover_parse_scan_ms(int argc, char** argv, int* scan_ms_out)
{
    int i;

    *scan_ms_out = LEAP_DISCOVER_DEFAULT_SCAN_MS;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--scan-ms") == 0)
        {
            if (i + 1 >= argc)
            {
                return -1;
            }

            *scan_ms_out = atoi(argv[i + 1]);
            if (*scan_ms_out <= 0)
            {
                return -1;
            }
            return 0;
        }
    }

    return 0;
}

static const char* discover_ifname(int argc, char** argv)
{
    int i;

    for (i = argc - 1; i >= 1; i--)
    {
        if (argv[i][0] != '-')
        {
            return argv[i];
        }
    }

    return "lo";
}

int main(int argc, char** argv)
{
    LeapRawLinuxSocket        transport;
    LeapRawLinuxOpenOptions   open_options;
    LeapControllerStackIo     io;
    LeapControllerPeerTable   table;
    int                       scan_ms;
    const char*               ifname;
    unsigned                  i;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_discover requires Linux AF_PACKET support.\n");
    return 1;
#else
    if (discover_parse_scan_ms(argc, argv, &scan_ms) != 0)
    {
        discover_print_usage(argv[0]);
        return 1;
    }

    ifname = discover_ifname(argc, argv);

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous     = 1;
    open_options.filter_dest_mac = 0;

    if (leap_raw_linux_open_ex(
            &transport,
            ifname,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        leap_linux_print_transport_error("open");
        return 1;
    }

    leap_controller_peer_table_init(&table);
    leap_linux_controller_io_init(&io, &transport);

    printf("LEAP discovery on %s (scan %d ms)\n", ifname, scan_ms);
    leap_linux_print_mac("  local MAC: ", transport.local_mac);

    if (leap_controller_peer_table_discover(&table, &io, scan_ms) != LEAP_CTRL_PEER_OK)
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
        printf("    profile=0x%08X state=%u\n",
               entry->active_profile_id,
               (unsigned)entry->device_state);
    }

    leap_raw_linux_close(&transport);
    return (table.count > 0u) ? 0 : 2;
#endif
}
