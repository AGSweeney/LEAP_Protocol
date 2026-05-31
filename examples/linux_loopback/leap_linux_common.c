/*
 * leap_linux_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEAP_LINUX_TX_BUF 1600u

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
            fprintf(stderr,
                    "hint: WSL2 cannot bind AF_PACKET — use native Linux or a VM "
                    "(see examples/linux_loopback/README.md)\n");
        }
    }
    else
    {
        fprintf(stderr, "%s failed\n", action);
    }
}

int leap_linux_send_leap(
    const LeapRawLinuxSocket* sock,
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
    const LeapRawLinuxSocket* sock,
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
        else if (argv[i][0] != '-')
        {
            options->ifname = argv[i];
        }
    }
}

int leap_linux_recv_leap(
    const LeapRawLinuxSocket* sock,
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
