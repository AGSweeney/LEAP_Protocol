/*
 * leap_mgmt_process.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_mgmt_process.h"

#include <string.h>

typedef struct LeapMgmtSnapshot
{
    LeapState_u16 device_state;
    uint8_t       owner_active;
    uint32_t      owner_session_id;
    uint8_t       owner_mac[LEAP_MGMT_DEVICE_MAC_LEN];
} LeapMgmtSnapshot;

static void leap_mgmt_snapshot(const LeapMgmtDeviceContext* ctx, LeapMgmtSnapshot* snap)
{
    snap->device_state     = ctx->device_state;
    snap->owner_active     = ctx->owner_active;
    snap->owner_session_id = ctx->owner_session_id;
    (void)memcpy(snap->owner_mac, ctx->owner_mac, LEAP_MGMT_DEVICE_MAC_LEN);
}

static void leap_mgmt_apply_diff_flags(
    const LeapMgmtSnapshot* before,
    const LeapMgmtDeviceContext* after,
    uint32_t* flags)
{
    if (before->owner_active != after->owner_active ||
        before->owner_session_id != after->owner_session_id ||
        memcmp(before->owner_mac, after->owner_mac, LEAP_MGMT_DEVICE_MAC_LEN) != 0)
    {
        *flags |= LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED;
    }

    if (before->device_state != after->device_state)
    {
        *flags |= LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED;
    }

    if (after->device_state == LEAP_STATE_SAFE && before->device_state != LEAP_STATE_SAFE)
    {
        *flags |= LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED;
    }
}

static LeapMgmtProcessStatus leap_mgmt_map_handle_status(
    LeapMgmtDeviceHandleStatus handle_status,
    LeapMgmtProcessResult*     result)
{
    switch (handle_status)
    {
    case LEAP_MGMT_DEVICE_HANDLE_OK:
        result->flags |= LEAP_MGMT_PROCESS_FLAG_PROCESSED;
        if (result->reply.message_type != 0u)
        {
            result->flags |= LEAP_MGMT_PROCESS_FLAG_HAS_REPLY;
        }
        return LEAP_MGMT_PROCESS_OK;
    case LEAP_MGMT_DEVICE_HANDLE_NO_REPLY:
        result->flags |= LEAP_MGMT_PROCESS_FLAG_PROCESSED;
        return LEAP_MGMT_PROCESS_OK;
    case LEAP_MGMT_DEVICE_HANDLE_ERROR:
    default:
        result->status = LEAP_MGMT_PROCESS_HANDLER_ERROR;
        return LEAP_MGMT_PROCESS_HANDLER_ERROR;
    }
}

const char* leap_mgmt_process_status_string(LeapMgmtProcessStatus status)
{
    switch (status)
    {
    case LEAP_MGMT_PROCESS_OK:
        return "ok";
    case LEAP_MGMT_PROCESS_FRAME_ERROR:
        return "frame_error";
    case LEAP_MGMT_PROCESS_NOT_MGMT:
        return "not_mgmt";
    case LEAP_MGMT_PROCESS_IGNORED_RESPONSE:
        return "ignored_response";
    case LEAP_MGMT_PROCESS_UNSUPPORTED_FRAME:
        return "unsupported_frame";
    case LEAP_MGMT_PROCESS_HANDLER_ERROR:
        return "handler_error";
    default:
        return "unknown";
    }
}

LeapMgmtProcessStatus leap_mgmt_process_frame(
    LeapMgmtDeviceContext* ctx,
    const uint8_t*         source_mac,
    uint64_t               now_us,
    const uint8_t*         data,
    size_t                 length,
    LeapMgmtProcessResult* result)
{
    LeapFrameParseResult     parse_result;
    LeapMgmtSnapshot         before;
    LeapMgmtDeviceRequest    request;
    LeapMgmtDeviceHandleStatus handle_status;

    if (result == NULL)
    {
        return LEAP_MGMT_PROCESS_HANDLER_ERROR;
    }

    memset(result, 0, sizeof(*result));

    if (ctx == NULL || source_mac == NULL || data == NULL)
    {
        result->status = LEAP_MGMT_PROCESS_HANDLER_ERROR;
        return LEAP_MGMT_PROCESS_HANDLER_ERROR;
    }

    parse_result = leap_frame_parse(data, length, &result->frame);
    if (parse_result != LEAP_FRAME_OK)
    {
        result->status      = LEAP_MGMT_PROCESS_FRAME_ERROR;
        result->frame_error = parse_result;
        return LEAP_MGMT_PROCESS_FRAME_ERROR;
    }

    if (result->frame.header.service_id != (uint16_t)LEAP_SERVICE_MGMT)
    {
        result->status = LEAP_MGMT_PROCESS_NOT_MGMT;
        return LEAP_MGMT_PROCESS_NOT_MGMT;
    }

    if ((result->frame.header.flags & LEAP_FLAG_RESPONSE) != 0u)
    {
        result->status = LEAP_MGMT_PROCESS_IGNORED_RESPONSE;
        return LEAP_MGMT_PROCESS_IGNORED_RESPONSE;
    }

    if ((result->frame.header.flags & LEAP_FLAG_FRAGMENTED) != 0u)
    {
        result->status = LEAP_MGMT_PROCESS_UNSUPPORTED_FRAME;
        return LEAP_MGMT_PROCESS_UNSUPPORTED_FRAME;
    }

    leap_mgmt_snapshot(ctx, &before);

    memset(&request, 0, sizeof(request));
    request.source_mac     = source_mac;
    request.session_id     = result->frame.header.session_id;
    request.message_type   = result->frame.header.message_type;
    request.payload        = result->frame.payload;
    request.payload_length = (size_t)result->frame.payload_length;
    request.now_us         = now_us;

    handle_status = leap_mgmt_device_handle(ctx, &request, &result->reply);
    result->handle_status = handle_status;
    result->error_code    = result->reply.error_code;
    result->device_state  = leap_mgmt_device_get_state(ctx);

    leap_mgmt_apply_diff_flags(&before, ctx, &result->flags);

    result->status = leap_mgmt_map_handle_status(handle_status, result);
    return result->status;
}

LeapMgmtProcessStatus leap_mgmt_process_tick(
    LeapMgmtDeviceContext* ctx,
    uint64_t               now_us,
    uint32_t*              flags_out)
{
    LeapMgmtSnapshot before;
    uint32_t         flags = 0u;

    if (flags_out != NULL)
    {
        *flags_out = 0u;
    }

    if (ctx == NULL)
    {
        return LEAP_MGMT_PROCESS_HANDLER_ERROR;
    }

    leap_mgmt_snapshot(ctx, &before);
    leap_mgmt_device_tick(ctx, now_us);
    leap_mgmt_apply_diff_flags(&before, ctx, &flags);

    if (flags != 0u)
    {
        flags |= LEAP_MGMT_PROCESS_FLAG_PROCESSED;
    }

    if (flags_out != NULL)
    {
        *flags_out = flags;
    }

    return LEAP_MGMT_PROCESS_OK;
}
