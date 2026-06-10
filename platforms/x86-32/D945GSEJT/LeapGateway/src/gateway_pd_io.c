/*
 * gateway_pd_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_pd_io.h"

#include "leap_time.h"
#include "leap_transport.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <string.h>

typedef struct LeapGatewayPdTransport
{
    LeapRtemsTransport* transport;
} LeapGatewayPdTransport;

static int
gateway_pd_send_pd(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapGatewayPdTransport* ctx = (LeapGatewayPdTransport*)user_ctx;

    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return leap_rtems_transport_send_leap(
        ctx->transport,
        peer_mac,
        0u,
        (uint16_t)LEAP_SERVICE_PD,
        message_type,
        session_id,
        sequence,
        0u,
        payload,
        payload_length);
}

static int
gateway_pd_send_heartbeat(
    void*          user_ctx,
    const uint8_t* peer_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapGatewayPdTransport* ctx = (LeapGatewayPdTransport*)user_ctx;

    if (ctx == NULL || ctx->transport == NULL || payload == NULL)
    {
        return -1;
    }

    return leap_rtems_transport_send_leap(
        ctx->transport,
        peer_mac,
        0u,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_HEARTBEAT,
        session_id,
        sequence,
        0u,
        payload,
        payload_length);
}

static int
gateway_pd_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms,
    uint64_t*      reply_recv_us_out)
{
    LeapGatewayPdTransport* ctx = (LeapGatewayPdTransport*)user_ctx;
    LeapFrameView           view;
    uint8_t                 src_mac[6];
    uint8_t                 frame_buf[256];
    size_t                  frame_length;
    int                     rc;

    if (ctx == NULL || ctx->transport == NULL || reply_length == NULL ||
        peer_mac == NULL)
    {
        return -1;
    }

    if (reply_recv_us_out != NULL)
    {
        *reply_recv_us_out = 0u;
    }

    for (;;)
    {
        rc = leap_rtems_transport_recv(
            ctx->transport,
            src_mac,
            frame_buf,
            sizeof(frame_buf),
            &frame_length,
            timeout_ms);
        if (rc != 0)
        {
            return -1;
        }

        if (memcmp(src_mac, peer_mac, 6) != 0)
        {
            continue;
        }

        if (leap_frame_parse(frame_buf, frame_length, &view) != LEAP_FRAME_OK)
        {
            continue;
        }

        if (view.header.service_id != (uint16_t)LEAP_SERVICE_PD ||
            view.header.message_type != LEAP_PD_EXCHANGE_REPLY)
        {
            continue;
        }

        if (view.payload_length > reply_capacity)
        {
            return -1;
        }

        *reply_length = view.payload_length;
        memcpy(reply_payload, view.payload, view.payload_length);
        if (reply_recv_us_out != NULL)
        {
            *reply_recv_us_out = leap_rtems_monotonic_us();
        }
        return 0;
    }
}

static uint64_t
gateway_pd_monotonic_us(void* user_ctx)
{
    (void)user_ctx;
    return leap_rtems_monotonic_us();
}

void
leap_gateway_pd_io_init(LeapPdControllerIo* io, LeapRtemsTransport* transport)
{
    static LeapGatewayPdTransport ctx;

    if (io == NULL)
    {
        return;
    }

    ctx.transport = transport;
    memset(io, 0, sizeof(*io));
    io->user_ctx            = &ctx;
    io->send_pd             = gateway_pd_send_pd;
    io->send_heartbeat      = gateway_pd_send_heartbeat;
    io->wait_exchange_reply = gateway_pd_wait_exchange_reply;
    io->monotonic_us        = gateway_pd_monotonic_us;
}
