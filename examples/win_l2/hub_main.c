/*
 * examples/win_l2/hub_main.c
 *
 * Multi-device Windows controller: discover → session hub bootstrap → round-robin PD.
 *
 * Usage:
 *   leap_win_hub.exe [options] [adapter]
 *   leap_win_hub.exe --list
 *   leap_win_hub.exe --min-peers 2 --exchange '\Device\NPF_{GUID}'
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../win_smoke/leap_win_io.h"
#include "leap_win_common.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static volatile int g_hub_stop = 0;

#if defined(_WIN32)
static DWORD WINAPI hub_run_sec_thread(LPVOID param)
{
    unsigned sec = *(unsigned*)param;

    if (sec > 0u)
    {
        Sleep(sec * 1000u);
        g_hub_stop = 1;
    }

    return 0;
}
#endif

static void hub_start_run_timer(unsigned run_sec)
{
#if defined(_WIN32)
    static unsigned run_sec_storage;

    if (run_sec == 0u)
    {
        return;
    }

    run_sec_storage = run_sec;
    (void)CreateThread(
        NULL,
        0,
        hub_run_sec_thread,
        &run_sec_storage,
        0,
        NULL);
    leap_log_printf("auto-stop in %u s (--run-sec)\n", run_sec);
#else
    (void)run_sec;
#endif
}

static void hub_unbuffer_stdout(void)
{
#if defined(_WIN32)
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

static void hub_print_usage(const char* prog)
{
    printf(
        "Usage: %s [options] [adapter]\n"
        "\n"
        "  adapter              Npcap device (default: \\\\Device\\\\NPF_Loopback)\n"
        "  --list               List Npcap adapters\n"
        "  --scan-ms MS         Broadcast discovery window (default 1000; 0 = minimal)\n"
        "  --min-peers N        Require N discovered peers; stop scan early (default 1)\n"
        "  --peer-mac MAC       Known peer (repeatable); order sets finish slot when auto\n"
        "  --peer-slot N:MAC    Place peer at hub finish slot N (0 = first finish)\n"
        "  --cyclic-ms MS       Per-peer PD period when pacing (default 100)\n"
        "  --exchange           Use PD exchange mode\n"
        "  --promisc            Promiscuous capture (default on)\n"
        "  --no-promisc         Disable promiscuous capture\n"
        "  --pacing             Sleep to cyclic-ms after each peer cycle (default on)\n"
        "  --no-pacing          Full-speed (round-robin: per peer; parallel: per lap)\n"
        "  --parallel           Send to all peers, then recv (default: round-robin)\n"
        "  --round-robin        Visit one peer at a time (default)\n"
        "  --stats-interval N   Per-peer PD stats every N cycles (default 500)\n"
        "  --run-sec N          Stop automatically after N seconds (0 = run until Ctrl+C)\n"
        "  --no-stats           Disable periodic PD stats\n"
        "\n"
        "Round-robin visits each OP peer once per lap. With --pacing and N peers,\n"
        "each device sees roughly N * cyclic-ms between its own cycles.\n"
        "Parallel sends to all peers before waiting for replies; with --pacing,\n"
        "each device sees roughly cyclic-ms between its own cycles. Finish order\n"
        "follows slot assignment (--peer-mac order or --peer-slot N:MAC).\n"
        "\n"
        "Example (two ClearCores on bench NIC):\n"
        "  %s --min-peers 2 --exchange --promisc "
        "'\\\\Device\\\\NPF_{256115D8-9646-4C83-9306-22347BEAA9D2}'\n"
        "\n"
        "Fast re-run with known MACs (no broadcast scan):\n"
        "  %s --scan-ms 0 --peer-mac 24:15:10:b0:61:57 --peer-mac 24:15:10:b0:5f:bc "
        "--min-peers 2 --exchange $adp\n",
        prog != NULL ? prog : "leap_win_hub",
        prog != NULL ? prog : "leap_win_hub",
        prog != NULL ? prog : "leap_win_hub");
}

static const LeapControllerPeerEntry* hub_table_entry_for_mac(
    const LeapControllerPeerTable* table,
    const uint8_t*                   mac)
{
    int idx;

    if (table == NULL || mac == NULL)
    {
        return NULL;
    }

    idx = leap_controller_peer_table_find(table, mac);
    if (idx < 0)
    {
        return NULL;
    }

    return leap_controller_peer_table_get(table, (unsigned)idx);
}

static unsigned hub_bootstrap_peers_ordered(
    LeapControllerSessionHub*      hub,
    const LeapControllerStackIo* stack_io,
    const LeapControllerPeerTable* table,
    const LeapWinHubOptions*       options,
    const uint8_t*                 controller_mac)
{
    unsigned boot_count = 0u;
    unsigned auto_idx   = 0u;
    int      slot;

    if (options != NULL && options->peer_mac_count > 0u)
    {
        uint8_t slot_used[LEAP_CTRL_MAX_PEERS];

        memset(slot_used, 0, sizeof(slot_used));

        for (slot = 0; slot < LEAP_CTRL_MAX_PEERS; slot++)
        {
            unsigned j;

            for (j = 0u; j < options->peer_mac_count; j++)
            {
                const LeapControllerPeerEntry* entry;
                LeapHelloReply                   hello;
                LeapControllerStackStatus        status;

                if (options->peer_mac_slot[j] != slot)
                {
                    continue;
                }

                entry = hub_table_entry_for_mac(table, options->peer_macs[j]);
                if (entry == NULL || entry->reachable == 0)
                {
                    leap_log_eprintf("  peer slot %d: MAC not discovered ", slot);
                    leap_win_print_mac(NULL, options->peer_macs[j]);
                    continue;
                }

                if (hub->config.skip_foreign_owned_peers != 0 &&
                    leap_controller_peer_owned_by_other(entry, controller_mac) != 0)
                {
                    leap_log_printf("  skipping foreign-owned peer slot %d ", slot);
                    leap_win_print_mac(NULL, entry->mac);
                    continue;
                }

                if (leap_controller_session_hub_find(hub, entry->mac) >= 0)
                {
                    slot_used[slot] = 1u;
                    boot_count++;
                    continue;
                }

                memset(&hello, 0, sizeof(hello));
                hello.current_state      = entry->device_state;
                hello.active_profile_id  = entry->active_profile_id;
                hello.default_profile_id = entry->default_profile_id;
                memcpy(hello.active_owner_mac, entry->active_owner_mac, 6);

                leap_log_printf("  bootstrapping slot %d ", slot);
                leap_win_print_mac(NULL, entry->mac);

                status = leap_controller_session_hub_bootstrap_peer_at_slot(
                    hub,
                    stack_io,
                    entry->mac,
                    &hello,
                    slot);
                if (status != LEAP_CTRL_STACK_OK)
                {
                    leap_log_eprintf("    bootstrap failed (status=%d)\n", (int)status);
                    continue;
                }

                slot_used[slot] = 1u;
                boot_count++;
            }
        }

        while (auto_idx < options->peer_mac_count)
        {
            const LeapControllerPeerEntry* entry;
            LeapHelloReply                   hello;
            LeapControllerStackStatus        status;
            int                              target_slot;

            if (options->peer_mac_slot[auto_idx] >= 0)
            {
                auto_idx++;
                continue;
            }

            entry = hub_table_entry_for_mac(
                table,
                options->peer_macs[auto_idx]);
            auto_idx++;
            if (entry == NULL || entry->reachable == 0)
            {
                continue;
            }

            if (hub->config.skip_foreign_owned_peers != 0 &&
                leap_controller_peer_owned_by_other(entry, controller_mac) != 0)
            {
                continue;
            }

            if (leap_controller_session_hub_find(hub, entry->mac) >= 0)
            {
                boot_count++;
                continue;
            }

            target_slot = -1;
            for (slot = 0; slot < LEAP_CTRL_MAX_PEERS; slot++)
            {
                if (slot_used[(unsigned)slot] == 0u &&
                    hub->slots[slot].in_use == 0)
                {
                    target_slot = slot;
                    break;
                }
            }

            if (target_slot < 0)
            {
                leap_log_eprintf("  no free slot for peer ");
                leap_win_print_mac(NULL, entry->mac);
                continue;
            }

            memset(&hello, 0, sizeof(hello));
            hello.current_state      = entry->device_state;
            hello.active_profile_id  = entry->active_profile_id;
            hello.default_profile_id = entry->default_profile_id;
            memcpy(hello.active_owner_mac, entry->active_owner_mac, 6);

            if (entry->device_state == (uint16_t)LEAP_STATE_SAFE)
            {
                leap_log_printf(
                    "  bootstrapping slot %d (was SAFE, STEAL_EXPIRED) ",
                    target_slot);
            }
            else
            {
                leap_log_printf("  bootstrapping slot %d ", target_slot);
            }
            leap_win_print_mac(NULL, entry->mac);

            status = leap_controller_session_hub_bootstrap_peer_at_slot(
                hub,
                stack_io,
                entry->mac,
                &hello,
                target_slot);
            if (status != LEAP_CTRL_STACK_OK)
            {
                leap_log_eprintf("    bootstrap failed (status=%d)\n", (int)status);
                continue;
            }

            slot_used[(unsigned)target_slot] = 1u;
            boot_count++;
        }

        return boot_count;
    }

    for (slot = 0; slot < (int)table->count; slot++)
    {
        const LeapControllerPeerEntry* entry =
            leap_controller_peer_table_get(table, (unsigned)slot);
        LeapHelloReply                   hello;
        LeapControllerStackStatus        status;
        int                              out_slot;

        if (entry == NULL || entry->reachable == 0)
        {
            continue;
        }

        if (hub->config.skip_foreign_owned_peers != 0 &&
            leap_controller_peer_owned_by_other(entry, controller_mac) != 0)
        {
            continue;
        }

        if (leap_controller_session_hub_find(hub, entry->mac) >= 0)
        {
            boot_count++;
            continue;
        }

        memset(&hello, 0, sizeof(hello));
        hello.current_state      = entry->device_state;
        hello.active_profile_id  = entry->active_profile_id;
        hello.default_profile_id = entry->default_profile_id;
        memcpy(hello.active_owner_mac, entry->active_owner_mac, 6);

        if (entry->device_state == (uint16_t)LEAP_STATE_SAFE)
        {
            leap_log_printf("  bootstrapping peer (was SAFE, STEAL_EXPIRED) ");
        }
        else
        {
            leap_log_printf("  bootstrapping peer ");
        }
        leap_win_print_mac(NULL, entry->mac);

        status = leap_controller_session_hub_bootstrap_peer(
            hub,
            stack_io,
            entry->mac,
            &hello,
            &out_slot);
        if (status != LEAP_CTRL_STACK_OK)
        {
            leap_log_eprintf("    bootstrap failed (status=%d)\n", (int)status);
            continue;
        }

        boot_count++;
    }

    return boot_count;
}

static void hub_apply_pd_runtime_config(
    LeapControllerSessionHub* hub,
    const LeapWinHubOptions*  options)
{
    unsigned i;

    if (hub == NULL || options == NULL)
    {
        return;
    }

    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        LeapControllerStack* stack;

        if (leap_controller_session_hub_is_op(hub, (int)i) == 0)
        {
            continue;
        }

        stack = leap_controller_session_hub_stack(hub, (int)i);
        if (stack == NULL)
        {
            continue;
        }

        stack->pd.config.hub_parallel_finish =
            (options->parallel != 0) ? 1 : 0;
        stack->pd.config.hub_finish_slot = i;
    }
}

static void hub_log_peer_stats(
    const LeapControllerSessionHub* hub,
    int                             slot)
{
    LeapControllerStack* stack;

    stack = leap_controller_session_hub_stack((LeapControllerSessionHub*)hub, slot);
    if (stack == NULL)
    {
        return;
    }

    leap_pd_controller_log_stats(
        &stack->pd,
        leap_controller_session_hub_peer_mac(hub, slot));
}

int main(int argc, char** argv)
{
    LeapWinHubOptions              options;
    LeapWinSharedTransport         transport;
    LeapRawWinpcapOpenOptions      open_options;
    LeapControllerStackIo          stack_io;
    LeapPdControllerIo             pd_io;
    LeapControllerPeerTable        table;
    LeapControllerSessionHub       hub;
    LeapControllerSessionHubConfig hub_config;
    unsigned                       boot_count = 0u;
    unsigned                       i;
    const char*                    adapter_label;

#if !defined(_WIN32)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_win_hub requires Windows and Npcap.\n");
    return 1;
#else
    hub_unbuffer_stdout();
    leap_log_reset_origin();
    leap_win_hub_parse_args(argc, argv, &options);

    if (options.list_adapters != 0)
    {
        leap_raw_winpcap_list_devices();
        return 0;
    }

    for (i = 1u; i < (unsigned)argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            hub_print_usage(argv[0]);
            return 0;
        }
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = options.promiscuous;
    open_options.filter_leap_ethertype = 1;

    leap_win_shared_transport_init(&transport);
    if (leap_raw_winpcap_open(
            &transport.sock,
            options.adapter,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        leap_win_print_transport_error("open");
        leap_win_shared_transport_shutdown(&transport);
        return 1;
    }

    leap_controller_peer_table_init(&table);
    leap_win_controller_io_init_shared(&stack_io, &transport);

    adapter_label =
        (options.adapter != NULL) ? options.adapter : transport.sock.device_name;

    leap_log_printf(
        "LEAP session hub on %s (scan %d ms, min-peers %u, cyclic %u ms%s%s%s)\n",
        adapter_label,
        options.scan_ms,
        options.min_peers,
        options.cyclic_period_ms,
        options.exchange != 0 ? ", exchange" : "",
        options.parallel != 0 ? ", parallel" : ", round-robin",
        options.pacing != 0 ? ", paced" : ", no-pacing");
    if (options.peer_mac_count > 0u)
    {
        leap_log_printf("  known peers: %u (--scan-ms 0 skips broadcast)\n",
               options.peer_mac_count);
    }
    leap_log_printf("  local MAC: ");
    leap_win_print_mac(NULL, transport.sock.local_mac);

    if (leap_win_hub_discover_peers(&table, &stack_io, &options) !=
        LEAP_CTRL_PEER_OK)
    {
        leap_log_eprintf("discovery failed\n");
        leap_win_shared_transport_shutdown(&transport);
        return 1;
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
        if (leap_controller_peer_owned_by_other(entry, transport.sock.local_mac) != 0)
        {
            leap_log_printf(" (foreign owner - hub bootstrap will skip)");
        }
        leap_log_printf("\n");
    }

    if (table.count < options.min_peers)
    {
        leap_log_eprintf(
            "need at least %u peer(s), found %u (check power, switch, promisc)\n",
            options.min_peers,
            table.count);
        leap_win_shared_transport_shutdown(&transport);
        return 2;
    }

    memset(&hub_config, 0, sizeof(hub_config));
    memcpy(hub_config.default_peer.mgmt.controller_mac, transport.sock.local_mac, 6);
    hub_config.default_peer.bootstrap_lease_us     = 5000000u;
    hub_config.default_peer.bootstrap_watchdog_us  = 5000000u;
    hub_config.default_peer.pd.cycle_period_ms = options.cyclic_period_ms;
    hub_config.default_peer.pd.use_exchange    = options.exchange;
    hub_config.default_peer.pd.heartbeat_every_n_cycles = 10u;
    hub_config.default_peer.pd.stats_log_interval =
        options.stats != 0 ? options.stats_interval : 0u;
    hub_config.default_peer.recv_timeout_ms      = LEAP_CTRL_HUB_BOOTSTRAP_RECV_MS;
    hub_config.skip_foreign_owned_peers          = 1;
    leap_controller_session_hub_init(&hub, &hub_config);

    leap_win_pd_init_io_shared(&pd_io, &transport);

    boot_count = hub_bootstrap_peers_ordered(
        &hub,
        &stack_io,
        &table,
        &options,
        transport.sock.local_mac);
    hub_apply_pd_runtime_config(&hub, &options);

    if (boot_count == 0u)
    {
        leap_log_eprintf("hub bootstrap failed (no OP peers)\n");
        leap_win_shared_transport_shutdown(&transport);
        return 1;
    }

    if (boot_count < options.min_peers)
    {
        leap_log_eprintf(
            "need at least %u OP peer(s), bootstrapped %u\n",
            options.min_peers,
            boot_count);
        leap_controller_session_hub_release_all(&hub, &stack_io);
        leap_win_shared_transport_shutdown(&transport);
        return 1;
    }

    leap_log_printf("hub bootstrap: %u peer(s) in OP\n", boot_count);
    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        if (leap_controller_session_hub_is_op(&hub, (int)i) == 0)
        {
            continue;
        }

        leap_log_printf("  slot %u: ", i);
        leap_win_print_mac(
            NULL,
            leap_controller_session_hub_peer_mac(&hub, (int)i));
        leap_log_printf(
            "    session_id=0x%08X peer_state=%u\n",
            leap_mgmt_controller_session_id(
                &leap_controller_session_hub_stack(&hub, (int)i)->mgmt),
            (unsigned)leap_controller_session_hub_stack(&hub, (int)i)
                ->mgmt.peer_device_state);
    }

    hub_start_run_timer(options.run_sec);
    leap_win_install_ctrl_handler(&g_hub_stop);

    if (options.parallel != 0)
    {
        if (leap_win_hub_run_parallel_with_link_watch(
                &hub,
                &pd_io,
                &transport.sock,
                &g_hub_stop,
                options.pacing != 0 ? 1 : 0) != LEAP_PD_CTRL_OK)
        {
            leap_log_eprintf("parallel PD stopped with error\n");
        }
    }
    else if (leap_win_hub_run_round_robin_with_link_watch(
            &hub,
            &pd_io,
            &transport.sock,
            &g_hub_stop,
            options.pacing != 0 ? 1 : 0) != LEAP_PD_CTRL_OK)
    {
        leap_log_eprintf("round-robin PD stopped with error\n");
    }

    leap_log_printf("hub PD stopped - per-peer stats:\n");
    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        if (hub.slots[i].in_use == 0)
        {
            continue;
        }

        hub_log_peer_stats(&hub, (int)i);
    }

    leap_controller_session_hub_release_all(&hub, &stack_io);
    leap_win_print_transport_stats(&transport.sock);
    leap_win_shared_transport_shutdown(&transport);
    return 0;
#endif
}
