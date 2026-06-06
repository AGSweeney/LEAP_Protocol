/*
 * leap_device_stack.h
 *
 * Device-side frame dispatch: LEAP-DISC, LEAP-DIR, LEAP-MGMT, and LEAP-PD.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DEVICE_STACK_H
#define LEAP_DEVICE_STACK_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_dir_device.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_diag_device.h"
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
    LeapMgmtDeviceContext  mgmt;
    LeapDiscDeviceContext  disc;
    LeapDirDeviceContext   dir;
    LeapDiagDeviceContext  diag;
    LeapPdDeviceContext    pd;
    LeapPdDeviceIoBinding  pd_io;
    int                    pd_io_bound;
    uint64_t               last_frame_rx_us;
    uint16_t               last_frame_service_id;
} LeapDeviceStack;

typedef struct LeapDeviceStackConfig
{
    LeapMgmtDeviceConfig mgmt;
    LeapDiscDeviceConfig disc;
    LeapDirDeviceConfig  dir;
    LeapDiagDeviceConfig diag;
} LeapDeviceStackConfig;

typedef enum LeapDeviceStackStatus
{
    LEAP_DEVICE_STACK_OK = 0,
    LEAP_DEVICE_STACK_FRAME_ERROR,
    LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE,
    LEAP_DEVICE_STACK_MGMT_ERROR,
    LEAP_DEVICE_STACK_PD_REJECTED,
    LEAP_DEVICE_STACK_DISC_ERROR,
    LEAP_DEVICE_STACK_DIR_ERROR,
    LEAP_DEVICE_STACK_DIAG_ERROR
} LeapDeviceStackStatus;

#define LEAP_DEVICE_STACK_FLAG_MGMT_PROCESSED       LEAP_MGMT_PROCESS_FLAG_PROCESSED
#define LEAP_DEVICE_STACK_FLAG_OWNERSHIP_CHANGED    LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED
#define LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED   LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED
#define LEAP_DEVICE_STACK_FLAG_STATE_CHANGED        LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED
#define LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY       LEAP_MGMT_PROCESS_FLAG_HAS_REPLY
#define LEAP_DEVICE_STACK_FLAG_PD_PROCESSED         LEAP_PD_DEVICE_FLAG_PROCESSED
#define LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED      LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED
#define LEAP_DEVICE_STACK_FLAG_LEASE_REFRESHED      LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED
#define LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY       (1u << 16)
#define LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY        (1u << 17)
#define LEAP_DEVICE_STACK_FLAG_DIR_PROFILE_SELECTED LEAP_DIR_DEVICE_FLAG_PROFILE_SELECTED
#define LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY       (1u << 18)
#define LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY         LEAP_PD_DEVICE_FLAG_HAS_REPLY
#define LEAP_DEVICE_STACK_FLAG_PD_INPUTS_READ       LEAP_PD_DEVICE_FLAG_INPUTS_READ

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
    uint16_t              disc_message_type;
    uint8_t               disc_payload[LEAP_DISC_DEVICE_MAX_REPLY];
    size_t                disc_payload_length;
    uint16_t              dir_message_type;
    uint8_t               dir_payload[LEAP_DIR_DEVICE_MAX_REPLY];
    size_t                dir_payload_length;
    uint16_t              diag_message_type;
    uint8_t               diag_payload[LEAP_DIAG_DEVICE_MAX_REPLY];
    size_t                diag_payload_length;
    uint16_t              pd_reply_message_type;
    uint8_t               pd_reply_payload[LEAP_PD_DEVICE_MAX_REPLY];
    size_t                pd_reply_payload_length;
    uint16_t              pd_outputs_applied;
    uint16_t              pd_inputs_snapshot;
} LeapDeviceStackResult;

void leap_device_stack_init(LeapDeviceStack* stack, const LeapMgmtDeviceConfig* config);
void leap_device_stack_init_full(LeapDeviceStack* stack, const LeapDeviceStackConfig* config);

void leap_device_stack_bind_pd_io(
    LeapDeviceStack*             stack,
    const LeapPdDeviceIoBinding* io_binding);

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

void leap_device_stack_note_frame_rx(
    LeapDeviceStack* stack,
    uint64_t         now_us,
    uint16_t         service_id);

void leap_device_stack_notify_tx_ok(
    LeapDeviceStack* stack,
    uint64_t         now_us);

void leap_device_stack_notify_tx_drop(LeapDeviceStack* stack);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DEVICE_STACK_H */
