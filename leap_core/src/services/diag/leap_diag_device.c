/*
 * leap_diag_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_diag_device.h"

#include "leap/leap_device_host_perf.h"
#include "leap/leap_mgmt_process.h"
#include "leap/leap_pd_device.h"

#include <string.h>

static uint32_t leap_diag_remaining_us(uint64_t deadline_us, uint64_t now_us)
{
    uint64_t delta;

    if (deadline_us <= now_us)
    {
        return 0u;
    }

    delta = deadline_us - now_us;
    if (delta > 0xFFFFFFFFu)
    {
        return 0xFFFFFFFFu;
    }

    return (uint32_t)delta;
}

static int leap_diag_session_allows_read(
    const LeapMgmtDeviceContext* mgmt,
    uint32_t                     session_id,
    const uint8_t*               source_mac,
    uint64_t                       now_us)
{
    return leap_mgmt_device_session_allows_diag_read(
        mgmt,
        session_id,
        source_mac,
        now_us);
}

static int leap_diag_get_counter(
    const LeapDiagDeviceContext* ctx,
    uint16_t                     counter_id,
    uint64_t*                    value_out)
{
    if (ctx == NULL || value_out == NULL)
    {
        return 0;
    }

    switch (counter_id)
    {
    case LEAP_COUNTER_RX_FRAMES_ACCEPTED:
        *value_out = ctx->rx_frames_accepted;
        return 1;
    case LEAP_COUNTER_RX_FRAMES_REJECTED:
        *value_out = ctx->rx_frames_rejected;
        return 1;
    case LEAP_COUNTER_TX_FRAMES_ACCEPTED:
        *value_out = ctx->tx_frames_accepted;
        return 1;
    case LEAP_COUNTER_TX_FRAMES_DROPPED:
        *value_out = ctx->tx_frames_dropped;
        return 1;
    case LEAP_COUNTER_CRC_FAILURES:
        *value_out = ctx->crc_failures;
        return 1;
    case LEAP_COUNTER_BAD_LENGTH_FAILURES:
        *value_out = ctx->bad_length_failures;
        return 1;
    case LEAP_COUNTER_UNSUPPORTED_MESSAGES:
        *value_out = ctx->unsupported_messages;
        return 1;
    case LEAP_COUNTER_DUPLICATE_SEQUENCES:
        *value_out = ctx->duplicate_sequences;
        return 1;
    case LEAP_COUNTER_LEASE_EXPIRATIONS:
        *value_out = ctx->lease_expirations;
        return 1;
    case LEAP_COUNTER_STATE_TRANSITION_REJECTS:
        *value_out = ctx->state_transition_rejects;
        return 1;
    case LEAP_COUNTER_PROCESS_CYCLES_ACCEPTED:
        *value_out = ctx->process_cycles_accepted;
        return 1;
    case LEAP_COUNTER_PROCESS_CYCLES_MISSED:
        *value_out = ctx->process_cycles_missed;
        return 1;
    case LEAP_COUNTER_STALE_PROCESS_FRAMES:
        *value_out = ctx->stale_process_frames;
        return 1;
    case LEAP_COUNTER_LATE_PROCESS_FRAMES:
        *value_out = ctx->late_process_frames;
        return 1;
    case LEAP_COUNTER_OUT_OF_ORDER_FRAMES:
        *value_out = ctx->out_of_order_frames;
        return 1;
    case LEAP_COUNTER_REPLY_TIMEOUTS:
        *value_out = ctx->reply_timeouts;
        return 1;
    case LEAP_COUNTER_MAX_REPLY_LATENCY_US:
        *value_out = ctx->max_reply_latency_us;
        return 1;
    case LEAP_COUNTER_LAST_REPLY_LATENCY_US:
        *value_out = ctx->last_reply_latency_us;
        return 1;
    case LEAP_COUNTER_SWITCH_CONGESTION_HINTS:
        *value_out = ctx->switch_congestion_hints;
        return 1;
    default:
        *value_out = 0u;
        return 0;
    }
}

static size_t leap_diag_build_counters_reply(
    const LeapDiagDeviceContext*    ctx,
    const LeapReadCountersRequest*  req,
    uint8_t*                        out,
    size_t                          out_capacity)
{
    LeapCountersReply* hdr;
    LeapCounterEntry*  entry;
    size_t             total;
    uint16_t           i;

    if (ctx == NULL || req == NULL || out == NULL || req->counter_count == 0u)
    {
        return 0u;
    }

    total = sizeof(LeapCountersReply) +
            ((size_t)req->counter_count * sizeof(LeapCounterEntry));
    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    hdr = (LeapCountersReply*)out;
    hdr->counter_count = req->counter_count;
    entry              = (LeapCounterEntry*)(out + sizeof(LeapCountersReply));

    for (i = 0u; i < req->counter_count; i++)
    {
        uint16_t id = (uint16_t)(req->first_counter_id + i);
        uint64_t value = 0u;

        entry[i].counter_id = id;
        (void)leap_diag_get_counter(ctx, id, &value);
        entry[i].value = value;
    }

    return total;
}

static size_t leap_diag_build_timing_reply(
    const LeapDiagDeviceContext* ctx,
    LeapMgmtDeviceContext*       mgmt,
    uint64_t                       now_us,
    uint8_t*                       out,
    size_t                           out_capacity)
{
    LeapTimingReply* reply;

    if (ctx == NULL || mgmt == NULL || out == NULL)
    {
        return 0u;
    }

    if (out_capacity < sizeof(LeapTimingReply))
    {
        return 0u;
    }

    memset(out, 0, sizeof(LeapTimingReply));
    reply = (LeapTimingReply*)out;

    reply->last_cycle_time_us = ctx->last_cycle_time_us;
    reply->max_cycle_time_us  = ctx->max_cycle_time_us;
    reply->min_cycle_time_us  = ctx->min_cycle_time_us;
    reply->last_reply_latency_us = (uint32_t)ctx->last_reply_latency_us;
    reply->max_reply_latency_us  = (uint32_t)ctx->max_reply_latency_us;

    if (mgmt->watchdog_deadline_us > now_us)
    {
        reply->process_watchdog_remaining_us =
            leap_diag_remaining_us(mgmt->watchdog_deadline_us, now_us);
    }

    if (mgmt->owner_active != 0u && mgmt->lease_deadline_us > now_us)
    {
        reply->owner_lease_remaining_us =
            leap_diag_remaining_us(mgmt->lease_deadline_us, now_us);
    }

    return sizeof(LeapTimingReply);
}

static size_t leap_diag_build_events_reply(
    const LeapDiagDeviceContext* ctx,
    const LeapReadEventsRequest* req,
    uint8_t*                     out,
    size_t                       out_capacity)
{
    LeapEventsReply* hdr;
    LeapEventEntry*  entry;
    LeapEventEntry   selected[LEAP_DIAG_DEVICE_MAX_EVENTS];
    size_t           selected_count = 0u;
    size_t           total;
    size_t           i;
    size_t           slot;

    if (ctx == NULL || req == NULL || out == NULL || req->max_events == 0u)
    {
        return 0u;
    }

    for (i = 0u; i < ctx->config.max_events; i++)
    {
        const LeapEventEntry* candidate = &ctx->events[i];

        if (candidate->event_id == 0u)
        {
            continue;
        }

        if (candidate->event_index < req->start_index)
        {
            continue;
        }

        if (selected_count >= (size_t)req->max_events)
        {
            break;
        }

        selected[selected_count++] = *candidate;
    }

    total = sizeof(LeapEventsReply) + (selected_count * sizeof(LeapEventEntry));
    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    hdr = (LeapEventsReply*)out;
    entry = (LeapEventEntry*)(out + sizeof(LeapEventsReply));

    for (slot = 0u; slot < selected_count; slot++)
    {
        entry[slot] = selected[slot];
    }

    hdr->next_index  = ctx->event_next_index;
    hdr->event_count = (uint16_t)selected_count;
    return total;
}

void leap_diag_device_init(LeapDiagDeviceContext* ctx, const LeapDiagDeviceConfig* config)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    if (config != NULL)
    {
        ctx->config = *config;
    }

    if (ctx->config.max_events == 0u)
    {
        ctx->config.max_events = LEAP_DIAG_DEVICE_MAX_EVENTS;
    }

    if (ctx->config.max_events > LEAP_DIAG_DEVICE_MAX_EVENTS)
    {
        ctx->config.max_events = LEAP_DIAG_DEVICE_MAX_EVENTS;
    }
}

void leap_diag_device_on_transport_ready(LeapDiagDeviceContext* ctx, uint64_t now_us)
{
    if (ctx == NULL || ctx->boot_event_recorded != 0)
    {
        return;
    }

    leap_diag_device_record_event(
        ctx,
        (uint16_t)LEAP_EVENT_BOOT,
        0u,
        0u,
        now_us);
    ctx->boot_event_recorded = 1;
}

void leap_diag_device_record_event(
    LeapDiagDeviceContext* ctx,
    uint16_t               event_id,
    uint32_t               detail0,
    uint32_t               detail1,
    uint64_t               now_us)
{
    LeapEventEntry* slot;
    uint32_t        capacity;

    if (ctx == NULL)
    {
        return;
    }

    capacity = ctx->config.max_events;
    if (capacity == 0u)
    {
        return;
    }

    slot = &ctx->events[ctx->event_next_index % capacity];
    memset(slot, 0, sizeof(*slot));
    slot->event_index  = ctx->event_next_index++;
    slot->timestamp_us = now_us;
    slot->event_id     = event_id;
    slot->detail0      = detail0;
    slot->detail1      = detail1;

    if (ctx->event_total < capacity)
    {
        ctx->event_total++;
    }
}

void leap_diag_device_on_frame_parse_error(
    LeapDiagDeviceContext* ctx,
    LeapFrameParseResult   error)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->rx_frames_rejected++;

    switch (error)
    {
    case LEAP_FRAME_ERR_BAD_HEADER_CRC:
    case LEAP_FRAME_ERR_BAD_PAYLOAD_CRC:
        ctx->crc_failures++;
        break;
    case LEAP_FRAME_ERR_BAD_LENGTH:
    case LEAP_FRAME_ERR_BAD_MAGIC:
    case LEAP_FRAME_ERR_TOO_SHORT:
        ctx->bad_length_failures++;
        break;
    default:
        break;
    }
}

void leap_diag_device_on_frame_accepted(LeapDiagDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->rx_frames_accepted++;
}

static int leap_diag_device_should_sample_reply_latency(
    const LeapDiagDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return 0;
    }

    if (LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL <= 1u)
    {
        return 1;
    }

    return (ctx->tx_frames_accepted % LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL) == 0u;
}

void leap_diag_device_on_frame_transmitted(
    LeapDiagDeviceContext* ctx,
    uint64_t               reply_latency_us)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->tx_frames_accepted++;

    if (reply_latency_us == 0u)
    {
        return;
    }

    if (reply_latency_us > ctx->max_reply_latency_us)
    {
        ctx->max_reply_latency_us = reply_latency_us;
    }

    if (!leap_diag_device_should_sample_reply_latency(ctx))
    {
        return;
    }

    ctx->last_reply_latency_us = reply_latency_us;
}

void leap_diag_device_on_frame_tx_dropped(LeapDiagDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->tx_frames_dropped++;
}

void leap_diag_device_on_pd_cycle_time(
    LeapDiagDeviceContext* ctx,
    uint32_t               cycle_time_us)
{
    if (ctx == NULL || cycle_time_us == 0u)
    {
        return;
    }

    if (LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL > 1u &&
        (ctx->tx_frames_accepted %
         LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL) != 0u)
    {
        return;
    }

    ctx->last_cycle_time_us = cycle_time_us;

    if (ctx->min_cycle_time_us == 0u || cycle_time_us < ctx->min_cycle_time_us)
    {
        ctx->min_cycle_time_us = cycle_time_us;
    }

    if (cycle_time_us > ctx->max_cycle_time_us)
    {
        ctx->max_cycle_time_us = cycle_time_us;
    }
}

void leap_diag_device_on_frame_rejected(LeapDiagDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->rx_frames_rejected++;
}

void leap_diag_device_on_unsupported_service(LeapDiagDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->unsupported_messages++;
    ctx->rx_frames_rejected++;
}

void leap_diag_device_on_pd_result(
    LeapDiagDeviceContext*    ctx,
    const LeapPdDeviceResult* pd_result,
    uint64_t                    now_us)
{
    if (ctx == NULL || pd_result == NULL)
    {
        return;
    }

    (void)now_us;

    if (pd_result->status == LEAP_PD_DEVICE_OK)
    {
        ctx->process_cycles_accepted++;
        if ((pd_result->flags & LEAP_PD_DEVICE_FLAG_SEQUENCE_GAP) != 0u)
        {
            ctx->out_of_order_frames++;
        }
        return;
    }

    if (pd_result->status != LEAP_PD_DEVICE_REJECTED)
    {
        return;
    }

    ctx->process_cycles_missed++;

    switch (pd_result->error_code)
    {
    case LEAP_STATUS_DUPLICATE_SEQUENCE:
        ctx->duplicate_sequences++;
        ctx->stale_process_frames++;
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_STALE_FRAME_REJECTED,
            pd_result->error_code,
            0u,
            now_us);
        break;
    case LEAP_STATUS_OUT_OF_ORDER:
        ctx->out_of_order_frames++;
        break;
    case LEAP_STATUS_STALE_FRAME:
        ctx->stale_process_frames++;
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_STALE_FRAME_REJECTED,
            pd_result->error_code,
            0u,
            now_us);
        break;
    default:
        break;
    }
}

void leap_diag_device_on_mgmt_flags(
    LeapDiagDeviceContext* ctx,
    uint32_t               flags,
    uint16_t               mgmt_message_type,
    LeapState_u16          device_state,
    uint64_t               now_us)
{
    if (ctx == NULL)
    {
        return;
    }

    if ((flags & LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED) != 0u)
    {
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_STATE_CHANGED,
            (uint32_t)device_state,
            0u,
            now_us);
    }

    if ((flags & LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED) != 0u)
    {
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_WATCHDOG_EXPIRED,
            (uint32_t)device_state,
            0u,
            now_us);
    }

    if (mgmt_message_type == LEAP_MGMT_OPEN_SESSION_REPLY &&
        (flags & LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED) != 0u)
    {
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_SESSION_OPENED,
            0u,
            0u,
            now_us);
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_OWNER_ACQUIRED,
            0u,
            0u,
            now_us);
    }

    if (mgmt_message_type == LEAP_MGMT_OWNER_RELEASE)
    {
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_OWNER_RELEASED,
            0u,
            0u,
            now_us);
    }
}

void leap_diag_device_on_tick_flags(
    LeapDiagDeviceContext* ctx,
    uint32_t               flags,
    uint64_t               now_us)
{
    if (ctx == NULL)
    {
        return;
    }

    if ((flags & LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED) != 0u)
    {
        ctx->lease_expirations++;
        leap_diag_device_record_event(
            ctx,
            (uint16_t)LEAP_EVENT_WATCHDOG_EXPIRED,
            0u,
            0u,
            now_us);
    }
}

LeapDiagDeviceStatus leap_diag_device_process_frame(
    LeapDiagDeviceContext* diag,
    LeapMgmtDeviceContext* mgmt,
    const uint8_t*         source_mac,
    uint64_t                 now_us,
    const uint8_t*         data,
    size_t                   length,
    LeapDiagDeviceResult*  result)
{
    LeapFrameParseResult parse_result;
    size_t               reply_length;

    if (result == NULL || diag == NULL || mgmt == NULL || data == NULL)
    {
        return LEAP_DIAG_DEVICE_ERROR;
    }

    memset(result, 0, sizeof(*result));

    parse_result = leap_frame_parse(data, length, &result->frame);
    if (parse_result != LEAP_FRAME_OK)
    {
        result->status = LEAP_DIAG_DEVICE_ERROR;
        return LEAP_DIAG_DEVICE_ERROR;
    }

    if (result->frame.header.service_id != (uint16_t)LEAP_SERVICE_DIAG)
    {
        result->status = LEAP_DIAG_DEVICE_NOT_DIAG;
        return LEAP_DIAG_DEVICE_NOT_DIAG;
    }

    if ((result->frame.header.flags & LEAP_FLAG_RESPONSE) != 0u)
    {
        result->status = LEAP_DIAG_DEVICE_IGNORED_RESPONSE;
        return LEAP_DIAG_DEVICE_IGNORED_RESPONSE;
    }

    if ((result->frame.header.flags & LEAP_FLAG_FRAGMENTED) != 0u)
    {
        result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_DIAG_DEVICE_BAD_LENGTH;
    }

    switch (result->frame.header.message_type)
    {
    case LEAP_DIAG_READ_COUNTERS:
    {
        const LeapReadCountersRequest* req;

        if (result->frame.payload_length < sizeof(LeapReadCountersRequest))
        {
            result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIAG_DEVICE_BAD_LENGTH;
        }

        if (!leap_diag_session_allows_read(
                mgmt,
                result->frame.header.session_id,
                source_mac,
                now_us))
        {
            result->status     = LEAP_DIAG_DEVICE_INVALID_STATE;
            result->error_code = LEAP_STATUS_INVALID_STATE;
            return LEAP_DIAG_DEVICE_INVALID_STATE;
        }

        req = (const LeapReadCountersRequest*)result->frame.payload;
        reply_length = leap_diag_build_counters_reply(
            diag,
            req,
            result->payload,
            LEAP_DIAG_DEVICE_MAX_REPLY);
        if (reply_length == 0u)
        {
            result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIAG_DEVICE_BAD_LENGTH;
        }

        result->message_type   = LEAP_DIAG_COUNTERS_REPLY;
        result->payload_length = reply_length;
        result->status         = LEAP_DIAG_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIAG_DEVICE_OK;
    }

    case LEAP_DIAG_READ_TIMING:
    {
        if (result->frame.payload_length < sizeof(LeapReadTimingRequest))
        {
            result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIAG_DEVICE_BAD_LENGTH;
        }

        if (!leap_diag_session_allows_read(
                mgmt,
                result->frame.header.session_id,
                source_mac,
                now_us))
        {
            result->status     = LEAP_DIAG_DEVICE_INVALID_STATE;
            result->error_code = LEAP_STATUS_INVALID_STATE;
            return LEAP_DIAG_DEVICE_INVALID_STATE;
        }

        reply_length = leap_diag_build_timing_reply(
            diag,
            mgmt,
            now_us,
            result->payload,
            LEAP_DIAG_DEVICE_MAX_REPLY);
        if (reply_length == 0u)
        {
            result->status = LEAP_DIAG_DEVICE_ERROR;
            return LEAP_DIAG_DEVICE_ERROR;
        }

        result->message_type   = LEAP_DIAG_TIMING_REPLY;
        result->payload_length = reply_length;
        result->status         = LEAP_DIAG_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIAG_DEVICE_OK;
    }

    case LEAP_DIAG_READ_EVENTS:
    {
        const LeapReadEventsRequest* req;

        if (result->frame.payload_length < sizeof(LeapReadEventsRequest))
        {
            result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIAG_DEVICE_BAD_LENGTH;
        }

        if (!leap_diag_session_allows_read(
                mgmt,
                result->frame.header.session_id,
                source_mac,
                now_us))
        {
            result->status     = LEAP_DIAG_DEVICE_INVALID_STATE;
            result->error_code = LEAP_STATUS_INVALID_STATE;
            return LEAP_DIAG_DEVICE_INVALID_STATE;
        }

        req = (const LeapReadEventsRequest*)result->frame.payload;
        reply_length = leap_diag_build_events_reply(
            diag,
            req,
            result->payload,
            LEAP_DIAG_DEVICE_MAX_REPLY);
        if (reply_length == 0u)
        {
            result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIAG_DEVICE_BAD_LENGTH;
        }

        result->message_type   = LEAP_DIAG_EVENTS_REPLY;
        result->payload_length = reply_length;
        result->status         = LEAP_DIAG_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIAG_DEVICE_OK;
    }

    case LEAP_DIAG_TRACE_MARK:
    {
        const LeapTraceMarkRequest* req;

        if (result->frame.payload_length < sizeof(LeapTraceMarkRequest))
        {
            result->status     = LEAP_DIAG_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIAG_DEVICE_BAD_LENGTH;
        }

        if (!leap_diag_session_allows_read(
                mgmt,
                result->frame.header.session_id,
                source_mac,
                now_us))
        {
            result->status     = LEAP_DIAG_DEVICE_INVALID_STATE;
            result->error_code = LEAP_STATUS_INVALID_STATE;
            return LEAP_DIAG_DEVICE_INVALID_STATE;
        }

        req = (const LeapTraceMarkRequest*)result->frame.payload;
        leap_diag_device_record_event(
            diag,
            (uint16_t)LEAP_EVENT_VENDOR_FIRST,
            req->trace_id,
            req->value0,
            now_us);

        result->status     = LEAP_DIAG_DEVICE_NO_REPLY;
        result->error_code = LEAP_STATUS_OK;
        return LEAP_DIAG_DEVICE_NO_REPLY;
    }

    default:
        diag->unsupported_messages++;
        result->status     = LEAP_DIAG_DEVICE_UNSUPPORTED_MESSAGE;
        result->error_code = LEAP_STATUS_UNSUPPORTED_MESSAGE;
        return LEAP_DIAG_DEVICE_UNSUPPORTED_MESSAGE;
    }
}
