/*
 * leap_linux_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_frame.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_leap_linux_send_retries = 0u;

uint64_t leap_linux_send_retry_count(void)
{
    return g_leap_linux_send_retries;
}

void leap_linux_reset_send_retry_count(void)
{
    g_leap_linux_send_retries = 0u;
}

#include <string.h>

#define LEAP_LINUX_TX_BUF 1600u

int leap_linux_link_stop_on_down(
    LeapRawLinuxSocket* sock,
    volatile int*       stop_flag)
{
    int                   changed = 0;
    LeapRawLinuxLinkState state;

    if (sock == NULL || stop_flag == NULL)
    {
        return 0;
    }

    if (leap_raw_linux_poll_link(sock, &changed, &state) != 0)
    {
        return 0;
    }

    if (changed != 0)
    {
        leap_log_printf(
            "transport: link %s (iface_up=%d carrier_up=%d transitions=%llu)\n",
            state.link_up != 0 ? "UP" : "DOWN",
            state.interface_up,
            state.carrier_up,
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

LeapPdControllerStatus leap_linux_controller_run_cyclic_pd_with_link_watch(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    LeapRawLinuxSocket*       transport,
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
        (void)leap_linux_link_stop_on_down(transport, stop_flag);
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
    leap_pd_controller_log_stats(&stack->pd);
    return LEAP_PD_CTRL_OK;
}

LeapPdControllerStatus leap_linux_hub_run_round_robin_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawLinuxSocket*       transport,
    volatile int*             stop_flag)
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

    leap_log_printf("hub round-robin PD - Ctrl+C or link-down to stop\n");

    while (*stop_flag == 0)
    {
        (void)leap_linux_link_stop_on_down(transport, stop_flag);
        if (*stop_flag != 0)
        {
            break;
        }

        ran_any = 0;

        for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
        {
            if (hub->slots[i].in_use == 0)
            {
                continue;
            }

            if (leap_controller_stack_get_phase(&hub->slots[i].stack) !=
                LEAP_CTRL_STACK_OP)
            {
                continue;
            }

            status = leap_controller_session_hub_run_one_cycle(
                hub,
                (int)i,
                pd_io,
                stop_flag);
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

void leap_linux_print_mac(const char* label, const uint8_t* mac)
{
    if (label != NULL)
    {
        printf("%s", label);
    }

    if (mac == NULL)
    {
        printf("(null)\n");
        return;
    }

    printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
           mac[0],
           mac[1],
           mac[2],
           mac[3],
           mac[4],
           mac[5]);
}

void leap_linux_print_transport_error(const char* action)
{
    int err = leap_raw_linux_last_errno();

    if (err != 0)
    {
        fprintf(stderr, "%s failed: %s\n", action, strerror(err));
        if (err == ENODEV && strcmp(action, "open") == 0)
        {
            if (getenv("CI") != NULL)
            {
                fprintf(stderr,
                        "hint: this host rejects AF_PACKET on loopback (ENODEV); "
                        "CI smoke tests use an isolated veth bridge\n");
            }
            else
            {
                fprintf(stderr,
                        "hint: AF_PACKET bind failed (ENODEV) — common on WSL2; "
                        "use native Linux, a VM, or veth (see examples/linux_loopback/README.md)\n");
            }
        }
    }
    else
    {
        fprintf(stderr, "%s failed\n", action);
    }
}

void leap_linux_print_transport_stats(const LeapRawLinuxSocket* sock)
{
    LeapRawLinuxStats stats;

    if (sock == NULL)
    {
        return;
    }

    leap_raw_linux_get_stats(sock, &stats);
    printf(
        "transport: tx_ok=%llu tx_err=%llu tx_partial=%llu "
        "rx_ok=%llu rx_filtered=%llu rx_timeout=%llu rx_err=%llu rx_short=%llu "
        "link_xitions=%llu\n",
        (unsigned long long)stats.tx_frames_ok,
        (unsigned long long)stats.tx_errors,
        (unsigned long long)stats.tx_partial_chunks,
        (unsigned long long)stats.rx_frames_ok,
        (unsigned long long)stats.rx_filtered,
        (unsigned long long)stats.rx_timeouts,
        (unsigned long long)stats.rx_errors,
        (unsigned long long)stats.rx_short_frames,
        (unsigned long long)stats.link_transitions);
}

void leap_linux_poll_link_and_log(LeapRawLinuxSocket* sock)
{
    LeapRawLinuxLinkState state;
    int                   changed = 0;

    if (sock == NULL)
    {
        return;
    }

    if (leap_raw_linux_poll_link(sock, &changed, &state) != 0)
    {
        return;
    }

    if (changed == 0)
    {
        return;
    }

    printf(
        "transport: link %s (iface_up=%d carrier_up=%d transitions=%llu)\n",
        state.link_up != 0 ? "UP" : "DOWN",
        state.interface_up,
        state.carrier_up,
        (unsigned long long)sock->stats.link_transitions);
}

int leap_linux_send_leap(
    LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    uint8_t                   flags,
    uint16_t                  service_id,
    uint16_t                  message_type,
    uint32_t                  session_id,
    uint32_t                  sequence,
    uint32_t                  ack_sequence,
    const uint8_t*            payload,
    size_t                    payload_length)
{
    uint8_t tx[LEAP_LINUX_TX_BUF];
    size_t  tx_len = 0u;

    if (leap_frame_write(
            tx,
            sizeof(tx),
            &tx_len,
            flags,
            service_id,
            message_type,
            session_id,
            sequence,
            ack_sequence,
            payload,
            payload_length) != 0)
    {
        fprintf(stderr, "failed to build LEAP frame (service=0x%04X msg=0x%04X)\n",
                service_id,
                message_type);
        return -1;
    }

    if (leap_raw_linux_send(sock, dst_mac, tx, tx_len) != 0)
    {
        leap_linux_print_transport_error("send");
        return -1;
    }

    return 0;
}

int leap_linux_send_leap_retry(
    LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    uint8_t                   flags,
    uint16_t                  service_id,
    uint16_t                  message_type,
    uint32_t                  session_id,
    uint32_t                  sequence,
    uint32_t                  ack_sequence,
    const uint8_t*            payload,
    size_t                    payload_length,
    int                       max_attempts)
{
    int attempt;

    if (max_attempts < 1)
    {
        max_attempts = 1;
    }

    for (attempt = 0; attempt < max_attempts; attempt++)
    {
        if (attempt > 0)
        {
            g_leap_linux_send_retries++;
        }

        if (leap_linux_send_leap(
                sock,
                dst_mac,
                flags,
                service_id,
                message_type,
                session_id,
                sequence,
                ack_sequence,
                payload,
                payload_length) == 0)
        {
            return 0;
        }
    }

    return -1;
}

void leap_linux_controller_parse_args(
    int                        argc,
    char**                     argv,
    LeapLinuxControllerOptions* options)
{
    int i;

    if (options == NULL)
    {
        return;
    }

    options->ifname           = "lo";
    options->lease_demo       = 0;
    options->cyclic           = 0;
    options->cyclic_period_ms = 100u;
    options->promiscuous      = 0;
    options->exchange         = 0;
    options->stats            = 0;
    options->stats_interval   = 100u;
    options->diag             = 0;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--lease-demo") == 0)
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
            options->ifname = argv[i];
        }
    }
}

int leap_linux_recv_leap(
    LeapRawLinuxSocket* sock,
    uint8_t*                  src_mac,
    uint8_t*                  payload,
    size_t                    payload_capacity,
    size_t*                   payload_length,
    int                       timeout_ms)
{
    if (leap_raw_linux_recv(
            sock,
            src_mac,
            payload,
            payload_capacity,
            payload_length,
            timeout_ms) != 0)
    {
        if (leap_raw_linux_last_errno() != 0)
        {
            leap_linux_print_transport_error("recv");
        }
        return -1;
    }

    return 0;
}
