/*
 * leap_linux_controller_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_controller_io.h"

#include "leap_linux_common.h"

#include <string.h>

typedef struct LeapLinuxControllerIoCtx
{
    LeapRawLinuxSocket* sock;
} LeapLinuxControllerIoCtx;

static int leap_linux_ctrl_io_send(
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
    LeapLinuxControllerIoCtx* ctx = (LeapLinuxControllerIoCtx*)user_ctx;

    if (ctx == NULL || ctx->sock == NULL)
    {
        return -1;
    }

    return leap_linux_send_leap_retry(
        ctx->sock,
        dst_mac,
        flags,
        service_id,
        message_type,
        session_id,
        sequence,
        ack_sequence,
        payload,
        payload_length,
        3);
}

static int leap_linux_ctrl_io_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    LeapLinuxControllerIoCtx* ctx = (LeapLinuxControllerIoCtx*)user_ctx;

    (void)parsed;

    if (ctx == NULL || ctx->sock == NULL || payload_length == NULL)
    {
        return -1;
    }

    return leap_linux_recv_leap(
        ctx->sock,
        src_mac,
        payload_buf,
        payload_capacity,
        payload_length,
        timeout_ms);
}

static uint64_t leap_linux_ctrl_io_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_linux_monotonic_us();
}

void leap_linux_controller_io_init(
    LeapControllerStackIo* io,
    LeapRawLinuxSocket*    sock)
{
    static LeapLinuxControllerIoCtx ctx;

    if (io == NULL)
    {
        return;
    }

    ctx.sock = sock;
    memset(io, 0, sizeof(*io));
    io->user_ctx     = &ctx;
    io->send_frame   = leap_linux_ctrl_io_send;
    io->recv_frame   = leap_linux_ctrl_io_recv;
    io->monotonic_us = leap_linux_ctrl_io_monotonic;
}
