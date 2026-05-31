/*
 * leap_diag_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_diag_controller.h"

#include <string.h>

size_t leap_diag_controller_build_read_counters(
    uint8_t* out,
    size_t   out_capacity,
    uint16_t first_counter_id,
    uint16_t counter_count,
    uint32_t read_flags)
{
    LeapReadCountersRequest* req;

    if (out == NULL || out_capacity < sizeof(LeapReadCountersRequest))
    {
        return 0u;
    }

    memset(out, 0, sizeof(LeapReadCountersRequest));
    req                 = (LeapReadCountersRequest*)out;
    req->first_counter_id = first_counter_id;
    req->counter_count    = counter_count;
    req->read_flags       = read_flags;
    return sizeof(LeapReadCountersRequest);
}

size_t leap_diag_controller_build_read_timing(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t timing_flags)
{
    LeapReadTimingRequest* req;

    if (out == NULL || out_capacity < sizeof(LeapReadTimingRequest))
    {
        return 0u;
    }

    memset(out, 0, sizeof(LeapReadTimingRequest));
    req               = (LeapReadTimingRequest*)out;
    req->timing_flags = timing_flags;
    return sizeof(LeapReadTimingRequest);
}

size_t leap_diag_controller_build_read_events(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t start_index,
    uint16_t max_events,
    uint16_t event_flags)
{
    LeapReadEventsRequest* req;

    if (out == NULL || out_capacity < sizeof(LeapReadEventsRequest))
    {
        return 0u;
    }

    memset(out, 0, sizeof(LeapReadEventsRequest));
    req             = (LeapReadEventsRequest*)out;
    req->start_index  = start_index;
    req->max_events   = max_events;
    req->event_flags  = event_flags;
    return sizeof(LeapReadEventsRequest);
}

size_t leap_diag_controller_build_trace_mark(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t trace_id,
    uint32_t value0,
    uint32_t value1)
{
    LeapTraceMarkRequest* req;

    if (out == NULL || out_capacity < sizeof(LeapTraceMarkRequest))
    {
        return 0u;
    }

    memset(out, 0, sizeof(LeapTraceMarkRequest));
    req           = (LeapTraceMarkRequest*)out;
    req->trace_id = trace_id;
    req->value0   = value0;
    req->value1   = value1;
    return sizeof(LeapTraceMarkRequest);
}

LeapDiagControllerStatus leap_diag_controller_on_counters_reply(
    const uint8_t*     payload,
    size_t             payload_length,
    LeapCountersReply* header_out,
    LeapCounterEntry*  entries_out,
    size_t             entries_capacity,
    size_t*            entries_out_count)
{
    size_t expected;
    size_t i;

    if (payload == NULL || header_out == NULL)
    {
        return LEAP_DIAG_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapCountersReply))
    {
        return LEAP_DIAG_CTRL_BAD_LENGTH;
    }

    memcpy(header_out, payload, sizeof(LeapCountersReply));
    expected = sizeof(LeapCountersReply) +
               ((size_t)header_out->counter_count * sizeof(LeapCounterEntry));
    if (payload_length < expected)
    {
        return LEAP_DIAG_CTRL_BAD_LENGTH;
    }

    if (entries_out_count != NULL)
    {
        *entries_out_count = header_out->counter_count;
    }

    if (entries_out == NULL || entries_capacity == 0u)
    {
        return LEAP_DIAG_CTRL_OK;
    }

    for (i = 0u; i < header_out->counter_count && i < entries_capacity; i++)
    {
        memcpy(
            &entries_out[i],
            payload + sizeof(LeapCountersReply) + (i * sizeof(LeapCounterEntry)),
            sizeof(LeapCounterEntry));
    }

    return LEAP_DIAG_CTRL_OK;
}

LeapDiagControllerStatus leap_diag_controller_on_timing_reply(
    const uint8_t*   payload,
    size_t           payload_length,
    LeapTimingReply* out)
{
    if (payload == NULL || out == NULL)
    {
        return LEAP_DIAG_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapTimingReply))
    {
        return LEAP_DIAG_CTRL_BAD_LENGTH;
    }

    memcpy(out, payload, sizeof(LeapTimingReply));
    return LEAP_DIAG_CTRL_OK;
}

LeapDiagControllerStatus leap_diag_controller_on_events_reply(
    const uint8_t*   payload,
    size_t           payload_length,
    LeapEventsReply* header_out,
    LeapEventEntry*  entries_out,
    size_t           entries_capacity,
    size_t*          entries_out_count)
{
    size_t expected;
    size_t i;

    if (payload == NULL || header_out == NULL)
    {
        return LEAP_DIAG_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapEventsReply))
    {
        return LEAP_DIAG_CTRL_BAD_LENGTH;
    }

    memcpy(header_out, payload, sizeof(LeapEventsReply));
    expected = sizeof(LeapEventsReply) +
               ((size_t)header_out->event_count * sizeof(LeapEventEntry));
    if (payload_length < expected)
    {
        return LEAP_DIAG_CTRL_BAD_LENGTH;
    }

    if (entries_out_count != NULL)
    {
        *entries_out_count = header_out->event_count;
    }

    if (entries_out == NULL || entries_capacity == 0u)
    {
        return LEAP_DIAG_CTRL_OK;
    }

    for (i = 0u; i < header_out->event_count && i < entries_capacity; i++)
    {
        memcpy(
            &entries_out[i],
            payload + sizeof(LeapEventsReply) + (i * sizeof(LeapEventEntry)),
            sizeof(LeapEventEntry));
    }

    return LEAP_DIAG_CTRL_OK;
}
