/*
 * leap_device_stack.h
 *
 * Device-side frame dispatch: LEAP-MGMT + LEAP-PD over a single ingress path.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DEVICE_STACK_H
#define LEAP_DEVICE_STACK_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_mgmt_process.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapDeviceStack
{
    LeapMgmtDeviceContext mgmt;
} LeapDeviceStack;

typedef enum LeapDeviceStackStatus
{
    LEAP_DEVICE_STACK_OK = 0,
    LEAP_DEVICE_STACK_FRAME_ERROR,
    LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE,
    LEAP_DEVICE_STACK_MGMT_ERROR,
    LEAP_DEVICE_STACK_PD_REJECTED
} LeapDeviceStackStatus;

#define LEAP_DEVICE_STACK_FLAG_MGMT_PROCESSED       LEAP_MGMT_PROCESS_FLAG_PROCESSED
#define LEAP_DEVICE_STACK_FLAG_OWNERSHIP_CHANGED    LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED
#define LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED   LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED
#define LEAP_DEVICE_STACK_FLAG_STATE_CHANGED        LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED
#define LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY       LEAP_MGMT_PROCESS_FLAG_HAS_REPLY
#define LEAP_DEVICE_STACK_FLAG_PD_PROCESSED         LEAP_PD_DEVICE_FLAG_PROCESSED
#define LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED      LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED
#define LEAP_DEVICE_STACK_FLAG_LEASE_REFRESHED      LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED

typedef struct LeapDeviceStackResult
{
    LeapDeviceStackStatus status;
    LeapFrameParseResult  frame_error;
    uint32_t              flags;
    LeapServiceId_u16     service_id;
    LeapState_u16         device_state;
    LeapStatusCode_u16    error_code;
    LeapFrameView         frame;
    LeapMgmtDeviceReply   mgmt_reply;
} LeapDeviceStackResult;

void leap_device_stack_init(LeapDeviceStack* stack, const LeapMgmtDeviceConfig* config);

LeapDeviceStackStatus leap_device_stack_process_frame(
    LeapDeviceStack*          stack,
    const uint8_t*            source_mac,
    uint64_t                  now_us,
    const uint8_t*            data,
    size_t                    length,
    LeapDeviceStackResult*    result);

LeapDeviceStackStatus leap_device_stack_tick(
    LeapDeviceStack* stack,
    uint64_t         now_us,
    uint32_t*        flags_out);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DEVICE_STACK_H */
