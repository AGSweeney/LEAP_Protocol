/*
 * leap_transport.h — Linux AF_PACKET transport (RTEMS-compatible API).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_TRANSPORT_H
#define LEAP_RTEMS_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_raw_linux.h"

#define LEAP_RTEMS_MAC_LEN 6u

typedef struct LeapRtemsTransport
{
    LeapRawLinuxSocket sock;
    int                link_up;
    uint16_t           ethertype;
    uint8_t            local_mac[LEAP_RTEMS_MAC_LEN];
    char               ifname[16];
} LeapRtemsTransport;

int  leap_rtems_transport_init(LeapRtemsTransport* transport, const char* ifname, uint16_t ethertype);
int  leap_rtems_transport_init_auto(LeapRtemsTransport* transport, uint16_t ethertype);
void leap_rtems_transport_close(LeapRtemsTransport* transport);

int leap_rtems_transport_recv(
    LeapRtemsTransport* transport,
    uint8_t*            src_mac_out,
    uint8_t*            payload_out,
    size_t              payload_capacity,
    size_t*             payload_len_out,
    int                 timeout_ms);

int leap_rtems_transport_send_leap(
    LeapRtemsTransport* transport,
    const uint8_t*      dst_mac,
    uint8_t             flags,
    uint16_t            service_id,
    uint16_t            message_type,
    uint32_t            session_id,
    uint32_t            sequence,
    uint32_t            ack_sequence,
    const uint8_t*      payload,
    size_t              payload_length);

int leap_rtems_transport_poll_link(LeapRtemsTransport* transport);

#endif /* LEAP_RTEMS_TRANSPORT_H */
