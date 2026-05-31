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

static void leap_pd_apply_digital_outputs(
    const LeapPdDeviceIoBinding* io_binding,
    uint16_t                     outputs,
    LeapPdDeviceResult*          result)
{
    if (io_binding != NULL && io_binding->digital_outputs != NULL)
    {
        *io_binding->digital_outputs = outputs;
    }

    result->digital_outputs_applied = outputs;
    result->flags |= LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED;
}

static LeapPdDeviceStatus leap_pd_handle_write_endpoint(
    LeapMgmtDeviceContext*       ctx,
    const LeapPdDeviceIoBinding* io_binding,
    uint64_t                     now_us,
    LeapPdDeviceResult*          result)
{
    LeapPdEndpointView view;
    uint16_t           outputs;
    LeapPdCommonStatus parse_status;

    (void)ctx;
    (void)now_us;

    parse_status = leap_pd_endpoint_view(
        result->frame.payload,
        result->frame.payload_length,
        &view);
    if (parse_status != LEAP_PD_COMMON_OK)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    if ((view.header->endpoint_flags & LEAP_PD_FLAG_APPLY_OUTPUTS) == 0u)
    {
        result->status     = LEAP_PD_DEVICE_OK;
        result->error_code = LEAP_STATUS_OK;
        return LEAP_PD_DEVICE_OK;
    }

    if (leap_pd_unpack_digital16x16_outputs(&view, &outputs) != LEAP_PD_COMMON_OK)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_PROFILE_MISMATCH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    leap_pd_apply_digital_outputs(io_binding, outputs, result);
    result->status     = LEAP_PD_DEVICE_OK;
    result->error_code = LEAP_STATUS_OK;
    return LEAP_PD_DEVICE_OK;
}

static LeapPdDeviceStatus leap_pd_handle_exchange(
    const LeapPdDeviceIoBinding* io_binding,
    uint64_t                     now_us,
    LeapPdDeviceResult*          result)
{
    LeapPdExchangeView   view;
    LeapExchangeStatus   status;
    uint8_t              read_profile[sizeof(LeapProfileDigital16x16)];
    uint16_t             outputs = 0u;
    uint16_t             inputs  = 0u;
    size_t               reply_length;

    (void)now_us;

    if (leap_pd_exchange_view(
            result->frame.payload,
            result->frame.payload_length,
            &view) != LEAP_PD_COMMON_OK)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    if (view.write_length >= sizeof(LeapProfileDigital16x16) &&
        (view.header->exchange_flags & LEAP_PD_FLAG_APPLY_OUTPUTS) != 0u)
    {
        const LeapProfileDigital16x16* write_profile =
            (const LeapProfileDigital16x16*)view.write_data;

        outputs = write_profile->digital_outputs;
        leap_pd_apply_digital_outputs(io_binding, outputs, result);
    }

    if (io_binding != NULL && io_binding->digital_inputs != NULL)
    {
        inputs = *io_binding->digital_inputs;
    }

    result->digital_inputs_snapshot = inputs;
    result->flags |= LEAP_PD_DEVICE_FLAG_INPUTS_READ;

    if (leap_pd_pack_digital16x16(
            read_profile,
            sizeof(read_profile),
            outputs,
            inputs,
            LEAP_DIO_STATUS_OK) != LEAP_PD_COMMON_OK)
    {
        result->status = LEAP_PD_DEVICE_REJECTED;
        return LEAP_PD_DEVICE_REJECTED;
    }

    memset(&status, 0, sizeof(status));
    status.latest_process_sequence_consumed = view.header->process_sequence;
    status.device_process_sequence          = view.header->process_sequence;
    status.status_code                      = LEAP_STATUS_OK;

    reply_length = leap_pd_build_exchange_reply(
        result->reply_payload,
        sizeof(result->reply_payload),
        view.header,
        view.write_data,
        view.write_length,
        read_profile,
        sizeof(read_profile),
        &status);
    if (reply_length == 0u)
    {
        result->status = LEAP_PD_DEVICE_REJECTED;
        return LEAP_PD_DEVICE_REJECTED;
    }

    result->reply_message_type   = LEAP_PD_EXCHANGE_REPLY;
    result->reply_payload_length = reply_length;
    result->flags               |= LEAP_PD_DEVICE_FLAG_HAS_REPLY;
    result->status               = LEAP_PD_DEVICE_OK;
    result->error_code           = LEAP_STATUS_OK;
    return LEAP_PD_DEVICE_OK;
}

LeapPdDeviceStatus leap_pd_device_process_frame(
    LeapMgmtDeviceContext*     ctx,
    const LeapPdDeviceIoBinding* io_binding,
    const uint8_t*             source_mac,
    uint64_t                   now_us,
    const uint8_t*             data,
    size_t                     length,
    LeapPdDeviceResult*        result)
{
    LeapFrameParseResult parse_result;
    LeapPdDeviceStatus   handler_status;

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

    if (result->frame.header.message_type == LEAP_PD_WRITE_ENDPOINT)
    {
        handler_status = leap_pd_handle_write_endpoint(
            ctx,
            io_binding,
            now_us,
            result);
    }
    else
    {
        handler_status = leap_pd_handle_exchange(io_binding, now_us, result);
    }

    if (handler_status == LEAP_PD_DEVICE_OK &&
        io_binding != NULL &&
        io_binding->io_status != NULL)
    {
        *io_binding->io_status = LEAP_DIO_STATUS_OK;
    }

    return handler_status;
}
