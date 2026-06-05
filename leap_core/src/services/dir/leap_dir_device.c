/*
 * leap_dir_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_dir_device.h"

#include <string.h>

#define LEAP_DIR_IDENTITY_OBJECT_ID \
    LEAP_OBJECT_ID(LEAP_OBJ_NS_IDENTITY, 0x0001u)

#define LEAP_DIR_PROFILE_OBJECT_ID \
    LEAP_OBJECT_ID(LEAP_OBJ_NS_ENDPOINT_PROFILE, 0x0001u)

static int leap_dir_mac_equal(const uint8_t* a, const uint8_t* b)
{
    return (memcmp(a, b, 6) == 0);
}

static size_t leap_dir_append_tlv(
    uint8_t*       out,
    size_t         out_capacity,
    size_t         offset,
    uint16_t       tlv_type,
    const uint8_t* value,
    uint16_t       value_length)
{
    LeapTlvHeader* hdr;
    size_t         total;
    size_t         padded;
    size_t         pad_bytes;

    total = (size_t)LEAP_TLV_TOTAL_LENGTH(value_length);
    if (offset + total > out_capacity)
    {
        return 0u;
    }

    hdr = (LeapTlvHeader*)(out + offset);
    hdr->type   = tlv_type;
    hdr->length = value_length;
    if (value_length > 0u && value != NULL)
    {
        memcpy(out + offset + sizeof(LeapTlvHeader), value, value_length);
    }

    padded   = LEAP_TLV_PADDED_LENGTH(value_length);
    pad_bytes = padded - (size_t)value_length;
    if (pad_bytes > 0u)
    {
        memset(out + offset + sizeof(LeapTlvHeader) + value_length, 0, pad_bytes);
    }

    return total;
}

static const LeapDirDeviceProfile* leap_dir_find_profile(
    const LeapDirDeviceContext* ctx,
    uint32_t                    profile_id)
{
    size_t i;

    if (ctx == NULL)
    {
        return NULL;
    }

    for (i = 0u; i < ctx->config.profile_count; i++)
    {
        if (ctx->config.profiles[i].descriptor.profile_id == profile_id)
        {
            return &ctx->config.profiles[i];
        }
    }

    return NULL;
}

static void leap_dir_install_default_digital_profile(LeapDirDeviceContext* ctx)
{
    LeapDirDeviceProfile* profile;

    if (ctx == NULL || ctx->config.profile_count > 0u)
    {
        return;
    }

    profile = &ctx->config.profiles[0];
    memset(profile, 0, sizeof(*profile));

    profile->descriptor.profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    profile->descriptor.profile_revision = 1u;
    profile->descriptor.endpoint_count   = 2u;
    profile->descriptor.profile_flags    = 0u;
    profile->descriptor.schema_object_id = LEAP_DIR_PROFILE_OBJECT_ID;

    profile->endpoints[0].endpoint_id      = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    profile->endpoints[0].direction        = (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE;
    profile->endpoints[0].flags            = (uint8_t)LEAP_ENDPOINT_FLAG_FIXED;
    profile->endpoints[0].profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    profile->endpoints[0].byte_length      = 2u;
    profile->endpoints[0].alignment        = 2u;
    profile->endpoints[0].schema_object_id = LEAP_DIR_PROFILE_OBJECT_ID;

    profile->endpoints[1].endpoint_id      = LEAP_ENDPOINT_DIGITAL_INPUTS;
    profile->endpoints[1].direction        = (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER;
    profile->endpoints[1].flags            =
        (uint8_t)(LEAP_ENDPOINT_FLAG_FIXED | LEAP_ENDPOINT_FLAG_READABLE_SAFE);
    profile->endpoints[1].profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    profile->endpoints[1].byte_length      = 2u;
    profile->endpoints[1].alignment        = 2u;
    profile->endpoints[1].schema_object_id = LEAP_DIR_PROFILE_OBJECT_ID;

    profile->endpoint_count = 2u;
    ctx->config.profile_count = 1u;

    if (ctx->config.default_profile_id == 0u)
    {
        ctx->config.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    }

    if (ctx->config.active_profile_id == 0u)
    {
        ctx->config.active_profile_id = ctx->config.default_profile_id;
    }
}

static size_t leap_dir_build_directory_tlvs(
    const LeapDirDeviceContext* ctx,
    uint8_t*                      out,
    size_t                        out_capacity)
{
    const LeapDirDeviceProfile* profile;
    size_t                      offset = 0u;
    size_t                      chunk;
    size_t                      i;
    uint32_t                    active_id;

    profile = leap_dir_find_profile(ctx, ctx->config.active_profile_id);
    if (profile == NULL)
    {
        return 0u;
    }

    active_id = ctx->config.active_profile_id;

    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_DEVICE_IDENTITY,
        (const uint8_t*)&ctx->config.identity,
        (uint16_t)sizeof(LeapIdentity));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_DEFAULT_PROFILE_ID,
        (const uint8_t*)&ctx->config.default_profile_id,
        (uint16_t)sizeof(uint32_t));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_ACTIVE_PROFILE_ID,
        (const uint8_t*)&active_id,
        (uint16_t)sizeof(uint32_t));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_PROFILE_DESCRIPTOR,
        (const uint8_t*)&profile->descriptor,
        (uint16_t)sizeof(LeapProfileDescriptor));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    for (i = 0u; i < profile->endpoint_count; i++)
    {
        chunk = leap_dir_append_tlv(
            out,
            out_capacity,
            offset,
            LEAP_TLV_ENDPOINT_DESCRIPTOR,
            (const uint8_t*)&profile->endpoints[i],
            (uint16_t)sizeof(LeapEndpointDescriptor));
        if (chunk == 0u)
        {
            return 0u;
        }
        offset += chunk;
    }

    return offset;
}

static size_t leap_dir_build_profile_reply(
    const LeapDirDeviceContext* ctx,
    uint8_t*                      out,
    size_t                        out_capacity)
{
    const LeapDirDeviceProfile* profile;
    LeapProfileReply*           body;
    size_t                      endpoint_bytes;
    size_t                      total;
    size_t                      i;

    profile = leap_dir_find_profile(ctx, ctx->config.active_profile_id);
    if (profile == NULL)
    {
        return 0u;
    }

    endpoint_bytes = profile->endpoint_count * sizeof(LeapEndpointDescriptor);
    total          = sizeof(LeapProfileReply) + endpoint_bytes;

    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    body = (LeapProfileReply*)out;
    body->active_profile_id = ctx->config.active_profile_id;
    body->endpoint_count    = (uint16_t)profile->endpoint_count;
    body->profile_flags     = profile->descriptor.profile_flags;

    for (i = 0u; i < profile->endpoint_count; i++)
    {
        memcpy(
            out + sizeof(LeapProfileReply) + (i * sizeof(LeapEndpointDescriptor)),
            &profile->endpoints[i],
            sizeof(LeapEndpointDescriptor));
    }

    return total;
}

static size_t leap_dir_read_object_bytes(
    const LeapDirDeviceContext* ctx,
    uint32_t                    object_id,
    uint32_t                    offset,
    uint32_t                    length,
    uint8_t*                      out,
    size_t                        out_capacity)
{
    const LeapDirDeviceProfile* profile;
    const uint8_t*              object_bytes = NULL;
    size_t                      object_size  = 0u;

    if (object_id == LEAP_DIR_IDENTITY_OBJECT_ID)
    {
        object_bytes = (const uint8_t*)&ctx->config.identity;
        object_size  = sizeof(LeapIdentity);
    }
    else if (object_id == LEAP_DIR_PROFILE_OBJECT_ID)
    {
        profile = leap_dir_find_profile(ctx, ctx->config.active_profile_id);
        if (profile == NULL)
        {
            return 0u;
        }

        object_bytes = (const uint8_t*)&profile->descriptor;
        object_size  = sizeof(LeapProfileDescriptor) +
                       (profile->endpoint_count * sizeof(LeapEndpointDescriptor));
    }
    else
    {
        return 0u;
    }

    if ((size_t)offset >= object_size)
    {
        return 0u;
    }

    if (length == 0u)
    {
        length = (uint32_t)(object_size - (size_t)offset);
    }

    if ((size_t)offset + (size_t)length > object_size)
    {
        length = (uint32_t)(object_size - (size_t)offset);
    }

    if ((size_t)length > out_capacity)
    {
        return 0u;
    }

    memcpy(out, object_bytes + offset, length);
    return (size_t)length;
}

static int leap_dir_owner_authorized(
    const LeapMgmtDeviceContext* mgmt,
    const uint8_t*               source_mac,
    uint32_t                     session_id)
{
    if (mgmt == NULL)
    {
        return 0;
    }

    if (mgmt->device_state == LEAP_STATE_INIT ||
        mgmt->device_state == LEAP_STATE_CONFIGURED)
    {
        return 1;
    }

    if (mgmt->owner_active == 0u)
    {
        return 0;
    }

    if (session_id != mgmt->owner_session_id)
    {
        return 0;
    }

    if (source_mac != NULL && !leap_dir_mac_equal(source_mac, mgmt->owner_mac))
    {
        return 0;
    }

    return 1;
}

void leap_dir_device_init(LeapDirDeviceContext* ctx, const LeapDirDeviceConfig* config)
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

    leap_dir_install_default_digital_profile(ctx);
}

void leap_dir_device_sync_disc(LeapDirDeviceContext* dir, LeapDiscDeviceContext* disc)
{
    if (dir == NULL || disc == NULL)
    {
        return;
    }

    disc->config.default_profile_id = dir->config.default_profile_id;
    disc->config.active_profile_id  = dir->config.active_profile_id;
    disc->config.identity           = dir->config.identity;
}

LeapDirDeviceStatus leap_dir_device_process_frame(
    LeapDirDeviceContext*  dir,
    LeapDiscDeviceContext* disc,
    LeapMgmtDeviceContext* mgmt,
    const uint8_t*         source_mac,
    const uint8_t*         data,
    size_t                   length,
    LeapDirDeviceResult*   result)
{
    LeapFrameParseResult parse_result;
    size_t               reply_length;
    LeapReadDirectoryReply* dir_hdr;
    LeapReadObjectReply*    obj_hdr;
    size_t                  object_bytes;

    if (result == NULL || dir == NULL || disc == NULL || mgmt == NULL || data == NULL)
    {
        return LEAP_DIR_DEVICE_ERROR;
    }

    memset(result, 0, sizeof(*result));

    parse_result = leap_frame_parse(data, length, &result->frame);
    if (parse_result != LEAP_FRAME_OK)
    {
        result->status = LEAP_DIR_DEVICE_ERROR;
        return LEAP_DIR_DEVICE_ERROR;
    }

    if (result->frame.header.service_id != (uint16_t)LEAP_SERVICE_DIR)
    {
        result->status = LEAP_DIR_DEVICE_NOT_DIR;
        return LEAP_DIR_DEVICE_NOT_DIR;
    }

    if ((result->frame.header.flags & LEAP_FLAG_RESPONSE) != 0u)
    {
        result->status = LEAP_DIR_DEVICE_IGNORED_RESPONSE;
        return LEAP_DIR_DEVICE_IGNORED_RESPONSE;
    }

    if ((result->frame.header.flags & LEAP_FLAG_FRAGMENTED) != 0u)
    {
        result->status     = LEAP_DIR_DEVICE_ERROR;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_DIR_DEVICE_ERROR;
    }

    switch (result->frame.header.message_type)
    {
    case LEAP_DIR_READ_DIRECTORY:
        if (result->frame.payload_length < sizeof(LeapReadDirectoryRequest))
        {
            result->status     = LEAP_DIR_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIR_DEVICE_BAD_LENGTH;
        }

        reply_length = leap_dir_build_directory_tlvs(
            dir,
            result->payload + sizeof(LeapReadDirectoryReply),
            LEAP_DIR_DEVICE_MAX_REPLY - sizeof(LeapReadDirectoryReply));
        if (reply_length == 0u)
        {
            result->status = LEAP_DIR_DEVICE_ERROR;
            return LEAP_DIR_DEVICE_ERROR;
        }

        memset(result->payload, 0, sizeof(LeapReadDirectoryReply) + reply_length);
        dir_hdr = (LeapReadDirectoryReply*)result->payload;
        dir_hdr->returned_bytes = (uint16_t)reply_length;
        dir_hdr->total_bytes    = (uint16_t)reply_length;

        result->message_type   = LEAP_DIR_READ_DIRECTORY_REPLY;
        result->payload_length = sizeof(LeapReadDirectoryReply) + reply_length;
        result->status         = LEAP_DIR_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIR_DEVICE_OK;

    case LEAP_DIR_READ_OBJECT:
    {
        const LeapReadObjectRequest* req;

        if (result->frame.payload_length < sizeof(LeapReadObjectRequest))
        {
            result->status     = LEAP_DIR_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIR_DEVICE_BAD_LENGTH;
        }

        req = (const LeapReadObjectRequest*)result->frame.payload;
        object_bytes = leap_dir_read_object_bytes(
            dir,
            req->object_id,
            req->offset,
            req->length,
            result->payload + sizeof(LeapReadObjectReply),
            LEAP_DIR_DEVICE_MAX_REPLY - sizeof(LeapReadObjectReply));
        if (object_bytes == 0u)
        {
            result->status     = LEAP_DIR_DEVICE_ERROR;
            result->error_code = LEAP_STATUS_RANGE;
            return LEAP_DIR_DEVICE_ERROR;
        }

        memset(result->payload, 0, sizeof(LeapReadObjectReply) + object_bytes);
        obj_hdr = (LeapReadObjectReply*)result->payload;
        obj_hdr->object_id    = req->object_id;
        obj_hdr->offset       = req->offset;
        obj_hdr->length       = (uint32_t)object_bytes;
        obj_hdr->object_flags = 0u;

        result->message_type   = LEAP_DIR_READ_OBJECT_REPLY;
        result->payload_length = sizeof(LeapReadObjectReply) + object_bytes;
        result->status         = LEAP_DIR_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIR_DEVICE_OK;
    }

    case LEAP_DIR_SELECT_PROFILE:
    {
        const LeapSelectProfileRequest* req;
        const LeapDirDeviceProfile*       profile;

        if (result->frame.payload_length < sizeof(LeapSelectProfileRequest))
        {
            result->status     = LEAP_DIR_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIR_DEVICE_BAD_LENGTH;
        }

        if (mgmt->device_state == LEAP_STATE_OP)
        {
            result->status     = LEAP_DIR_DEVICE_INVALID_STATE;
            result->error_code = LEAP_STATUS_INVALID_STATE;
            return LEAP_DIR_DEVICE_INVALID_STATE;
        }

        req = (const LeapSelectProfileRequest*)result->frame.payload;
        if (!leap_dir_owner_authorized(
                mgmt,
                source_mac,
                result->frame.header.session_id))
        {
            result->status     = LEAP_DIR_DEVICE_NOT_OWNER;
            result->error_code = LEAP_STATUS_NOT_OWNER;
            return LEAP_DIR_DEVICE_NOT_OWNER;
        }

        profile = leap_dir_find_profile(dir, req->requested_profile_id);
        if (profile == NULL)
        {
            result->status     = LEAP_DIR_DEVICE_PROFILE_MISMATCH;
            result->error_code = LEAP_STATUS_PROFILE_MISMATCH;
            return LEAP_DIR_DEVICE_PROFILE_MISMATCH;
        }

        dir->config.active_profile_id = req->requested_profile_id;
        leap_dir_device_sync_disc(dir, disc);

        if (mgmt->device_state == LEAP_STATE_INIT)
        {
            leap_mgmt_device_on_profile_selected(mgmt);
            result->flags |= LEAP_DIR_DEVICE_FLAG_STATE_CONFIGURED;
        }

        result->flags |= LEAP_DIR_DEVICE_FLAG_PROFILE_SELECTED;

        reply_length = leap_dir_build_profile_reply(
            dir,
            result->payload,
            LEAP_DIR_DEVICE_MAX_REPLY);
        if (reply_length == 0u)
        {
            result->status = LEAP_DIR_DEVICE_ERROR;
            return LEAP_DIR_DEVICE_ERROR;
        }

        result->message_type   = LEAP_DIR_PROFILE_REPLY;
        result->payload_length = reply_length;
        result->status         = LEAP_DIR_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIR_DEVICE_OK;
    }

    default:
        result->status     = LEAP_DIR_DEVICE_UNSUPPORTED_MESSAGE;
        result->error_code = LEAP_STATUS_UNSUPPORTED_MESSAGE;
        return LEAP_DIR_DEVICE_UNSUPPORTED_MESSAGE;
    }
}
