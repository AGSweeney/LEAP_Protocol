/*
 * leap_win_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_win_common.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_frame.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static volatile int* g_win_ctrl_stop_ptr = NULL;

#if defined(_WIN32)
static BOOL WINAPI leap_win_console_ctrl_handler(DWORD ctrl_type)
{
    (void)ctrl_type;

    if (g_win_ctrl_stop_ptr != NULL)
    {
        *g_win_ctrl_stop_ptr = 1;
    }

    return TRUE;
}
#endif

void leap_win_install_ctrl_handler(volatile int* stop_flag)
{
#if defined(_WIN32)
    g_win_ctrl_stop_ptr = stop_flag;
    (void)SetConsoleCtrlHandler(leap_win_console_ctrl_handler, TRUE);
#else
    (void)stop_flag;
#endif
}

int leap_win_link_stop_on_down(
    LeapRawWinpcapSocket* sock,
    volatile int*       stop_flag)
{
    int                       changed = 0;
    LeapRawWinpcapLinkState   state;

    if (sock == NULL || stop_flag == NULL)
    {
        return 0;
    }

    if (leap_raw_winpcap_poll_link(sock, &changed, &state) != 0)
    {
        return 0;
    }

    if (changed != 0)
    {
        leap_log_printf(
            "transport: link %s (iface_up=%d transitions=%llu)\n",
            state.link_up != 0 ? "UP" : "DOWN",
            state.interface_up,
            (unsigned long long)sock->stats.link_transitions);
    }

    if (state.link_up == 0)
    {
        if (*stop_flag == 0)
        {
            leap_log_printf("transport: link down - stopping PD\n");
            *stop_flag = 1;
        }
        return 1;
    }

    return 0;
}

LeapPdControllerStatus leap_win_controller_run_cyclic_pd_with_link_watch(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag)
{
    LeapPdControllerStatus status;

    if (stack == NULL || pd_io == NULL || transport == NULL || stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (leap_controller_stack_get_phase(stack) != LEAP_CTRL_STACK_OP)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    leap_log_printf(
        "cyclic PD (%u ms%s) - Ctrl+C or link-down to stop\n",
        stack->pd.config.cycle_period_ms,
        stack->pd.config.use_exchange != 0 ? ", exchange" : "");

    while (*stop_flag == 0)
    {
        (void)leap_win_link_stop_on_down(transport, stop_flag);
        if (*stop_flag != 0)
        {
            break;
        }

        status = leap_pd_controller_run_one_cycle(
            &stack->pd,
            &stack->mgmt,
            pd_io,
            stack->peer_mac,
            stop_flag,
            1);
        if (status == LEAP_PD_CTRL_STOPPED)
        {
            break;
        }
        if (status != LEAP_PD_CTRL_OK)
        {
            return status;
        }
    }

    leap_log_printf("cyclic PD stopped\n");
    leap_pd_controller_log_stats(&stack->pd, stack->peer_mac);
    return LEAP_PD_CTRL_OK;
}

LeapPdControllerStatus leap_win_hub_run_round_robin_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag,
    int                       sleep_for_period)
{
    LeapPdControllerStatus status;
    unsigned               i;
    int                    ran_any;

    if (hub == NULL || pd_io == NULL || transport == NULL || stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (hub->active_count == 0u)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    leap_log_printf(
        "hub round-robin PD%s - Ctrl+C or link-down to stop\n",
        sleep_for_period != 0 ? " (paced per peer)" : "");

    while (*stop_flag == 0)
    {
        (void)leap_win_link_stop_on_down(transport, stop_flag);
        if (*stop_flag != 0)
        {
            break;
        }

        ran_any = 0;

        for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
        {
            LeapControllerStack* stack;

            if (hub->slots[i].in_use == 0)
            {
                continue;
            }

            stack = &hub->slots[i].stack;
            if (leap_controller_stack_get_phase(stack) != LEAP_CTRL_STACK_OP)
            {
                continue;
            }

            status = leap_pd_controller_run_one_cycle(
                &stack->pd,
                &stack->mgmt,
                pd_io,
                hub->slots[i].peer_mac,
                stop_flag,
                sleep_for_period);
            if (status == LEAP_PD_CTRL_STOPPED)
            {
                return LEAP_PD_CTRL_OK;
            }
            if (status != LEAP_PD_CTRL_OK)
            {
                return status;
            }

            ran_any = 1;
        }

        if (ran_any == 0)
        {
            return LEAP_PD_CTRL_INVALID_ARG;
        }
    }

    return LEAP_PD_CTRL_OK;
}

LeapPdControllerStatus leap_win_hub_run_parallel_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag,
    int                       sleep_for_period)
{
    LeapPdControllerStatus status;

    if (hub == NULL || pd_io == NULL || transport == NULL || stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (hub->active_count == 0u)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    leap_log_printf(
        "hub parallel PD%s - Ctrl+C or link-down to stop\n",
        sleep_for_period != 0 ? " (paced per lap)" : "");

    while (*stop_flag == 0)
    {
        (void)leap_win_link_stop_on_down(transport, stop_flag);
        if (*stop_flag != 0)
        {
            break;
        }

        status = leap_controller_session_hub_run_parallel_lap(
            hub,
            pd_io,
            stop_flag,
            sleep_for_period);
        if (status == LEAP_PD_CTRL_STOPPED)
        {
            return LEAP_PD_CTRL_OK;
        }
        if (status != LEAP_PD_CTRL_OK)
        {
            return status;
        }
    }

    return LEAP_PD_CTRL_OK;
}

void leap_win_print_transport_error(const char* action)
{
    const char* err = leap_raw_winpcap_last_error();
    int         win_err = leap_raw_winpcap_last_errno();

    if (err != NULL && err[0] != '\0')
    {
        if (win_err != 0)
        {
            leap_log_eprintf(
                "%s failed: %s (win32 err=%d)\n",
                action,
                err,
                win_err);
        }
        else
        {
            leap_log_eprintf("%s failed: %s\n", action, err);
        }
    }
    else if (win_err != 0)
    {
        leap_log_eprintf("%s failed (win32 err=%d)\n", action, win_err);
    }
    else
    {
        leap_log_eprintf("%s failed\n", action);
    }
}

void leap_win_print_transport_stats(const LeapRawWinpcapSocket* sock)
{
    LeapRawWinpcapStats stats;

    if (sock == NULL)
    {
        return;
    }

    leap_raw_winpcap_get_stats(sock, &stats);
    leap_log_printf(
        "transport: tx_ok=%llu tx_err=%llu "
        "rx_ok=%llu rx_filtered=%llu rx_timeout=%llu rx_err=%llu rx_short=%llu "
        "link_xitions=%llu\n",
        (unsigned long long)stats.tx_frames_ok,
        (unsigned long long)stats.tx_errors,
        (unsigned long long)stats.rx_frames_ok,
        (unsigned long long)stats.rx_filtered,
        (unsigned long long)stats.rx_timeouts,
        (unsigned long long)stats.rx_errors,
        (unsigned long long)stats.rx_short_frames,
        (unsigned long long)stats.link_transitions);

    if (stats.rx_frames_ok > 0u)
    {
        if (sock->last_rx_valid_leap != 0)
        {
            leap_log_printf(
                "transport: last_rx eth_cap=%u svc=0x%04X msg=0x%04X leap_len=%zu "
                "(expect eth_cap~100 leap_len~86 HELLO_REPLY)\n",
                (unsigned)sock->last_rx_eth_caplen,
                (unsigned)sock->last_rx_service_id,
                (unsigned)sock->last_rx_message_type,
                sock->last_rx_payload_len);
        }
        else
        {
            leap_log_printf(
                "transport: last_rx eth_cap=%u leap_len=%zu (no LEAP magic)\n",
                (unsigned)sock->last_rx_eth_caplen,
                sock->last_rx_payload_len);
        }

        if (sock->last_rx_payload_len > 0u)
        {
            LeapFrameView        view;
            LeapFrameParseResult parse_result;

            parse_result = leap_frame_parse(
                sock->last_rx_payload,
                sock->last_rx_payload_len,
                &view);
            leap_log_printf(
                "transport: last_rx parse=%s\n",
                leap_frame_parse_result_string(parse_result));
        }
    }
}

void leap_win_poll_link_and_log(LeapRawWinpcapSocket* sock)
{
    LeapRawWinpcapLinkState state;
    int                     changed = 0;

    if (sock == NULL)
    {
        return;
    }

    if (leap_raw_winpcap_poll_link(sock, &changed, &state) != 0)
    {
        return;
    }

    if (changed == 0)
    {
        return;
    }

    leap_log_printf(
        "transport: link %s (iface_up=%d transitions=%llu)\n",
        state.link_up != 0 ? "UP" : "DOWN",
        state.interface_up,
        (unsigned long long)sock->stats.link_transitions);
}

void leap_win_controller_parse_args(
    int                     argc,
    char**                  argv,
    LeapWinControllerOptions* options)
{
    int i;

    if (options == NULL)
    {
        return;
    }

    options->adapter           = NULL;
    options->lease_demo        = 0;
    options->cyclic            = 0;
    options->cyclic_period_ms  = 100u;
    options->promiscuous       = 0;
    options->exchange          = 0;
    options->stats             = 0;
    options->stats_interval    = 100u;
    options->diag              = 0;
    options->list_adapters     = 0;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--list") == 0)
        {
            options->list_adapters = 1;
        }
        else if (strcmp(argv[i], "--lease-demo") == 0)
        {
            options->lease_demo = 1;
        }
        else if (strcmp(argv[i], "--cyclic") == 0)
        {
            options->cyclic = 1;
            options->stats  = 1;
        }
        else if (strcmp(argv[i], "--cyclic-ms") == 0)
        {
            options->cyclic = 1;
            options->stats  = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->cyclic_period_ms = (unsigned)strtoul(argv[i], NULL, 10);
                if (options->cyclic_period_ms == 0u)
                {
                    options->cyclic_period_ms = 100u;
                }
            }
        }
        else if (strcmp(argv[i], "--promisc") == 0)
        {
            options->promiscuous = 1;
        }
        else if (strcmp(argv[i], "--exchange") == 0)
        {
            options->exchange = 1;
        }
        else if (strcmp(argv[i], "--stats") == 0)
        {
            options->stats = 1;
        }
        else if (strcmp(argv[i], "--stats-interval") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->stats_interval = (unsigned)strtoul(argv[i], NULL, 10);
                if (options->stats_interval == 0u)
                {
                    options->stats_interval = 100u;
                }
            }
        }
        else if (strcmp(argv[i], "--diag") == 0)
        {
            options->diag = 1;
        }
        else if (argv[i][0] != '-')
        {
            options->adapter = argv[i];
        }
    }
}

void leap_win_hub_parse_args(
    int               argc,
    char**            argv,
    LeapWinHubOptions* options)
{
    int i;

    if (options == NULL)
    {
        return;
    }

    options->adapter           = NULL;
    options->scan_ms           = LEAP_CTRL_PEER_DISCOVER_DEFAULT_SCAN_MS;
    options->cyclic_period_ms  = 100u;
    options->min_peers         = 1u;
    options->promiscuous       = 1;
    options->exchange          = 0;
    options->pacing            = 1;
    options->parallel          = 0;
    options->stats             = 1;
    options->stats_interval    = 500u;
    options->run_sec           = 0u;
    options->list_adapters     = 0;
    options->peer_mac_count    = 0u;
    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        options->peer_mac_slot[i] = -1;
    }

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--list") == 0)
        {
            options->list_adapters = 1;
        }
        else if (strcmp(argv[i], "--scan-ms") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->scan_ms = atoi(argv[i]);
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
                    options->cyclic_period_ms = 100u;
                }
            }
        }
        else if (strcmp(argv[i], "--min-peers") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->min_peers = (unsigned)strtoul(argv[i], NULL, 10);
                if (options->min_peers == 0u)
                {
                    options->min_peers = 1u;
                }
            }
        }
        else if (strcmp(argv[i], "--promisc") == 0)
        {
            options->promiscuous = 1;
        }
        else if (strcmp(argv[i], "--no-promisc") == 0)
        {
            options->promiscuous = 0;
        }
        else if (strcmp(argv[i], "--exchange") == 0)
        {
            options->exchange = 1;
        }
        else if (strcmp(argv[i], "--parallel") == 0)
        {
            options->parallel = 1;
        }
        else if (strcmp(argv[i], "--round-robin") == 0)
        {
            options->parallel = 0;
        }
        else if (strcmp(argv[i], "--no-pacing") == 0)
        {
            options->pacing = 0;
        }
        else if (strcmp(argv[i], "--pacing") == 0)
        {
            options->pacing = 1;
        }
        else if (strcmp(argv[i], "--stats") == 0)
        {
            options->stats = 1;
        }
        else if (strcmp(argv[i], "--no-stats") == 0)
        {
            options->stats = 0;
        }
        else if (strcmp(argv[i], "--stats-interval") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->stats_interval = (unsigned)strtoul(argv[i], NULL, 10);
                if (options->stats_interval == 0u)
                {
                    options->stats_interval = 500u;
                }
            }
        }
        else if (strcmp(argv[i], "--run-sec") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->run_sec = (unsigned)strtoul(argv[i], NULL, 10);
            }
        }
        else if (strcmp(argv[i], "--peer-mac") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                if (options->peer_mac_count < LEAP_CTRL_MAX_PEERS &&
                    leap_controller_peer_parse_mac(
                        argv[i],
                        options->peer_macs[options->peer_mac_count]) != 0)
                {
                    options->peer_mac_slot[options->peer_mac_count] = -1;
                    options->peer_mac_count++;
                }
            }
        }
        else if (strcmp(argv[i], "--peer-slot") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                const char* arg;
                const char* mac_text;
                char        slot_text[8];
                size_t      slot_len;
                unsigned long slot;
                int         slot_i;

                i++;
                arg = argv[i];
                mac_text = strchr(arg, ':');
                if (mac_text != NULL && mac_text != arg)
                {
                    slot_len = (size_t)(mac_text - arg);
                    if (slot_len > 0u && slot_len < sizeof(slot_text))
                    {
                        memcpy(slot_text, arg, slot_len);
                        slot_text[slot_len] = '\0';
                        slot = strtoul(slot_text, NULL, 10);
                        if (slot < (unsigned long)LEAP_CTRL_MAX_PEERS &&
                            options->peer_mac_count < LEAP_CTRL_MAX_PEERS &&
                            leap_controller_peer_parse_mac(
                                mac_text + 1,
                                options->peer_macs[options->peer_mac_count]) != 0)
                        {
                            slot_i = (int)slot;
                            options->peer_mac_slot[options->peer_mac_count] =
                                slot_i;
                            options->peer_mac_count++;
                        }
                    }
                }
            }
        }
        else if (argv[i][0] != '-')
        {
            options->adapter = argv[i];
        }
    }
}

LeapControllerPeerStatus leap_win_hub_discover_peers(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    const LeapWinHubOptions*     options)
{
    LeapControllerPeerDiscoverConfig config;
    unsigned                         i;

    if (table == NULL || io == NULL || options == NULL)
    {
        return LEAP_CTRL_PEER_INVALID_ARG;
    }

    for (i = 0u; i < options->peer_mac_count; i++)
    {
        (void)leap_controller_peer_table_probe_peer(
            table,
            io,
            options->peer_macs[i],
            LEAP_CTRL_PEER_PROBE_TIMEOUT_MS);
    }

    if (options->peer_mac_count > 0u && options->scan_ms == 0)
    {
        return LEAP_CTRL_PEER_OK;
    }

    memset(&config, 0, sizeof(config));
    config.min_peers = options->min_peers;
    if (options->scan_ms == 0)
    {
        config.scan_duration_ms = 0;
    }
    else
    {
        config.scan_duration_ms = options->scan_ms;
    }

    return leap_controller_peer_table_discover_ex(table, io, &config);
}

void leap_win_discover_parse_args(
    int                    argc,
    char**                 argv,
    LeapWinDiscoverOptions* options)
{
    int i;

    if (options == NULL)
    {
        return;
    }

    options->adapter       = NULL;
    options->scan_ms       = LEAP_CTRL_PEER_DISCOVER_DEFAULT_SCAN_MS;
    options->min_peers     = 0u;
    options->promiscuous   = 1;
    options->list_adapters = 0;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--list") == 0)
        {
            options->list_adapters = 1;
        }
        else if (strcmp(argv[i], "--scan-ms") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->scan_ms = atoi(argv[i]);
            }
        }
        else if (strcmp(argv[i], "--min-peers") == 0)
        {
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                i++;
                options->min_peers = (unsigned)strtoul(argv[i], NULL, 10);
            }
        }
        else if (strcmp(argv[i], "--promisc") == 0)
        {
            options->promiscuous = 1;
        }
        else if (strcmp(argv[i], "--no-promisc") == 0)
        {
            options->promiscuous = 0;
        }
        else if (argv[i][0] != '-')
        {
            options->adapter = argv[i];
        }
    }
}

void leap_win_device_parse_args(
    int          argc,
    char**       argv,
    const char** adapter_out,
    int*         stats_out)
{
    int i;

    if (adapter_out != NULL)
    {
        *adapter_out = NULL;
    }

    if (stats_out != NULL)
    {
        *stats_out = 0;
    }

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--stats") == 0)
        {
            if (stats_out != NULL)
            {
                *stats_out = 1;
            }
        }
        else if (argv[i][0] != '-')
        {
            if (adapter_out != NULL)
            {
                *adapter_out = argv[i];
            }
        }
    }
}
