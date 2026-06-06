/*
 * leap_dir_device.h
 *
 * Device-side LEAP-DIR: directory read, object read, and profile selection.
 *
 * SELECT_PROFILE transitions INIT -> CONFIGURED via leap_mgmt_device when
 * the requested profile is supported.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DIR_DEVICE_H
#define LEAP_DIR_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_disc_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_DIR_DEVICE_MAX_PROFILES   4u
#define LEAP_DIR_DEVICE_MAX_ENDPOINTS  8u
#define LEAP_DIR_DEVICE_MAX_REPLY      512u

typedef struct LeapDirDeviceProfile
{
    LeapProfileDescriptor               descriptor;
    LeapEndpointDescriptor              endpoints[LEAP_DIR_DEVICE_MAX_ENDPOINTS];
    size_t                              endpoint_count;
} LeapDirDeviceProfile;

typedef struct LeapDirDeviceConfig
{
    LeapIdentity        identity;
    uint32_t            default_profile_id;
    uint32_t            active_profile_id;
    LeapDirDeviceProfile profiles[LEAP_DIR_DEVICE_MAX_PROFILES];
    size_t              profile_count;
} LeapDirDeviceConfig;

typedef struct LeapDirDeviceContext
{
    LeapDirDeviceConfig config;
} LeapDirDeviceContext;

typedef enum LeapDirDeviceStatus
{
    LEAP_DIR_DEVICE_OK = 0,
    LEAP_DIR_DEVICE_NOT_DIR,
    LEAP_DIR_DEVICE_IGNORED_RESPONSE,
    LEAP_DIR_DEVICE_UNSUPPORTED_MESSAGE,
    LEAP_DIR_DEVICE_BAD_LENGTH,
    LEAP_DIR_DEVICE_INVALID_STATE,
    LEAP_DIR_DEVICE_PROFILE_MISMATCH,
    LEAP_DIR_DEVICE_NOT_OWNER,
    LEAP_DIR_DEVICE_ERROR
} LeapDirDeviceStatus;

typedef struct LeapDirDeviceResult
{
    LeapDirDeviceStatus status;
    LeapStatusCode_u16  error_code;
    uint16_t            message_type;
    uint8_t             payload[LEAP_DIR_DEVICE_MAX_REPLY];
    size_t              payload_length;
    uint32_t            flags;
    LeapFrameView       frame;
} LeapDirDeviceResult;

#define LEAP_DIR_DEVICE_FLAG_PROFILE_SELECTED   (1u << 0)
#define LEAP_DIR_DEVICE_FLAG_STATE_CONFIGURED   (1u << 1)

void leap_dir_device_init(LeapDirDeviceContext* ctx, const LeapDirDeviceConfig* config);

/*
 * Install a single digital I/O profile on config (replaces any existing profiles).
 * output_bit_count / input_bit_count: 0 omits that endpoint direction.
 * profile_id must be LEAP_PROFILE_DIGITAL_IO_8X8, _16X16, or _32X32.
 * Returns 0 on success, -1 on invalid arguments.
 */
int leap_dir_device_config_set_digital_io(
    LeapDirDeviceConfig* config,
    uint32_t             profile_id,
    uint16_t             output_bit_count,
    uint16_t             input_bit_count);

void leap_dir_device_sync_disc(LeapDirDeviceContext* dir, LeapDiscDeviceContext* disc);

LeapDirDeviceStatus leap_dir_device_process_frame(
    LeapDirDeviceContext*        dir,
    LeapDiscDeviceContext*       disc,
    LeapMgmtDeviceContext*       mgmt,
    const uint8_t*               source_mac,
    const uint8_t*               data,
    size_t                         length,
    LeapDirDeviceResult*         result);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DIR_DEVICE_H */
