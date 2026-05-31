/*
 * leap_win_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_win_io.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#define LEAP_WIN_TX_BUF 1600u

static const uint8_t k_leap_win_bcast_mac[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

typedef struct LeapWinIoCtx
{
    LeapRawWinpcapSocket* sock;
    int                   loopback_mirror;
} LeapWinIoCtx;

static LeapWinSharedTransport* g_win_active_transport;

void leap_win_shared_transport_init(LeapWinSharedTransport* transport)
{
    if (transport == NULL)
    {
        return;
    }

    memset(transport, 0, sizeof(*transport));
    InitializeCriticalSection(&transport->lock);
    transport->lock_ready = 1;
    g_win_active_transport = transport;
}

void leap_win_shared_transport_shutdown(LeapWinSharedTransport* transport)
{
    if (transport == NULL)
    {
        return;
    }

    if (transport->lock_ready != 0)
    {
        DeleteCriticalSection(&transport->lock);
        transport->lock_ready = 0;
    }

    leap_raw_winpcap_close(&transport->sock);
    g_win_active_transport = NULL;
}

int leap_win_send_leap(
    LeapRawWinpcapSocket* sock,
    const uint8_t*        dst_mac,
    uint8_t               flags,
    uint16_t              service_id,
    uint16_t              message_type,
    uint32_t              session_id,
    uint32_t              sequence,
    uint32_t              ack_sequence,
    const uint8_t*        payload,
    size_t                payload_length)
{
    uint8_t tx[LEAP_WIN_TX_BUF];
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
        return -1;
    }

    return leap_raw_winpcap_send(sock, dst_mac, tx, tx_len);
}

int leap_win_shared_send_leap(
    LeapWinSharedTransport* transport,
    const uint8_t*          dst_mac,
    uint8_t                 flags,
    uint16_t                service_id,
    uint16_t                message_type,
    uint32_t                session_id,
    uint32_t                sequence,
    uint32_t                ack_sequence,
    const uint8_t*          payload,
    size_t                  payload_length)
{
    int result;

    if (transport == NULL)
    {
        return -1;
    }

    EnterCriticalSection(&transport->lock);
    result = leap_win_send_leap(
        &transport->sock,
        dst_mac,
        flags,
        service_id,
        message_type,
        session_id,
        sequence,
        ack_sequence,
        payload,
        payload_length);
    LeaveCriticalSection(&transport->lock);
    return result;
}

int leap_win_recv_leap(
    LeapRawWinpcapSocket* sock,
    uint8_t*              src_mac,
    uint8_t*              payload,
    size_t                payload_capacity,
    size_t*               payload_length,
    int                   timeout_ms)
{
    return leap_raw_winpcap_recv(
        sock,
        src_mac,
        payload,
        payload_capacity,
        payload_length,
        timeout_ms);
}

int leap_win_shared_recv_leap(
    LeapWinSharedTransport* transport,
    uint8_t*                src_mac,
    uint8_t*                payload,
    size_t                  payload_capacity,
    size_t*                 payload_length,
    int                     timeout_ms)
{
    int result;

    if (transport == NULL)
    {
        return -1;
    }

    EnterCriticalSection(&transport->lock);
    result = leap_win_recv_leap(
        &transport->sock,
        src_mac,
        payload,
        payload_capacity,
        payload_length,
        timeout_ms);
    LeaveCriticalSection(&transport->lock);
    return result;
}

void leap_win_print_mac(const char* label, const uint8_t* mac)
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

    printf(
        "%02x:%02x:%02x:%02x:%02x:%02x\n",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

static int leap_win_ctrl_io_send(
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
    LeapWinSharedTransport* transport = (LeapWinSharedTransport*)user_ctx;

    return leap_win_shared_send_leap(
        transport,
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

static int leap_win_ctrl_io_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    LeapWinSharedTransport* transport = (LeapWinSharedTransport*)user_ctx;

    (void)parsed;

    if (transport == NULL || payload_length == NULL)
    {
        return -1;
    }

    return leap_win_shared_recv_leap(
        transport,
        src_mac,
        payload_buf,
        payload_capacity,
        payload_length,
        timeout_ms);
}

static uint64_t leap_win_ctrl_io_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

static int leap_win_pd_send(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapWinSharedTransport* transport = (LeapWinSharedTransport*)user_ctx;

    return leap_win_shared_send_leap(
        transport,
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

static int leap_win_pd_send_heartbeat(
    void*          user_ctx,
    const uint8_t* peer_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapWinSharedTransport* transport = (LeapWinSharedTransport*)user_ctx;

    return leap_win_shared_send_leap(
        transport,
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

static int leap_win_pd_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms)
{
    LeapWinSharedTransport* transport = (LeapWinSharedTransport*)user_ctx;
    LeapFrameView           view;
    uint8_t                 src_mac[6];
    uint8_t                 frame_buf[512];
    size_t                  frame_length;

    if (transport == NULL || reply_length == NULL || peer_mac == NULL)
    {
        return -1;
    }

    for (;;)
    {
        if (leap_win_shared_recv_leap(
                transport,
                src_mac,
                frame_buf,
                sizeof(frame_buf),
                &frame_length,
                timeout_ms) != 0)
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
        return 0;
    }
}

static uint64_t leap_win_pd_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

void leap_win_controller_io_init_shared(
    LeapControllerStackIo*  io,
    LeapWinSharedTransport* transport)
{
    if (io == NULL)
    {
        return;
    }

    memset(io, 0, sizeof(*io));
    io->user_ctx     = transport;
    io->send_frame   = leap_win_ctrl_io_send;
    io->recv_frame   = leap_win_ctrl_io_recv;
    io->monotonic_us = leap_win_ctrl_io_monotonic;
}

void leap_win_pd_init_io_shared(
    LeapPdControllerIo*     pd_io,
    LeapWinSharedTransport* transport)
{
    if (pd_io == NULL)
    {
        return;
    }

    memset(pd_io, 0, sizeof(*pd_io));
    pd_io->user_ctx            = transport;
    pd_io->send_pd             = leap_win_pd_send;
    pd_io->send_heartbeat      = leap_win_pd_send_heartbeat;
    pd_io->wait_exchange_reply = leap_win_pd_wait_exchange_reply;
    pd_io->monotonic_us        = leap_win_pd_monotonic;
}

static int leap_win_plain_ctrl_send(
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
    LeapWinIoCtx* ctx = (LeapWinIoCtx*)user_ctx;
    int           result;

    if (ctx == NULL || ctx->sock == NULL)
    {
        return -1;
    }

    result = leap_win_send_leap(
        ctx->sock,
        dst_mac,
        flags,
        service_id,
        message_type,
        session_id,
        sequence,
        ack_sequence,
        payload,
        payload_length);
    if (result != 0)
    {
        return result;
    }

    if (ctx->loopback_mirror != 0 && dst_mac != NULL &&
        memcmp(dst_mac, k_leap_win_bcast_mac, 6) == 0)
    {
        (void)leap_win_send_leap(
            ctx->sock,
            ctx->sock->local_mac,
            flags,
            service_id,
            message_type,
            session_id,
            sequence,
            ack_sequence,
            payload,
            payload_length);
    }

    return 0;
}

static int leap_win_plain_ctrl_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    LeapWinIoCtx* ctx = (LeapWinIoCtx*)user_ctx;

    (void)parsed;

    if (ctx == NULL || ctx->sock == NULL || payload_length == NULL)
    {
        return -1;
    }

    return leap_win_recv_leap(
        ctx->sock,
        src_mac,
        payload_buf,
        payload_capacity,
        payload_length,
        timeout_ms);
}

static uint64_t leap_win_plain_ctrl_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

static int leap_win_plain_pd_send(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapWinIoCtx* ctx = (LeapWinIoCtx*)user_ctx;

    if (ctx == NULL || ctx->sock == NULL)
    {
        return -1;
    }

    return leap_win_send_leap(
        ctx->sock,
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

static int leap_win_plain_pd_send_heartbeat(
    void*          user_ctx,
    const uint8_t* peer_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapWinIoCtx* ctx = (LeapWinIoCtx*)user_ctx;

    if (ctx == NULL || ctx->sock == NULL)
    {
        return -1;
    }

    return leap_win_send_leap(
        ctx->sock,
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

static int leap_win_plain_pd_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms)
{
    LeapWinIoCtx* ctx = (LeapWinIoCtx*)user_ctx;
    LeapFrameView view;
    uint8_t       src_mac[6];
    uint8_t       frame_buf[512];
    size_t        frame_length;

    if (ctx == NULL || ctx->sock == NULL || reply_length == NULL ||
        peer_mac == NULL)
    {
        return -1;
    }

    for (;;)
    {
        if (leap_win_recv_leap(
                ctx->sock,
                src_mac,
                frame_buf,
                sizeof(frame_buf),
                &frame_length,
                timeout_ms) != 0)
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
        return 0;
    }
}

static uint64_t leap_win_plain_pd_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

static void leap_win_controller_io_init_common(
    LeapControllerStackIo* io,
    LeapWinIoCtx*          ctx,
    int                    loopback_mirror)
{
    if (io == NULL || ctx == NULL)
    {
        return;
    }

    ctx->loopback_mirror = loopback_mirror;
    memset(io, 0, sizeof(*io));
    io->user_ctx     = ctx;
    io->send_frame   = leap_win_plain_ctrl_send;
    io->recv_frame   = leap_win_plain_ctrl_recv;
    io->monotonic_us = leap_win_plain_ctrl_monotonic;
}

void leap_win_controller_io_init(
    LeapControllerStackIo* io,
    LeapRawWinpcapSocket*  sock)
{
    static LeapWinIoCtx ctx;

    ctx.sock             = sock;
    ctx.loopback_mirror  = 0;
    leap_win_controller_io_init_common(io, &ctx, 0);
}

void leap_win_controller_io_init_loopback(
    LeapControllerStackIo* io,
    LeapRawWinpcapSocket*  sock)
{
    static LeapWinIoCtx ctx;

    ctx.sock = sock;
    leap_win_controller_io_init_common(io, &ctx, 1);
}

void leap_win_pd_init_io(
    LeapPdControllerIo*    pd_io,
    LeapRawWinpcapSocket* sock)
{
    static LeapWinIoCtx ctx;

    if (pd_io == NULL)
    {
        return;
    }

    ctx.sock             = sock;
    ctx.loopback_mirror  = 0;
    memset(pd_io, 0, sizeof(*pd_io));
    pd_io->user_ctx            = &ctx;
    pd_io->send_pd             = leap_win_plain_pd_send;
    pd_io->send_heartbeat      = leap_win_plain_pd_send_heartbeat;
    pd_io->wait_exchange_reply = leap_win_plain_pd_wait_exchange_reply;
    pd_io->monotonic_us        = leap_win_plain_pd_monotonic;
}
