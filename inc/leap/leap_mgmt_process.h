/*
 * leap_mgmt_process.h
 *
 * Integrates leap_frame_parse() with the device-side MGMT handler.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_MGMT_PROCESS_H
#define LEAP_MGMT_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapMgmtProcessStatus
{
    LEAP_MGMT_PROCESS_OK = 0,
    LEAP_MGMT_PROCESS_FRAME_ERROR,
    LEAP_MGMT_PROCESS_NOT_MGMT,
    LEAP_MGMT_PROCESS_IGNORED_RESPONSE,
    LEAP_MGMT_PROCESS_UNSUPPORTED_FRAME,
    LEAP_MGMT_PROCESS_HANDLER_ERROR
} LeapMgmtProcessStatus;

#define LEAP_MGMT_PROCESS_FLAG_PROCESSED          (1u << 0)
#define LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED  (1u << 1)
#define LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED (1u << 2)
#define LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED      (1u << 3)
#define LEAP_MGMT_PROCESS_FLAG_HAS_REPLY          (1u << 4)

typedef struct LeapMgmtProcessResult
{
    LeapMgmtProcessStatus      status;
    LeapFrameParseResult       frame_error;
    uint32_t                   flags;
    LeapState_u16              device_state;
    LeapMgmtDeviceHandleStatus handle_status;
    LeapStatusCode_u16         error_code;
    LeapFrameView              frame;
    LeapMgmtDeviceReply        reply;
} LeapMgmtProcessResult;

/*
 * Parse a LEAP Ethernet payload and dispatch LEAP-MGMT requests to the device
 * handler. source_mac is the Ethernet source address of the frame (not present
 * in LeapHeader). now_us is injected monotonic time for lease/watchdog logic.
 *
 * On LEAP_MGMT_PROCESS_OK the frame was accepted and handled (with or without
 * a reply). Side-effect flags are set when ownership or device state changes.
 */
LeapMgmtProcessStatus leap_mgmt_process_frame(
    LeapMgmtDeviceContext*       ctx,
    const uint8_t*               source_mac,
    uint64_t                       now_us,
    const uint8_t*               data,
    size_t                         length,
    LeapMgmtProcessResult*       result);

/*
 * Run leap_mgmt_device_tick() and report ownership or SAFE-state side effects.
 */
LeapMgmtProcessStatus leap_mgmt_process_tick(
    LeapMgmtDeviceContext* ctx,
    uint64_t               now_us,
    uint32_t*              flags_out);

const char* leap_mgmt_process_status_string(LeapMgmtProcessStatus status);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_MGMT_PROCESS_H */
