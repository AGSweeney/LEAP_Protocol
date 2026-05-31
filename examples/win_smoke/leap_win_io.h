/*
 * leap_win_io.h
 *
 * WinPcap transport adapters for LEAP controller stack I/O.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_WIN_IO_H
#define LEAP_WIN_IO_H

#include "leap/leap_controller_stack.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_raw_winpcap.h"

#if defined(_WIN32)
#include <windows.h>
#endif

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
    size_t                payload_length);

int leap_win_recv_leap(
    LeapRawWinpcapSocket* sock,
    uint8_t*              src_mac,
    uint8_t*              payload,
    size_t                payload_capacity,
    size_t*               payload_length,
    int                   timeout_ms);

void leap_win_print_mac(const char* label, const uint8_t* mac);

void leap_win_controller_io_init(
    LeapControllerStackIo* io,
    LeapRawWinpcapSocket*  sock);

void leap_win_controller_io_init_loopback(
    LeapControllerStackIo* io,
    LeapRawWinpcapSocket*  sock);

void leap_win_pd_init_io(
    LeapPdControllerIo*    pd_io,
    LeapRawWinpcapSocket* sock);

typedef struct LeapWinSharedTransport
{
    LeapRawWinpcapSocket sock;
    CRITICAL_SECTION     lock;
    int                  lock_ready;
} LeapWinSharedTransport;

void leap_win_shared_transport_init(LeapWinSharedTransport* transport);

void leap_win_shared_transport_shutdown(LeapWinSharedTransport* transport);

void leap_win_controller_io_init_shared(
    LeapControllerStackIo*  io,
    LeapWinSharedTransport* transport);

void leap_win_pd_init_io_shared(
    LeapPdControllerIo*     pd_io,
    LeapWinSharedTransport* transport);

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
    size_t                  payload_length);

int leap_win_shared_recv_leap(
    LeapWinSharedTransport* transport,
    uint8_t*                src_mac,
    uint8_t*                payload,
    size_t                  payload_capacity,
    size_t*                 payload_length,
    int                     timeout_ms);

#endif /* LEAP_WIN_IO_H */
