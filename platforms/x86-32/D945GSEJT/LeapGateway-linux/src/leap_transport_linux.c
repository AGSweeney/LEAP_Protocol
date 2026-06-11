/*
 * leap_transport_linux.c — LeapRtemsTransport API over leap_raw_linux (AF_PACKET).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_transport.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#define LEAP_LINUX_TX_BUF 1600u

int
leap_rtems_transport_init(
    LeapRtemsTransport* transport,
    const char*         ifname,
    uint16_t            ethertype)
{
    LeapRawLinuxOpenOptions options;

    if (transport == NULL || ifname == NULL)
    {
        return -1;
    }

    memset(transport, 0, sizeof(*transport));
    memset(&options, 0, sizeof(options));
    options.promiscuous     = 0;
    options.filter_dest_mac = 1;

    if (leap_raw_linux_open_ex(&transport->sock, ifname, ethertype, &options) != 0)
    {
        return -1;
    }

    transport->ethertype = ethertype;
    transport->link_up   = 1;
    memcpy(transport->local_mac, transport->sock.local_mac, LEAP_RTEMS_MAC_LEN);
    snprintf(transport->ifname, sizeof(transport->ifname), "%s", ifname);
    return 0;
}

int
leap_rtems_transport_init_auto(LeapRtemsTransport* transport, uint16_t ethertype)
{
    static const char* const candidates[] = { "eth0", "eth1", "eth2", NULL };
    size_t i;

    for (i = 0; candidates[i] != NULL; ++i)
    {
        if (leap_rtems_transport_init(transport, candidates[i], ethertype) == 0)
        {
            return 0;
        }
    }

    return -1;
}

void
leap_rtems_transport_close(LeapRtemsTransport* transport)
{
    if (transport == NULL)
    {
        return;
    }

    leap_raw_linux_close(&transport->sock);
}

int
leap_rtems_transport_recv(
    LeapRtemsTransport* transport,
    uint8_t*            src_mac_out,
    uint8_t*            payload_out,
    size_t              payload_capacity,
    size_t*             payload_len_out,
    int                 timeout_ms)
{
    if (transport == NULL || payload_len_out == NULL)
    {
        return -1;
    }

    return leap_raw_linux_recv(
        &transport->sock,
        src_mac_out,
        payload_out,
        payload_capacity,
        payload_len_out,
        timeout_ms);
}

int
leap_rtems_transport_send_leap(
    LeapRtemsTransport* transport,
    const uint8_t*      dst_mac,
    uint8_t             flags,
    uint16_t            service_id,
    uint16_t            message_type,
    uint32_t            session_id,
    uint32_t            sequence,
    uint32_t            ack_sequence,
    const uint8_t*      payload,
    size_t              payload_length)
{
    uint8_t tx[LEAP_LINUX_TX_BUF];
    size_t  tx_len = 0u;

    if (transport == NULL || dst_mac == NULL)
    {
        return -1;
    }

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
        return -1;
    }

    return leap_raw_linux_send(&transport->sock, dst_mac, tx, tx_len);
}

int
leap_rtems_transport_poll_link(LeapRtemsTransport* transport)
{
    int                   changed = 0;
    LeapRawLinuxLinkState state;

    if (transport == NULL)
    {
        return -1;
    }

    if (leap_raw_linux_poll_link(&transport->sock, &changed, &state) != 0)
    {
        return -1;
    }

    transport->link_up = state.link_up;
    return state.link_up;
}
