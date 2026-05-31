/*
 * leap_win_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_win_common.h"

#include "leap/leap_frame.h"
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
        printf(
            "transport: link %s (iface_up=%d transitions=%llu)\n",
            state.link_up != 0 ? "UP" : "DOWN",
            state.interface_up,
            (unsigned long long)sock->stats.link_transitions);
    }

    if (state.link_up == 0)
    {
        if (*stop_flag == 0)
        {
            printf("transport: link down - stopping PD\n");
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

    printf(
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

    printf("cyclic PD stopped\n");
    leap_pd_controller_log_stats(&stack->pd);
    return LEAP_PD_CTRL_OK;
}

void leap_win_print_transport_error(const char* action)
{
    const char* err = leap_raw_winpcap_last_error();
    int         win_err = leap_raw_winpcap_last_errno();

    if (err != NULL && err[0] != '\0')
    {
        fprintf(stderr, "%s failed: %s", action, err);
        if (win_err != 0)
        {
            fprintf(stderr, " (win32 err=%d)", win_err);
        }
        fprintf(stderr, "\n");
    }
    else if (win_err != 0)
    {
        fprintf(stderr, "%s failed (win32 err=%d)\n", action, win_err);
    }
    else
    {
        fprintf(stderr, "%s failed\n", action);
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
    printf(
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
            printf(
                "transport: last_rx eth_cap=%u svc=0x%04X msg=0x%04X leap_len=%zu "
                "(expect eth_cap~100 leap_len~86 HELLO_REPLY)\n",
                (unsigned)sock->last_rx_eth_caplen,
                (unsigned)sock->last_rx_service_id,
                (unsigned)sock->last_rx_message_type,
                sock->last_rx_payload_len);
        }
        else
        {
            printf(
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
            printf(
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

    printf(
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
