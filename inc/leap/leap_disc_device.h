/*
 * leap_disc_device.h
 *
 * Device-side LEAP-DISC handling (HELLO, IDENTIFY).
 *
 * Discovery MUST NOT change outputs, owner leases, or process state.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DISC_DEVICE_H
#define LEAP_DISC_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_DISC_DEVICE_MAX_SERVICES 16u
#define LEAP_DISC_DEVICE_MAX_REPLY    128u

typedef struct LeapDiscDeviceConfig
{
    LeapIdentity identity;
    uint32_t     default_profile_id;
    uint32_t     active_profile_id;
    uint16_t     supported_services[LEAP_DISC_DEVICE_MAX_SERVICES];
    size_t       supported_service_count;
} LeapDiscDeviceConfig;

typedef struct LeapDiscDeviceContext
{
    LeapDiscDeviceConfig config;
} LeapDiscDeviceContext;

typedef enum LeapDiscDeviceStatus
{
    LEAP_DISC_DEVICE_OK = 0,
    LEAP_DISC_DEVICE_NOT_DISC,
    LEAP_DISC_DEVICE_IGNORED_RESPONSE,
    LEAP_DISC_DEVICE_UNSUPPORTED_MESSAGE,
    LEAP_DISC_DEVICE_BAD_LENGTH,
    LEAP_DISC_DEVICE_ERROR
} LeapDiscDeviceStatus;

typedef struct LeapDiscDeviceResult
{
    LeapDiscDeviceStatus status;
    LeapStatusCode_u16   error_code;
    uint16_t             message_type;
    uint8_t              payload[LEAP_DISC_DEVICE_MAX_REPLY];
    size_t               payload_length;
    LeapFrameView        frame;
} LeapDiscDeviceResult;

void leap_disc_device_init(LeapDiscDeviceContext* ctx, const LeapDiscDeviceConfig* config);

LeapDiscDeviceStatus leap_disc_device_process_frame(
    const LeapDiscDeviceContext*   disc,
    const LeapMgmtDeviceContext*   mgmt,
    const uint8_t*                 source_mac,
    const uint8_t*                 data,
    size_t                           length,
    LeapDiscDeviceResult*          result);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DISC_DEVICE_H */
