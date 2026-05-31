/*
 * examples/linux_loopback/device_main.c
 *
 * Linux AF_PACKET LEAP device responder (HELLO -> HELLO_REPLY).
 *
 * Usage: sudo ./leap_linux_device [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_device_stack.h"
#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"
#include "leap/leap_raw_linux.h"

#include <stdio.h>
#include <string.h>

#define LEAP_RX_BUF_SIZE 1600u
#define LEAP_TX_BUF_SIZE 1600u

int main(int argc, char** argv)
{
    const char*           ifname = "lo";
    LeapRawLinuxSocket    transport;
    LeapDeviceStack       stack;
    LeapDeviceStackResult result;
    uint8_t               rx[LEAP_RX_BUF_SIZE];
    uint8_t               tx[LEAP_TX_BUF_SIZE];
    uint8_t               src_mac[6];
    size_t                rx_len = 0u;
    size_t                tx_len = 0u;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_device requires Linux AF_PACKET support.\n");
    return 1;
#else
    if (argc > 1)
    {
        ifname = argv[1];
    }

    leap_device_stack_init(&stack, NULL);

    if (leap_raw_linux_open(&transport, ifname, LEAP_ETHERTYPE_DEVELOPMENT) != 0)
    {
        fprintf(stderr, "failed to open raw socket on %s (need CAP_NET_RAW/root)\n", ifname);
        return 1;
    }

    memcpy(stack.disc.config.identity.primary_mac, transport.local_mac, 6);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);

    printf("LEAP linux device listening on %s\n", ifname);

    for (;;)
    {
        if (leap_raw_linux_recv(&transport, src_mac, rx, sizeof(rx), &rx_len, 1000) != 0)
        {
            continue;
        }

        if (leap_device_stack_process_frame(
                &stack, src_mac, 0u, rx, rx_len, &result) != LEAP_DEVICE_STACK_OK)
        {
            continue;
        }

        if ((result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) == 0u)
        {
            continue;
        }

        if (leap_frame_write(
                tx,
                sizeof(tx),
                &tx_len,
                (uint8_t)(LEAP_FLAG_RESPONSE),
                (uint16_t)LEAP_SERVICE_DISC,
                result.disc_message_type,
                result.frame.header.session_id,
                result.frame.header.sequence,
                result.frame.header.ack_sequence,
                result.disc_payload,
                result.disc_payload_length) != 0)
        {
            continue;
        }

        if (leap_raw_linux_send(&transport, src_mac, tx, tx_len) == 0)
        {
            printf("sent HELLO_REPLY\n");
        }
    }
#endif
}
