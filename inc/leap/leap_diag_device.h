/*
 * leap_diag_device.h
 *
 * Device-side LEAP-DIAG: counters, timing, events, and trace marks.
 *
 * Observers MAY read diagnostics in OP (§10.2). INIT/CONFIGURED reads are
 * open for commissioning, matching LEAP-DIR read behavior.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DIAG_DEVICE_H
#define LEAP_DIAG_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_DIAG_DEVICE_MAX_EVENTS  32u
#define LEAP_DIAG_DEVICE_MAX_REPLY   1024u

typedef struct LeapDiagDeviceConfig
{
    uint32_t max_events;
} LeapDiagDeviceConfig;

typedef struct LeapDiagDeviceContext
{
    LeapDiagDeviceConfig config;

    uint64_t rx_frames_accepted;
    uint64_t rx_frames_rejected;
    uint64_t tx_frames_accepted;
    uint64_t tx_frames_dropped;
    uint64_t crc_failures;
    uint64_t bad_length_failures;
    uint64_t unsupported_messages;
    uint64_t duplicate_sequences;
    uint64_t lease_expirations;
    uint64_t state_transition_rejects;
    uint64_t process_cycles_accepted;
    uint64_t process_cycles_missed;
    uint64_t stale_process_frames;
    uint64_t late_process_frames;
    uint64_t out_of_order_frames;
    uint64_t reply_timeouts;
    uint64_t max_reply_latency_us;
    uint64_t last_reply_latency_us;
    uint64_t switch_congestion_hints;

    uint32_t last_cycle_time_us;
    uint32_t max_cycle_time_us;
    uint32_t min_cycle_time_us;

    LeapEventEntry events[LEAP_DIAG_DEVICE_MAX_EVENTS];
    uint32_t       event_next_index;
    uint32_t       event_total;

    int boot_event_recorded;
} LeapDiagDeviceContext;

typedef enum LeapDiagDeviceStatus
{
    LEAP_DIAG_DEVICE_OK = 0,
    LEAP_DIAG_DEVICE_NOT_DIAG,
    LEAP_DIAG_DEVICE_IGNORED_RESPONSE,
    LEAP_DIAG_DEVICE_UNSUPPORTED_MESSAGE,
    LEAP_DIAG_DEVICE_BAD_LENGTH,
    LEAP_DIAG_DEVICE_INVALID_STATE,
    LEAP_DIAG_DEVICE_NOT_OWNER,
    LEAP_DIAG_DEVICE_NO_REPLY,
    LEAP_DIAG_DEVICE_ERROR
} LeapDiagDeviceStatus;

typedef struct LeapDiagDeviceResult
{
    LeapDiagDeviceStatus status;
    LeapStatusCode_u16   error_code;
    uint16_t             message_type;
    uint8_t              payload[LEAP_DIAG_DEVICE_MAX_REPLY];
    size_t               payload_length;
    LeapFrameView        frame;
} LeapDiagDeviceResult;

void leap_diag_device_init(LeapDiagDeviceContext* ctx, const LeapDiagDeviceConfig* config);

void leap_diag_device_on_transport_ready(LeapDiagDeviceContext* ctx, uint64_t now_us);

void leap_diag_device_on_frame_parse_error(
    LeapDiagDeviceContext*   ctx,
    LeapFrameParseResult     error);

void leap_diag_device_on_frame_accepted(LeapDiagDeviceContext* ctx);

void leap_diag_device_on_frame_rejected(LeapDiagDeviceContext* ctx);

void leap_diag_device_on_unsupported_service(LeapDiagDeviceContext* ctx);

void leap_diag_device_on_pd_result(
    LeapDiagDeviceContext*     ctx,
    const LeapPdDeviceResult*  pd_result,
    uint64_t                   now_us);

void leap_diag_device_on_mgmt_flags(
    LeapDiagDeviceContext* ctx,
    uint32_t               flags,
    uint16_t               mgmt_message_type,
    LeapState_u16          device_state,
    uint64_t               now_us);

void leap_diag_device_on_tick_flags(
    LeapDiagDeviceContext* ctx,
    uint32_t               flags,
    uint64_t               now_us);

void leap_diag_device_record_event(
    LeapDiagDeviceContext* ctx,
    uint16_t               event_id,
    uint32_t               detail0,
    uint32_t               detail1,
    uint64_t               now_us);

LeapDiagDeviceStatus leap_diag_device_process_frame(
    LeapDiagDeviceContext*       diag,
    LeapMgmtDeviceContext*       mgmt,
    const uint8_t*               source_mac,
    uint64_t                       now_us,
    const uint8_t*               data,
    size_t                         length,
    LeapDiagDeviceResult*        result);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DIAG_DEVICE_H */
