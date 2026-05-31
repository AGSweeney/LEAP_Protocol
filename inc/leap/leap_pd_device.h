/*
 * leap_pd_device.h
 *
 * Device-side LEAP-PD frame handling integrated with LEAP-MGMT ownership.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_PD_DEVICE_H
#define LEAP_PD_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapPdDeviceStatus
{
    LEAP_PD_DEVICE_OK = 0,
    LEAP_PD_DEVICE_NOT_PD,
    LEAP_PD_DEVICE_IGNORED_RESPONSE,
    LEAP_PD_DEVICE_UNSUPPORTED_MESSAGE,
    LEAP_PD_DEVICE_REJECTED
} LeapPdDeviceStatus;

#define LEAP_PD_DEVICE_FLAG_PROCESSED       (1u << 0)
#define LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED (1u << 1)
#define LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED (1u << 2)

typedef struct LeapPdDeviceResult
{
    LeapPdDeviceStatus status;
    uint32_t           flags;
    LeapStatusCode_u16 error_code;
    LeapFrameView      frame;
} LeapPdDeviceResult;

/*
 * Handle a validated LEAP-PD frame. Accepted owner traffic in OP refreshes
 * lease and process watchdog per spec section 10.2.
 */
LeapPdDeviceStatus leap_pd_device_process_frame(
    LeapMgmtDeviceContext* ctx,
    const uint8_t*         source_mac,
    uint64_t               now_us,
    const uint8_t*         data,
    size_t                 length,
    LeapPdDeviceResult*    result);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_PD_DEVICE_H */
