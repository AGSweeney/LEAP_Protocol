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

#include "leap/leap_dir_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_pd_common.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_PD_DEVICE_FLAG_PROCESSED       (1u << 0)
#define LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED (1u << 1)
#define LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED (1u << 2)
#define LEAP_PD_DEVICE_FLAG_HAS_REPLY       (1u << 3)
#define LEAP_PD_DEVICE_FLAG_INPUTS_READ     (1u << 4)
#define LEAP_PD_DEVICE_FLAG_SEQUENCE_GAP    (1u << 5)

#define LEAP_PD_DEVICE_MAX_REPLY 160u

typedef enum LeapPdDeviceStatus
{
    LEAP_PD_DEVICE_OK = 0,
    LEAP_PD_DEVICE_NOT_PD,
    LEAP_PD_DEVICE_IGNORED_RESPONSE,
    LEAP_PD_DEVICE_UNSUPPORTED_MESSAGE,
    LEAP_PD_DEVICE_REJECTED
} LeapPdDeviceStatus;

typedef struct LeapPdDeviceConfig
{
    LeapPdProfileMap profile;
    /*
     * When non-zero (default), reject process_sequence <= last accepted value.
     * Gaps (seq > last + 1) are accepted and counted; duplicates/stale rejected.
     */
    int enforce_sequence;
} LeapPdDeviceConfig;

typedef struct LeapPdDeviceContext
{
    LeapPdDeviceConfig config;
    uint32_t           last_process_sequence;
    uint32_t           bound_session_id;
    uint32_t           stale_rejections;
    uint32_t           sequence_gaps;
    int                sequence_active;
} LeapPdDeviceContext;

typedef struct LeapPdDeviceIoBinding
{
    uint16_t* digital_outputs;
    uint16_t* digital_inputs;
    uint16_t* io_status;
} LeapPdDeviceIoBinding;

typedef struct LeapPdDeviceResult
{
    LeapPdDeviceStatus status;
    uint32_t           flags;
    LeapStatusCode_u16 error_code;
    LeapFrameView      frame;

    uint16_t digital_outputs_applied;
    uint16_t digital_inputs_snapshot;

    uint16_t reply_message_type;
    uint8_t  reply_payload[LEAP_PD_DEVICE_MAX_REPLY];
    size_t   reply_payload_length;
} LeapPdDeviceResult;

void leap_pd_device_init(
    LeapPdDeviceContext*       ctx,
    const LeapPdDeviceConfig* config);

void leap_pd_device_sync_profile_from_dir(
    LeapPdDeviceContext*        pd,
    const LeapDirDeviceContext* dir);

void leap_pd_device_reset_sequence(LeapPdDeviceContext* pd, uint32_t session_id);

/*
 * Handle a validated LEAP-PD frame. Accepted owner traffic in OP refreshes
 * lease and process watchdog per spec section 10.2.
 *
 * pd_ctx may be NULL to skip sequence enforcement and use default profile map.
 */
LeapPdDeviceStatus leap_pd_device_process_frame(
    LeapMgmtDeviceContext*       mgmt,
    LeapPdDeviceContext*         pd_ctx,
    const LeapPdDeviceIoBinding* io_binding,
    const uint8_t*               source_mac,
    uint64_t                     now_us,
    const uint8_t*               data,
    size_t                       length,
    LeapPdDeviceResult*          result);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_PD_DEVICE_H */
