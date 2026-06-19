/*
 * leap_dir_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_dir_device.h"

#include "../../leap_wire.h"

#include <string.h>

#define LEAP_DIR_IDENTITY_OBJECT_ID \
    LEAP_OBJECT_ID(LEAP_OBJ_NS_IDENTITY, 0x0001u)

#define LEAP_DIR_PROFILE_OBJECT_ID \
    LEAP_OBJECT_ID(LEAP_OBJ_NS_ENDPOINT_PROFILE, 0x0001u)

static int leap_dir_mac_equal(const uint8_t* a, const uint8_t* b)
{
    return (memcmp(a, b, 6) == 0);
}

static void leap_dir_write_identity(uint8_t* out, const LeapIdentity* identity)
{
    memcpy(out + 0, identity->primary_mac, 6);
    leap_wire_write_le16(out + 6, identity->vendor_id);
    leap_wire_write_le32(out + 8, identity->product_code);
    leap_wire_write_le32(out + 12, identity->serial_number);
    leap_wire_write_le16(out + 16, identity->hardware_revision);
    leap_wire_write_le16(out + 18, identity->firmware_revision);
    leap_wire_write_le32(out + 20, identity->device_capability_flags);
}

static void leap_dir_write_profile_descriptor(uint8_t* out, const LeapProfileDescriptor* profile)
{
    leap_wire_write_le32(out + 0, profile->profile_id);
    leap_wire_write_le16(out + 4, profile->profile_revision);
    leap_wire_write_le16(out + 6, profile->endpoint_count);
    leap_wire_write_le32(out + 8, profile->profile_flags);
    leap_wire_write_le32(out + 12, profile->schema_object_id);
}

static void leap_dir_write_endpoint_descriptor(uint8_t* out, const LeapEndpointDescriptor* endpoint)
{
    leap_wire_write_le16(out + 0, endpoint->endpoint_id);
    out[2] = endpoint->direction;
    out[3] = endpoint->flags;
    leap_wire_write_le32(out + 4, endpoint->profile_id);
    leap_wire_write_le16(out + 8, endpoint->byte_length);
    out[10] = endpoint->alignment;
    out[11] = endpoint->reserved;
    leap_wire_write_le32(out + 12, endpoint->schema_object_id);
}

static size_t leap_dir_append_tlv(
    uint8_t*       out,
    size_t         out_capacity,
    size_t         offset,
    uint16_t       tlv_type,
    const uint8_t* value,
    uint16_t       value_length)
{
    size_t         total;
    size_t         padded;
    size_t         pad_bytes;

    total = (size_t)LEAP_TLV_TOTAL_LENGTH(value_length);
    if (offset + total > out_capacity)
    {
        return 0u;
    }

    leap_wire_write_le16(out + offset + 0, tlv_type);
    leap_wire_write_le16(out + offset + 2, value_length);
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

static uint16_t leap_dir_profile_max_bits(uint32_t profile_id)
{
    switch (profile_id)
    {
    case LEAP_PROFILE_DIGITAL_IO_8X8:
        return 8u;
    case LEAP_PROFILE_DIGITAL_IO_16X16:
        return 16u;
    case LEAP_PROFILE_DIGITAL_IO_32X32:
        return 32u;
    default:
        return 0u;
    }
}

static uint16_t leap_dir_io_byte_length(uint16_t bit_count)
{
    if (bit_count == 0u)
    {
        return 0u;
    }

    return (uint16_t)((bit_count + 7u) / 8u);
}

int leap_dir_device_config_set_digital_io(
    LeapDirDeviceConfig* config,
    uint32_t             profile_id,
    uint16_t             output_bit_count,
    uint16_t             input_bit_count)
{
    LeapDirDeviceProfile* profile;
    uint16_t              max_bits;
    uint16_t              endpoint_index;
    uint16_t              byte_length;

    if (config == NULL)
    {
        return -1;
    }

    max_bits = leap_dir_profile_max_bits(profile_id);
    if (max_bits == 0u)
    {
        return -1;
    }

    if (output_bit_count == 0u && input_bit_count == 0u)
    {
        return -1;
    }

    if (output_bit_count > max_bits || input_bit_count > max_bits)
    {
        return -1;
    }

    memset(config->profiles, 0, sizeof(config->profiles));
    config->profile_count = 0u;

    profile = &config->profiles[0];
    profile->descriptor.profile_id       = profile_id;
    profile->descriptor.profile_revision = 1u;
    profile->descriptor.profile_flags    = 0u;
    profile->descriptor.schema_object_id = LEAP_DIR_PROFILE_OBJECT_ID;

    endpoint_index = 0u;

    if (output_bit_count > 0u)
    {
        LeapEndpointDescriptor* ep = &profile->endpoints[endpoint_index];

        byte_length = leap_dir_io_byte_length(output_bit_count);
        ep->endpoint_id      = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
        ep->direction        = (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE;
        ep->flags            = (uint8_t)LEAP_ENDPOINT_FLAG_FIXED;
        ep->profile_id       = profile_id;
        ep->byte_length      = byte_length;
        ep->alignment        = byte_length;
        ep->schema_object_id = LEAP_DIR_PROFILE_OBJECT_ID;
        endpoint_index++;
    }

    if (input_bit_count > 0u)
    {
        LeapEndpointDescriptor* ep = &profile->endpoints[endpoint_index];

        byte_length = leap_dir_io_byte_length(input_bit_count);
        ep->endpoint_id      = LEAP_ENDPOINT_DIGITAL_INPUTS;
        ep->direction        = (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER;
        ep->flags            =
            (uint8_t)(LEAP_ENDPOINT_FLAG_FIXED | LEAP_ENDPOINT_FLAG_READABLE_SAFE);
        ep->profile_id       = profile_id;
        ep->byte_length      = byte_length;
        ep->alignment        = byte_length;
        ep->schema_object_id = LEAP_DIR_PROFILE_OBJECT_ID;
        endpoint_index++;
    }

    profile->endpoint_count              = endpoint_index;
    profile->descriptor.endpoint_count   = endpoint_index;
    config->profile_count                = 1u;
    config->default_profile_id           = profile_id;
    config->active_profile_id            = profile_id;

    return 0;
}

static void leap_dir_install_default_digital_profile(LeapDirDeviceContext* ctx)
{
    if (ctx == NULL || ctx->config.profile_count > 0u)
    {
        return;
    }

    (void)leap_dir_device_config_set_digital_io(
        &ctx->config,
        LEAP_PROFILE_DIGITAL_IO_16X16,
        16u,
        16u);

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
    uint8_t                     identity_bytes[sizeof(LeapIdentity)];
    uint8_t                     u32_bytes[sizeof(uint32_t)];
    uint8_t                     profile_bytes[sizeof(LeapProfileDescriptor)];
    uint8_t                     endpoint_bytes[sizeof(LeapEndpointDescriptor)];

    profile = leap_dir_find_profile(ctx, ctx->config.active_profile_id);
    if (profile == NULL)
    {
        return 0u;
    }

    active_id = ctx->config.active_profile_id;
    leap_dir_write_identity(identity_bytes, &ctx->config.identity);

    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_DEVICE_IDENTITY,
        identity_bytes,
        (uint16_t)sizeof(LeapIdentity));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    leap_wire_write_le32(u32_bytes, ctx->config.default_profile_id);
    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_DEFAULT_PROFILE_ID,
        u32_bytes,
        (uint16_t)sizeof(uint32_t));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    leap_wire_write_le32(u32_bytes, active_id);
    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_ACTIVE_PROFILE_ID,
        u32_bytes,
        (uint16_t)sizeof(uint32_t));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    leap_dir_write_profile_descriptor(profile_bytes, &profile->descriptor);
    chunk = leap_dir_append_tlv(
        out,
        out_capacity,
        offset,
        LEAP_TLV_PROFILE_DESCRIPTOR,
        profile_bytes,
        (uint16_t)sizeof(LeapProfileDescriptor));
    if (chunk == 0u)
    {
        return 0u;
    }
    offset += chunk;

    for (i = 0u; i < profile->endpoint_count; i++)
    {
        leap_dir_write_endpoint_descriptor(endpoint_bytes, &profile->endpoints[i]);
        chunk = leap_dir_append_tlv(
            out,
            out_capacity,
            offset,
            LEAP_TLV_ENDPOINT_DESCRIPTOR,
            endpoint_bytes,
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
    leap_wire_write_le32(out + 0, ctx->config.active_profile_id);
    leap_wire_write_le16(out + 4, (uint16_t)profile->endpoint_count);
    leap_wire_write_le16(out + 6, (uint16_t)profile->descriptor.profile_flags);

    for (i = 0u; i < profile->endpoint_count; i++)
    {
        leap_dir_write_endpoint_descriptor(
            out + sizeof(LeapProfileReply) + (i * sizeof(LeapEndpointDescriptor)),
            &profile->endpoints[i]);
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

        memset(result->payload, 0, sizeof(LeapReadDirectoryReply));
        reply_length = leap_dir_build_directory_tlvs(
            dir,
            result->payload + sizeof(LeapReadDirectoryReply),
            LEAP_DIR_DEVICE_MAX_REPLY - sizeof(LeapReadDirectoryReply));
        if (reply_length == 0u)
        {
            result->status = LEAP_DIR_DEVICE_ERROR;
            return LEAP_DIR_DEVICE_ERROR;
        }

        leap_wire_write_le16(result->payload + 4, (uint16_t)reply_length);
        leap_wire_write_le16(result->payload + 6, (uint16_t)reply_length);

        result->message_type   = LEAP_DIR_READ_DIRECTORY_REPLY;
        result->payload_length = sizeof(LeapReadDirectoryReply) + reply_length;
        result->status         = LEAP_DIR_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIR_DEVICE_OK;

    case LEAP_DIR_READ_OBJECT:
    {
        uint32_t object_id;
        uint32_t object_offset;
        uint32_t object_length;

        if (result->frame.payload_length < sizeof(LeapReadObjectRequest))
        {
            result->status     = LEAP_DIR_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DIR_DEVICE_BAD_LENGTH;
        }

        object_id = leap_wire_read_le32(result->frame.payload + 0);
        object_offset = leap_wire_read_le32(result->frame.payload + 4);
        object_length = leap_wire_read_le32(result->frame.payload + 8);
        memset(result->payload, 0, sizeof(LeapReadObjectReply));
        object_bytes = leap_dir_read_object_bytes(
            dir,
            object_id,
            object_offset,
            object_length,
            result->payload + sizeof(LeapReadObjectReply),
            LEAP_DIR_DEVICE_MAX_REPLY - sizeof(LeapReadObjectReply));
        if (object_bytes == 0u)
        {
            result->status     = LEAP_DIR_DEVICE_ERROR;
            result->error_code = LEAP_STATUS_RANGE;
            return LEAP_DIR_DEVICE_ERROR;
        }

        leap_wire_write_le32(result->payload + 0, object_id);
        leap_wire_write_le32(result->payload + 4, object_offset);
        leap_wire_write_le32(result->payload + 8, (uint32_t)object_bytes);
        leap_wire_write_le32(result->payload + 12, 0u);

        result->message_type   = LEAP_DIR_READ_OBJECT_REPLY;
        result->payload_length = sizeof(LeapReadObjectReply) + object_bytes;
        result->status         = LEAP_DIR_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DIR_DEVICE_OK;
    }

    case LEAP_DIR_SELECT_PROFILE:
    {
        const LeapDirDeviceProfile*       profile;
        uint32_t requested_profile_id;

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

        requested_profile_id = leap_wire_read_le32(result->frame.payload + 0);
        if (!leap_dir_owner_authorized(
                mgmt,
                source_mac,
                result->frame.header.session_id))
        {
            result->status     = LEAP_DIR_DEVICE_NOT_OWNER;
            result->error_code = LEAP_STATUS_NOT_OWNER;
            return LEAP_DIR_DEVICE_NOT_OWNER;
        }

        profile = leap_dir_find_profile(dir, requested_profile_id);
        if (profile == NULL)
        {
            result->status     = LEAP_DIR_DEVICE_PROFILE_MISMATCH;
            result->error_code = LEAP_STATUS_PROFILE_MISMATCH;
            return LEAP_DIR_DEVICE_PROFILE_MISMATCH;
        }

        dir->config.active_profile_id = requested_profile_id;
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
