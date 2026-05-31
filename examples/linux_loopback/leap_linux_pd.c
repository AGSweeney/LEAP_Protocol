/*
 * leap_linux_pd.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_pd.h"

#include "leap_linux_common.h"

#include "leap/leap_frame.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_protocol.h"

#include <string.h>

static int leap_linux_pd_send_pd(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapLinuxPdTransport* transport = (LeapLinuxPdTransport*)user_ctx;

    if (transport == NULL || transport->sock == NULL)
    {
        return -1;
    }

    return leap_linux_send_leap_retry(
        transport->sock,
        peer_mac,
        0u,
        (uint16_t)LEAP_SERVICE_PD,
        message_type,
        session_id,
        sequence,
        0u,
        payload,
        payload_length,
        3);
}

static int leap_linux_pd_send_heartbeat(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapLinuxPdTransport* transport = (LeapLinuxPdTransport*)user_ctx;
    uint8_t               payload[sizeof(LeapHeartbeatPayload)];
    size_t                payload_length;

    if (transport == NULL || transport->mgmt == NULL)
    {
        return -1;
    }

    payload_length = leap_mgmt_controller_build_heartbeat(
        transport->mgmt,
        payload,
        sizeof(payload));
    if (payload_length == 0u)
    {
        return -1;
    }

    return leap_linux_send_leap_retry(
        transport->sock,
        peer_mac,
        0u,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_HEARTBEAT,
        session_id,
        sequence,
        0u,
        payload,
        payload_length,
        3);
}

static int leap_linux_pd_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms)
{
    LeapLinuxPdTransport* transport = (LeapLinuxPdTransport*)user_ctx;
    LeapFrameView         view;
    uint8_t                 src_mac[6];

    (void)peer_mac;

    if (transport == NULL || transport->sock == NULL || reply_length == NULL)
    {
        return -1;
    }

    if (leap_linux_recv_leap(
            transport->sock,
            src_mac,
            reply_payload,
            reply_capacity,
            reply_length,
            timeout_ms) != 0)
    {
        return -1;
    }

    if (leap_frame_parse(reply_payload, *reply_length, &view) != LEAP_FRAME_OK)
    {
        return -1;
    }

    if (view.header.service_id != (uint16_t)LEAP_SERVICE_PD ||
        view.header.message_type != LEAP_PD_EXCHANGE_REPLY)
    {
        return -1;
    }

    *reply_length = view.payload_length;
    memmove(reply_payload, view.payload, view.payload_length);
    return 0;
}

static uint64_t leap_linux_pd_monotonic_us(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_linux_monotonic_us();
}

void leap_linux_pd_init_io(
    LeapPdControllerIo*    io,
    LeapLinuxPdTransport* transport)
{
    if (io == NULL)
    {
        return;
    }

    memset(io, 0, sizeof(*io));
    io->user_ctx             = transport;
    io->send_pd              = leap_linux_pd_send_pd;
    io->send_heartbeat       = leap_linux_pd_send_heartbeat;
    io->wait_exchange_reply  = leap_linux_pd_wait_exchange_reply;
    io->monotonic_us         = leap_linux_pd_monotonic_us;
}
