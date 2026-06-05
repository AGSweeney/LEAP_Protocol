/*
 * leap_conformance_raw_io.h
 *
 * Transport-agnostic LEAP frame I/O on LeapRawWinpcapSocket (Windows).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_RAW_IO_H
#define LEAP_CONFORMANCE_RAW_IO_H

#include "leap/leap_controller_stack.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_raw_winpcap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapConformanceRawIo
{
    LeapRawWinpcapSocket*  transport;
    LeapControllerStackIo  stack_io;
    LeapPdControllerIo     pd_io;
} LeapConformanceRawIo;

void leap_conformance_raw_io_bind(
    LeapConformanceRawIo*   io,
    LeapRawWinpcapSocket*   transport);

int leap_conformance_raw_send_disc(
    LeapConformanceRawIo* io,
    const uint8_t*        peer_mac,
    uint16_t              message_type,
    const uint8_t*        payload,
    size_t                payload_length);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_RAW_IO_H */
