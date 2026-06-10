/*
 * gateway_rtems_io.c — LeapControllerStackIo over BPF transport.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_rtems_io.h"

#include "gateway_config.h"
#include "leap_config.h"
#include "leap_time.h"
#include "leap_transport.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <string.h>

typedef struct LeapGatewayControllerIoCtx
{
    LeapRtemsTransport* transport;
} LeapGatewayControllerIoCtx;

static int
gateway_ctrl_send(
    void*          user_ctx,
    const uint8_t* dst_mac,
    uint8_t        flags,
    uint16_t       service_id,
    uint16_t       message_type,
    uint32_t       session_id,
    uint32_t       sequence,
    uint32_t       ack_sequence,
    const uint8_t* payload,
    size_t         payload_length)
{
    LeapGatewayControllerIoCtx* ctx = (LeapGatewayControllerIoCtx*)user_ctx;

    if (ctx == NULL || ctx->transport == NULL)
    {
        return -1;
    }

    return leap_rtems_transport_send_leap(
        ctx->transport,
        dst_mac,
        flags,
        service_id,
        message_type,
        session_id,
        sequence,
        ack_sequence,
        payload,
        payload_length);
}

static int
gateway_ctrl_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    LeapGatewayControllerIoCtx* ctx = (LeapGatewayControllerIoCtx*)user_ctx;
    uint8_t                     frame_buf[LEAP_MAX_FRAME_BYTES];
    size_t                      payload_len = 0u;
    int                         rc;

    (void)parsed;

    if (ctx == NULL || ctx->transport == NULL || payload_length == NULL)
    {
        return -1;
    }

    rc = leap_rtems_transport_recv(
        ctx->transport,
        src_mac,
        frame_buf,
        sizeof(frame_buf),
        &payload_len,
        timeout_ms);
    if (rc != 0)
    {
        return rc;
    }

    if (payload_len > payload_capacity)
    {
        return -1;
    }

    memcpy(payload_buf, frame_buf, payload_len);
    *payload_length = payload_len;
    return 0;
}

static uint64_t
gateway_ctrl_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_rtems_monotonic_us();
}

void
leap_gateway_controller_io_init(
    LeapControllerStackIo* io,
    LeapRtemsTransport*    transport)
{
    static LeapGatewayControllerIoCtx ctx;

    if (io == NULL)
    {
        return;
    }

    ctx.transport = transport;
    memset(io, 0, sizeof(*io));
    io->user_ctx     = &ctx;
    io->send_frame   = gateway_ctrl_send;
    io->recv_frame   = gateway_ctrl_recv;
    io->monotonic_us = gateway_ctrl_monotonic;
}
