/*
 * leap_conformance_raw_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_raw_io.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define LEAP_CONF_RAW_TX_BUF 1600u
#define LEAP_CONF_RAW_RX_BUF 1600u

static int leap_conf_raw_send_leap(
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
    uint8_t tx[LEAP_CONF_RAW_TX_BUF];
    size_t  tx_len = 0u;

    if (sock == NULL || dst_mac == NULL)
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

    return leap_raw_winpcap_send(sock, dst_mac, tx, tx_len);
}

static int leap_conf_raw_recv_leap(
    LeapRawWinpcapSocket* sock,
    uint8_t*              src_mac,
    uint8_t*              frame_buf,
    size_t                frame_capacity,
    size_t*               frame_length,
    int                   timeout_ms)
{
    if (sock == NULL || frame_length == NULL)
    {
        return -1;
    }

    return leap_raw_winpcap_recv(
        sock,
        src_mac,
        frame_buf,
        frame_capacity,
        frame_length,
        timeout_ms,
        NULL);
}

static int leap_conf_raw_stack_send(
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
    LeapConformanceRawIo* io = (LeapConformanceRawIo*)user_ctx;

    if (io == NULL || io->transport == NULL)
    {
        return -1;
    }

    return leap_conf_raw_send_leap(
        io->transport,
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

static int leap_conf_raw_stack_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    LeapConformanceRawIo* io = (LeapConformanceRawIo*)user_ctx;
    uint8_t               frame_buf[LEAP_CONF_RAW_RX_BUF];
    size_t                frame_length = 0u;
    LeapFrameView         view;

    if (io == NULL || io->transport == NULL || payload_length == NULL)
    {
        return -1;
    }

    if (leap_conf_raw_recv_leap(
            io->transport,
            src_mac,
            frame_buf,
            sizeof(frame_buf),
            &frame_length,
            timeout_ms) != 0)
    {
        return -1;
    }

    if (leap_frame_parse(frame_buf, frame_length, &view) != LEAP_FRAME_OK)
    {
        return -1;
    }

    if (parsed != NULL)
    {
        *parsed = view;
    }

    /*
     * Peer discovery and the controller stack re-parse the full LEAP frame
     * from this buffer (same contract as leap_win_ctrl_io_recv). Returning
     * only the service payload broke HELLO_REPLY ingestion (peers=0 while
     * the device still logged incoming DISC HELLO).
     */
    if (frame_length > payload_capacity)
    {
        return -1;
    }

    *payload_length = frame_length;
    if (payload_buf != NULL && frame_length > 0u)
    {
        memcpy(payload_buf, frame_buf, frame_length);
    }

    return 0;
}

static uint64_t leap_conf_raw_mono(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

static int leap_conf_raw_pd_send(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapConformanceRawIo* io = (LeapConformanceRawIo*)user_ctx;

    if (io == NULL || io->transport == NULL)
    {
        return -1;
    }

    return leap_conf_raw_send_leap(
        io->transport,
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

static int leap_conf_raw_pd_hb(
    void*          user_ctx,
    const uint8_t* peer_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    LeapConformanceRawIo* io = (LeapConformanceRawIo*)user_ctx;

    if (io == NULL || io->transport == NULL)
    {
        return -1;
    }

    return leap_conf_raw_send_leap(
        io->transport,
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

static int leap_conf_raw_pd_wait_exchange(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms,
    uint64_t*      reply_recv_us_out)
{
    LeapConformanceRawIo* io = (LeapConformanceRawIo*)user_ctx;
    uint8_t               src_mac[6];
    uint8_t               frame_buf[LEAP_CONF_RAW_RX_BUF];
    size_t                frame_length = 0u;
    LeapFrameView         view;

    if (io == NULL || io->transport == NULL || peer_mac == NULL ||
        reply_length == NULL)
    {
        return -1;
    }

    if (reply_recv_us_out != NULL)
    {
        *reply_recv_us_out = 0u;
    }

    for (;;)
    {
        if (leap_conf_raw_recv_leap(
                io->transport,
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

        if (view.header.service_id != (uint16_t)LEAP_SERVICE_PD)
        {
            continue;
        }

        if ((view.header.flags & LEAP_FLAG_ERROR) != 0u)
        {
            return -1;
        }

        if (view.payload_length > reply_capacity)
        {
            return -1;
        }

        *reply_length = view.payload_length;
        if (reply_payload != NULL && view.payload_length > 0u &&
            view.payload != NULL)
        {
            memcpy(reply_payload, view.payload, view.payload_length);
        }

        return 0;
    }
}

int leap_conformance_raw_send_service(
    LeapConformanceRawIo* io,
    const uint8_t*        peer_mac,
    uint16_t              service_id,
    uint16_t              message_type,
    const uint8_t*        payload,
    size_t                payload_length)
{
    if (io == NULL || peer_mac == NULL || io->stack_io.send_frame == NULL)
    {
        return -1;
    }

    return io->stack_io.send_frame(
        io->stack_io.user_ctx,
        peer_mac,
        0u,
        service_id,
        message_type,
        0u,
        1u,
        0u,
        payload,
        payload_length);
}

int leap_conformance_raw_send_disc(
    LeapConformanceRawIo* io,
    const uint8_t*        peer_mac,
    uint16_t              message_type,
    const uint8_t*        payload,
    size_t                payload_length)
{
    if (payload == NULL || payload_length == 0u)
    {
        return -1;
    }

    return leap_conformance_raw_send_service(
        io,
        peer_mac,
        (uint16_t)LEAP_SERVICE_DISC,
        message_type,
        payload,
        payload_length);
}

void leap_conformance_raw_io_bind(
    LeapConformanceRawIo* io,
    LeapRawWinpcapSocket* transport)
{
    if (io == NULL)
    {
        return;
    }

    memset(io, 0, sizeof(*io));
    io->transport = transport;

    memset(&io->stack_io, 0, sizeof(io->stack_io));
    io->stack_io.user_ctx     = io;
    io->stack_io.send_frame   = leap_conf_raw_stack_send;
    io->stack_io.recv_frame   = leap_conf_raw_stack_recv;
    io->stack_io.monotonic_us = leap_conf_raw_mono;

    memset(&io->pd_io, 0, sizeof(io->pd_io));
    io->pd_io.user_ctx            = io;
    io->pd_io.send_pd             = leap_conf_raw_pd_send;
    io->pd_io.send_heartbeat      = leap_conf_raw_pd_hb;
    io->pd_io.wait_exchange_reply = leap_conf_raw_pd_wait_exchange;
    io->pd_io.monotonic_us        = leap_conf_raw_mono;
}
