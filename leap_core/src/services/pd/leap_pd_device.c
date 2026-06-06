/*
 * leap_pd_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_pd_device.h"

#include "leap/leap_log.h"

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

void leap_pd_device_init(
    LeapPdDeviceContext*       ctx,
    const LeapPdDeviceConfig* config)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config.enforce_sequence = 1;

    if (config != NULL)
    {
        ctx->config = *config;
    }

    if (ctx->config.profile.valid == 0)
    {
        leap_pd_profile_map_init_default(&ctx->config.profile);
    }
}

void leap_pd_device_sync_profile_from_dir(
    LeapPdDeviceContext*        pd,
    const LeapDirDeviceContext* dir)
{
    LeapPdProfileMap map;

    if (pd == NULL || dir == NULL)
    {
        return;
    }

    if (leap_pd_profile_map_from_dir(dir, &map) == LEAP_PD_COMMON_OK)
    {
        pd->config.profile = map;
    }
}

void leap_pd_device_reset_sequence(LeapPdDeviceContext* pd, uint32_t session_id)
{
    if (pd == NULL)
    {
        return;
    }

    pd->last_process_sequence = 0u;
    pd->sequence_active       = 0;
    pd->bound_session_id      = session_id;
}

static const LeapPdProfileMap* leap_pd_device_profile(
    const LeapPdDeviceContext* pd_ctx)
{
    static LeapPdProfileMap k_default;

    if (pd_ctx != NULL && pd_ctx->config.profile.valid != 0)
    {
        return &pd_ctx->config.profile;
    }

    leap_pd_profile_map_init_default(&k_default);
    return &k_default;
}

static int leap_pd_extract_process_sequence(
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t*      sequence_out)
{
    if (sequence_out == NULL || payload == NULL)
    {
        return -1;
    }

    if (message_type == LEAP_PD_WRITE_ENDPOINT)
    {
        if (payload_length < sizeof(LeapEndpointDataHeader))
        {
            return -1;
        }

        *sequence_out = ((const LeapEndpointDataHeader*)payload)->process_sequence;
        return 0;
    }

    if (message_type == LEAP_PD_EXCHANGE_ENDPOINTS)
    {
        if (payload_length < sizeof(LeapExchangeHeader))
        {
            return -1;
        }

        *sequence_out = ((const LeapExchangeHeader*)payload)->process_sequence;
        return 0;
    }

    return -1;
}

static int leap_pd_check_process_sequence(
    LeapPdDeviceContext* pd_ctx,
    uint32_t             session_id,
    uint32_t             process_sequence,
    LeapPdDeviceResult*  result)
{
    if (pd_ctx == NULL || pd_ctx->config.enforce_sequence == 0)
    {
        return 0;
    }

    if (pd_ctx->bound_session_id != session_id)
    {
        leap_pd_device_reset_sequence(pd_ctx, session_id);
    }

    if (pd_ctx->sequence_active == 0)
    {
        pd_ctx->last_process_sequence = process_sequence;
        pd_ctx->sequence_active       = 1;
        return 0;
    }

    if (process_sequence <= pd_ctx->last_process_sequence)
    {
        pd_ctx->stale_rejections++;
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = (process_sequence == pd_ctx->last_process_sequence)
                                 ? LEAP_STATUS_DUPLICATE_SEQUENCE
                                 : LEAP_STATUS_OUT_OF_ORDER;
        return -1;
    }

    if (process_sequence > pd_ctx->last_process_sequence + 1u)
    {
        pd_ctx->sequence_gaps++;
        result->flags |= LEAP_PD_DEVICE_FLAG_SEQUENCE_GAP;
    }

    pd_ctx->last_process_sequence = process_sequence;
    return 0;
}

static void leap_pd_apply_digital_outputs(
    const LeapPdDeviceIoBinding* io_binding,
    uint16_t                     outputs,
    LeapPdDeviceResult*          result)
{
    if (io_binding != NULL)
    {
        if (io_binding->outputs_dirty != NULL)
        {
            *io_binding->outputs_dirty = 1;
        }

        if (io_binding->apply_outputs != NULL)
        {
            io_binding->apply_outputs(outputs, io_binding->apply_outputs_ctx);
        }
    }

    result->digital_outputs_applied = outputs;
    result->flags |= LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED;
}

static LeapPdDeviceStatus leap_pd_handle_write_endpoint(
    const LeapPdProfileMap*      profile,
    const LeapPdDeviceIoBinding* io_binding,
    LeapPdDeviceResult*          result)
{
    LeapPdEndpointView view;
    uint16_t           outputs;
    LeapPdCommonStatus parse_status;

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

    if (leap_pd_profile_validate_write(
            profile,
            view.header->profile_id,
            view.header->endpoint_id,
            view.header->data_length) != LEAP_PD_COMMON_OK)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_PROFILE_MISMATCH;
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
    const LeapPdProfileMap*      profile,
    const LeapPdDeviceIoBinding* io_binding,
    LeapPdDeviceResult*          result)
{
    LeapPdExchangeView view;
    LeapExchangeStatus status;
    uint8_t            read_profile[sizeof(LeapProfileDigital16x16)];
    uint16_t           outputs = 0u;
    uint16_t           inputs  = 0u;
    size_t             reply_length;

    if (leap_pd_exchange_view(
            result->frame.payload,
            result->frame.payload_length,
            &view) != LEAP_PD_COMMON_OK)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    if (leap_pd_profile_validate_exchange(
            profile,
            view.header->profile_id,
            view.header->write_endpoint_id,
            view.header->read_endpoint_id,
            view.header->write_length,
            view.header->read_length) != LEAP_PD_COMMON_OK)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_PROFILE_MISMATCH;
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

LeapPdDeviceStatus leap_pd_device_process_parsed_frame(
    LeapMgmtDeviceContext*       mgmt,
    LeapPdDeviceContext*         pd_ctx,
    const LeapPdDeviceIoBinding* io_binding,
    const uint8_t*               source_mac,
    uint64_t                     now_us,
    const LeapFrameView*         frame,
    LeapPdDeviceResult*          result)
{
    LeapPdDeviceStatus      handler_status;
    const LeapPdProfileMap* profile;
    uint32_t                process_sequence;

    if (result == NULL)
    {
        return LEAP_PD_DEVICE_REJECTED;
    }

    memset(result, 0, sizeof(*result));

    if (mgmt == NULL || source_mac == NULL || frame == NULL)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_INVALID_STATE;
        return LEAP_PD_DEVICE_REJECTED;
    }

    result->frame = *frame;

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
            mgmt,
            result->frame.header.session_id,
            source_mac))
    {
        result->status     = LEAP_PD_DEVICE_IGNORED_RESPONSE;
        result->error_code = LEAP_STATUS_NOT_OWNER;
        leap_log_security(
            LEAP_LOG_SEC_PD_NOT_OWNER,
            "session_id=%u state=%u",
            result->frame.header.session_id,
            (unsigned)mgmt->device_state);
        return LEAP_PD_DEVICE_IGNORED_RESPONSE;
    }

    if (leap_pd_extract_process_sequence(
            result->frame.header.message_type,
            result->frame.payload,
            result->frame.payload_length,
            &process_sequence) != 0)
    {
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_PD_DEVICE_REJECTED;
    }

    if (leap_pd_check_process_sequence(
            pd_ctx,
            result->frame.header.session_id,
            process_sequence,
            result) != 0)
    {
        return LEAP_PD_DEVICE_REJECTED;
    }

    result->flags |= LEAP_PD_DEVICE_FLAG_PROCESSED;
    leap_mgmt_device_refresh_owner_lease(mgmt, now_us);
    leap_mgmt_device_refresh_process_watchdog(mgmt, now_us);
    result->flags |= (uint32_t)(LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED);

    profile = leap_pd_device_profile(pd_ctx);

    if (result->frame.header.message_type == LEAP_PD_WRITE_ENDPOINT)
    {
        handler_status = leap_pd_handle_write_endpoint(profile, io_binding, result);
    }
    else
    {
        handler_status = leap_pd_handle_exchange(profile, io_binding, result);
    }

    if (handler_status == LEAP_PD_DEVICE_OK &&
        io_binding != NULL &&
        io_binding->io_status != NULL)
    {
        *io_binding->io_status = LEAP_DIO_STATUS_OK;
    }

    return handler_status;
}

LeapPdDeviceStatus leap_pd_device_process_frame(
    LeapMgmtDeviceContext*       mgmt,
    LeapPdDeviceContext*         pd_ctx,
    const LeapPdDeviceIoBinding* io_binding,
    const uint8_t*               source_mac,
    uint64_t                     now_us,
    const uint8_t*               data,
    size_t                       length,
    LeapPdDeviceResult*          result)
{
    LeapFrameView        view;
    LeapFrameParseResult parse_result;

    if (result == NULL)
    {
        return LEAP_PD_DEVICE_REJECTED;
    }

    if (data == NULL)
    {
        memset(result, 0, sizeof(*result));
        result->status     = LEAP_PD_DEVICE_REJECTED;
        result->error_code = LEAP_STATUS_INVALID_STATE;
        return LEAP_PD_DEVICE_REJECTED;
    }

    parse_result = leap_frame_parse(data, length, &view);
    if (parse_result != LEAP_FRAME_OK)
    {
        memset(result, 0, sizeof(*result));
        result->status = LEAP_PD_DEVICE_REJECTED;
        return LEAP_PD_DEVICE_REJECTED;
    }

    return leap_pd_device_process_parsed_frame(
        mgmt,
        pd_ctx,
        io_binding,
        source_mac,
        now_us,
        &view,
        result);
}
