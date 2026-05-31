/*
 * examples/linux_loopback/controller_main.c
 *
 * Linux AF_PACKET LEAP controller: broadcast HELLO, wait for HELLO_REPLY.
 *
 * Usage: sudo ./leap_linux_controller [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"
#include "leap/leap_raw_linux.h"

#include <stdio.h>
#include <string.h>

#define LEAP_RX_BUF_SIZE 1600u
#define LEAP_TX_BUF_SIZE 256u

static const uint8_t k_bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

int main(int argc, char** argv)
{
    const char*        ifname = "lo";
    LeapRawLinuxSocket transport;
    LeapHelloRequest   hello;
    uint8_t            tx[LEAP_TX_BUF_SIZE];
    uint8_t            rx[LEAP_RX_BUF_SIZE];
    uint8_t            src_mac[6];
    size_t             tx_len = 0u;
    size_t             rx_len = 0u;
    LeapFrameView      view;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_controller requires Linux AF_PACKET support.\n");
    return 1;
#else
    if (argc > 1)
    {
        ifname = argv[1];
    }

    if (leap_raw_linux_open(&transport, ifname, LEAP_ETHERTYPE_DEVELOPMENT) != 0)
    {
        fprintf(stderr, "failed to open raw socket on %s (need CAP_NET_RAW/root)\n", ifname);
        return 1;
    }

    memset(&hello, 0, sizeof(hello));
    if (leap_frame_write(
            tx,
            sizeof(tx),
            &tx_len,
            LEAP_FLAG_BROADCAST,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            1u,
            0u,
            (const uint8_t*)&hello,
            sizeof(hello)) != 0)
    {
        fprintf(stderr, "failed to build HELLO frame\n");
        return 1;
    }

    if (leap_raw_linux_send(&transport, k_bcast, tx, tx_len) != 0)
    {
        fprintf(stderr, "failed to send HELLO\n");
        return 1;
    }

    printf("sent HELLO on %s, waiting for HELLO_REPLY...\n", ifname);

    if (leap_raw_linux_recv(&transport, src_mac, rx, sizeof(rx), &rx_len, 3000) != 0)
    {
        fprintf(stderr, "timeout waiting for HELLO_REPLY\n");
        leap_raw_linux_close(&transport);
        return 1;
    }

    if (leap_frame_parse(rx, rx_len, &view) != LEAP_FRAME_OK)
    {
        fprintf(stderr, "invalid HELLO_REPLY frame\n");
        leap_raw_linux_close(&transport);
        return 1;
    }

    if (view.header.service_id != (uint16_t)LEAP_SERVICE_DISC ||
        view.header.message_type != LEAP_DISC_HELLO_REPLY)
    {
        fprintf(stderr, "unexpected reply service/message\n");
        leap_raw_linux_close(&transport);
        return 1;
    }

    printf("received HELLO_REPLY from device, state=%u payload=%u bytes\n",
           (unsigned)view.header.flags,
           (unsigned)view.payload_length);

    leap_raw_linux_close(&transport);
    return 0;
#endif
}
