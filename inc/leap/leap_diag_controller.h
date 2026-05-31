/*
 * leap_diag_controller.h
 *
 * Controller-side LEAP-DIAG request builders and reply parsers.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DIAG_CONTROLLER_H
#define LEAP_DIAG_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapDiagControllerStatus
{
    LEAP_DIAG_CTRL_OK = 0,
    LEAP_DIAG_CTRL_BAD_LENGTH,
    LEAP_DIAG_CTRL_ERROR
} LeapDiagControllerStatus;

size_t leap_diag_controller_build_read_counters(
    uint8_t* out,
    size_t   out_capacity,
    uint16_t first_counter_id,
    uint16_t counter_count,
    uint32_t read_flags);

size_t leap_diag_controller_build_read_timing(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t timing_flags);

size_t leap_diag_controller_build_read_events(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t start_index,
    uint16_t max_events,
    uint16_t event_flags);

size_t leap_diag_controller_build_trace_mark(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t trace_id,
    uint32_t value0,
    uint32_t value1);

LeapDiagControllerStatus leap_diag_controller_on_counters_reply(
    const uint8_t*       payload,
    size_t               payload_length,
    LeapCountersReply*   header_out,
    LeapCounterEntry*    entries_out,
    size_t               entries_capacity,
    size_t*              entries_out_count);

LeapDiagControllerStatus leap_diag_controller_on_timing_reply(
    const uint8_t*  payload,
    size_t          payload_length,
    LeapTimingReply* out);

LeapDiagControllerStatus leap_diag_controller_on_events_reply(
    const uint8_t*    payload,
    size_t            payload_length,
    LeapEventsReply*  header_out,
    LeapEventEntry*   entries_out,
    size_t            entries_capacity,
    size_t*           entries_out_count);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DIAG_CONTROLLER_H */
