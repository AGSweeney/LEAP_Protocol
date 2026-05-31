/*
 * leap_pd_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_pd_device.h"

#include <string.h>

static int leap_pd_message_supported(uint16_t message_type)
{
    switch (message_type)
    {
    case LEAP_PD_WRITE_ENDPOINT:
    case LEAP_PD_EXCHANGE_ENDPOINTS:
        return 1;
    default:
        return 0;
    }
}

LeapPdDeviceStatus leap_pd_device_process_frame(
    LeapMgmtDeviceContext* ctx,
    const uint8_t*         source_mac,
    uint64_t               now_us,
    const uint8_t*         data,
    size_t                 length,
    LeapPdDeviceResult*    result)
{
    LeapFrameParseResult parse_result;
    const LeapEndpointDataHeader* endpoint;

    if (result == NULL)
    {
        return LEAP_PD_DEVICE_REJECTED;
    }

    memset(result, 0, sizeof(*result));

    if (ctx == NULL || source_mac == NULL || data == NULL)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_INVALID_STATE;
        return LEAP_PD_DEVICE_REJECTED;
    }

    parse_result = leap_frame_parse(data, length, &result->frame);
    if (parse_result != LEAP_FRAME_OK)
    {
        result->status = LEAP_PD_DEVICE_REJECTED;
        return LEAP_PD_DEVICE_REJECTED;
    }

    if (result->frame.header.service_id != (uint16_t)LEAP_SERVICE_PD)
    {
        result->status = LEAP_PD_DEVICE_NOT_PD;
        return LEAP_PD_DEVICE_NOT_PD;
    }

    if ((result->frame.header.flags & LEAP_FLAG_RESPONSE) != 0u)
    {
        result->status = LEAP_PD_DEVICE_IGNORED_RESPONSE;
        return LEAP_PD_DEVICE_IGNORED_RESPONSE;
    }

    if ((result->frame.header.flags & LEAP_FLAG_FRAGMENTED) != 0u)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    if (!leap_pd_message_supported(result->frame.header.message_type))
    {
        result->status     = LEAP_PD_DEVICE_UNSUPPORTED_MESSAGE;
        result->error_code = LEAP_STATUS_UNSUPPORTED_MESSAGE;
        return LEAP_PD_DEVICE_UNSUPPORTED_MESSAGE;
    }

    if (result->frame.payload_length < sizeof(LeapEndpointDataHeader))
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    endpoint = (const LeapEndpointDataHeader*)result->frame.payload;

    if (!leap_mgmt_device_session_allows_owner_pd(
            ctx,
            result->frame.header.session_id,
            source_mac))
    {
        if (ctx->device_state == LEAP_STATE_OP && ctx->owner_active != 0u)
        {
            leap_mgmt_device_force_safe(ctx);
        }

        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_NOT_OWNER;
        return LEAP_PD_DEVICE_REJECTED;
    }

    result->flags |= LEAP_PD_DEVICE_FLAG_PROCESSED;
    leap_mgmt_device_refresh_owner_lease(ctx, now_us);
    leap_mgmt_device_refresh_process_watchdog(ctx, now_us);
    result->flags |= (uint32_t)(LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED);

    if ((endpoint->endpoint_flags & LEAP_PD_FLAG_APPLY_OUTPUTS) != 0u)
    {
        result->flags |= LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED;
    }

    result->status     = LEAP_PD_DEVICE_OK;
    result->error_code = LEAP_STATUS_OK;
    return LEAP_PD_DEVICE_OK;
}
