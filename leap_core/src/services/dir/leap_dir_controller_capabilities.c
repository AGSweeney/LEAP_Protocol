/*
 * leap_dir_controller_capabilities.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_dir_controller_capabilities.h"

#include "leap/leap_disc_controller.h"

#include <string.h>

void leap_dir_controller_capabilities_init(LeapDirControllerCapabilities* caps)
{
    if (caps == NULL)
    {
        return;
    }

    memset(caps, 0, sizeof(*caps));
}

uint16_t leap_dir_profile_nominal_io_bits(uint32_t profile_id)
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

static uint16_t leap_dir_bits_from_byte_length(uint16_t byte_length, uint16_t nominal_bits)
{
    uint32_t bits;

    if (byte_length == 0u)
    {
        return nominal_bits;
    }

    bits = (uint32_t)byte_length * 8u;
    if (nominal_bits > 0u && bits > nominal_bits)
    {
        return nominal_bits;
    }

    return (uint16_t)bits;
}

static void leap_dir_caps_apply_endpoints(LeapDirControllerCapabilities* caps)
{
    size_t       i;
    uint32_t     profile_id;
    uint16_t     nominal_bits;
    LeapPdProfileMap map;

    if (caps == NULL)
    {
        return;
    }

    profile_id = caps->active_profile_id;
    if (profile_id == 0u)
    {
        profile_id = caps->default_profile_id;
    }
    if (profile_id == 0u && caps->has_profile_descriptor)
    {
        profile_id = caps->profile.profile_id;
    }

    nominal_bits = leap_dir_profile_nominal_io_bits(profile_id);

    memset(&map, 0, sizeof(map));
    map.profile_id = profile_id;
    map.valid      = (profile_id != 0u) ? 1 : 0;

    for (i = 0u; i < caps->endpoint_count; i++)
    {
        const LeapEndpointDescriptor* ep = &caps->endpoints[i];

        if (ep->endpoint_id == LEAP_ENDPOINT_DIGITAL_OUTPUTS ||
            (ep->direction == (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE &&
             caps->has_digital_outputs == 0))
        {
            caps->has_digital_outputs = 1;
            caps->output_byte_length  = ep->byte_length;
            caps->output_bit_count    =
                leap_dir_bits_from_byte_length(ep->byte_length, nominal_bits);
            map.write_endpoint_id = ep->endpoint_id;
            if (ep->byte_length > 0u)
            {
                map.endpoint_payload_size = (size_t)ep->byte_length;
            }
        }
        else if (ep->endpoint_id == LEAP_ENDPOINT_DIGITAL_INPUTS ||
                 (ep->direction == (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER &&
                  caps->has_digital_inputs == 0))
        {
            caps->has_digital_inputs = 1;
            caps->input_byte_length  = ep->byte_length;
            caps->input_bit_count    =
                leap_dir_bits_from_byte_length(ep->byte_length, nominal_bits);
            map.read_endpoint_id = ep->endpoint_id;
            if (map.endpoint_payload_size == 0u && ep->byte_length > 0u)
            {
                map.endpoint_payload_size = (size_t)ep->byte_length;
            }
        }
        else if (ep->direction ==
                 (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE)
        {
            map.write_endpoint_id = ep->endpoint_id;
            if (ep->byte_length > 0u)
            {
                map.endpoint_payload_size = (size_t)ep->byte_length;
            }
        }
        else if (ep->direction ==
                 (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER)
        {
            map.read_endpoint_id = ep->endpoint_id;
            if (map.endpoint_payload_size == 0u && ep->byte_length > 0u)
            {
                map.endpoint_payload_size = (size_t)ep->byte_length;
            }
        }
    }

    if (map.endpoint_payload_size == 0u && profile_id != 0u)
    {
        (void)leap_pd_profile_map_from_profile_id(profile_id, &map);
    }

    caps->pd_map = map;

    /*
     * Fallback for devices that report a standard digital I/O profile and a
     * plausible endpoint_count via LEAP-DIR (READ_DIRECTORY TLVs or
     * PROFILE_REPLY / profile object), but send zeroed or incomplete
     * LeapEndpointDescriptor entries (endpoint_id=0, direction=0, byte_length=0).
     *
     * This lets conformance (and other capability consumers) derive the
     * expected output/input counts and generate PD masks from the profile
     * the device itself declared, without inventing capabilities when
     * endpoint_count==0 (per spec requirements). If the device later sends
     * proper descriptors (with ids or directions), the explicit loop above
     * will have already set the precise values and byte lengths.
     */
    if (nominal_bits > 0u && caps->endpoint_count > 0u)
    {
        if (!caps->has_digital_outputs)
        {
            caps->has_digital_outputs = 1;
            caps->output_bit_count    = nominal_bits;
            caps->output_byte_length  = (uint16_t)(nominal_bits / 8u);
            if (map.write_endpoint_id == 0u)
            {
                map.write_endpoint_id = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
            }
            if (map.endpoint_payload_size == 0u)
            {
                map.endpoint_payload_size = (size_t)caps->output_byte_length;
            }
        }
        if (!caps->has_digital_inputs && caps->endpoint_count >= 2u)
        {
            caps->has_digital_inputs = 1;
            caps->input_bit_count    = nominal_bits;
            caps->input_byte_length  = (uint16_t)(nominal_bits / 8u);
            if (map.read_endpoint_id == 0u)
            {
                map.read_endpoint_id = LEAP_ENDPOINT_DIGITAL_INPUTS;
            }
            if (map.endpoint_payload_size == 0u)
            {
                map.endpoint_payload_size = (size_t)caps->input_byte_length;
            }
        }
    }

    /*
     * Standard digital I/O profiles carry LeapProfileDigital* structs on the
     * wire (e.g. 8 bytes for 16x16), not the raw output bit width in bytes.
     */
    if (map.valid != 0)
    {
        size_t nominal_pd = leap_pd_endpoint_payload_size(
            map.profile_id,
            map.write_endpoint_id);
        if (nominal_pd == 0u)
        {
            nominal_pd = leap_pd_endpoint_payload_size(
                map.profile_id,
                map.read_endpoint_id);
        }
        if (nominal_pd > map.endpoint_payload_size)
        {
            map.endpoint_payload_size = nominal_pd;
        }
    }

    caps->pd_map = map;
}

void leap_dir_controller_capabilities_finalize(
    LeapDirControllerCapabilities* caps)
{
    if (caps == NULL)
    {
        return;
    }

    leap_dir_caps_apply_endpoints(caps);
    caps->has_locate = (caps->locate_capability_flags != 0u) ? 1 : 0;
    caps->valid      = 1;
}

LeapDirControllerStatus leap_dir_controller_on_profile_reply_full(
    const uint8_t*               payload,
    size_t                         payload_length,
    LeapDirControllerCapabilities* caps)
{
    const LeapProfileReply* reply;
    size_t                  expected;
    size_t                  i;

    if (payload == NULL || caps == NULL)
    {
        return LEAP_DIR_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapProfileReply))
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    reply = (const LeapProfileReply*)payload;
    caps->active_profile_id = reply->active_profile_id;
    caps->endpoint_count    = 0u;

    if (reply->endpoint_count > LEAP_DIR_CTRL_MAX_ENDPOINTS)
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    expected = sizeof(LeapProfileReply) +
               ((size_t)reply->endpoint_count * sizeof(LeapEndpointDescriptor));
    if (payload_length < expected)
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    for (i = 0u; i < (size_t)reply->endpoint_count; i++)
    {
        memcpy(
            &caps->endpoints[i],
            payload + sizeof(LeapProfileReply) + (i * sizeof(LeapEndpointDescriptor)),
            sizeof(LeapEndpointDescriptor));
    }
    caps->endpoint_count = (size_t)reply->endpoint_count;

    caps->profile.profile_id       = reply->active_profile_id;
    caps->profile.endpoint_count = reply->endpoint_count;
    caps->profile.profile_flags  = reply->profile_flags;
    caps->has_profile_descriptor = 1;

    leap_dir_controller_capabilities_finalize(caps);
    return LEAP_DIR_CTRL_OK;
}

LeapDirControllerStatus leap_dir_controller_parse_directory_tlvs(
    const uint8_t*               tlv_data,
    size_t                         tlv_length,
    LeapDirControllerCapabilities* caps)
{
    size_t offset = 0u;

    if (caps == NULL)
    {
        return LEAP_DIR_CTRL_ERROR;
    }

    while (offset + sizeof(LeapTlvHeader) <= tlv_length)
    {
        const LeapTlvHeader* hdr;
        size_t               padded;
        size_t               total;
        const uint8_t*       value;

        hdr = (const LeapTlvHeader*)(tlv_data + offset);
        if (hdr->length > tlv_length - offset - sizeof(LeapTlvHeader))
        {
            break;
        }

        value  = tlv_data + offset + sizeof(LeapTlvHeader);
        padded = LEAP_TLV_PADDED_LENGTH(hdr->length);
        total  = sizeof(LeapTlvHeader) + padded;
        if (offset + total > tlv_length)
        {
            break;
        }

        switch (hdr->type)
        {
        case LEAP_TLV_DEVICE_IDENTITY:
            if (hdr->length >= (uint16_t)sizeof(LeapIdentity))
            {
                memcpy(&caps->identity, value, sizeof(LeapIdentity));
            }
            break;

        case LEAP_TLV_DEFAULT_PROFILE_ID:
            if (hdr->length >= (uint16_t)sizeof(uint32_t))
            {
                memcpy(&caps->default_profile_id, value, sizeof(uint32_t));
            }
            break;

        case LEAP_TLV_ACTIVE_PROFILE_ID:
            if (hdr->length >= (uint16_t)sizeof(uint32_t))
            {
                memcpy(&caps->active_profile_id, value, sizeof(uint32_t));
            }
            break;

        case LEAP_TLV_PROFILE_DESCRIPTOR:
            if (hdr->length >= (uint16_t)sizeof(LeapProfileDescriptor))
            {
                memcpy(&caps->profile, value, sizeof(LeapProfileDescriptor));
                caps->has_profile_descriptor = 1;
            }
            break;

        case LEAP_TLV_ENDPOINT_DESCRIPTOR:
            if (hdr->length >= (uint16_t)sizeof(LeapEndpointDescriptor) &&
                caps->endpoint_count < LEAP_DIR_CTRL_MAX_ENDPOINTS)
            {
                memcpy(
                    &caps->endpoints[caps->endpoint_count],
                    value,
                    sizeof(LeapEndpointDescriptor));
                caps->endpoint_count++;
            }
            break;

        case LEAP_TLV_LOCATE_CAPABILITY:
            if (hdr->length >= (uint16_t)sizeof(uint16_t))
            {
                memcpy(&caps->locate_capability_flags, value, sizeof(uint16_t));
            }
            break;

        default:
            break;
        }

        offset += total;
    }

    leap_dir_controller_capabilities_finalize(caps);
    return LEAP_DIR_CTRL_OK;
}

LeapDirControllerStatus leap_dir_controller_parse_profile_object(
    const uint8_t*               object_bytes,
    size_t                         object_length,
    LeapDirControllerCapabilities* caps)
{
    const LeapProfileDescriptor* desc;
    size_t                       endpoint_bytes;
    size_t                       endpoint_count;
    size_t                       i;

    if (object_bytes == NULL || caps == NULL ||
        object_length < sizeof(LeapProfileDescriptor))
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    desc = (const LeapProfileDescriptor*)object_bytes;
    caps->profile                = *desc;
    caps->has_profile_descriptor = 1;
    caps->active_profile_id      = desc->profile_id;

    endpoint_bytes  = object_length - sizeof(LeapProfileDescriptor);
    endpoint_count  = endpoint_bytes / sizeof(LeapEndpointDescriptor);
    if (endpoint_count == 0u || endpoint_count > LEAP_DIR_CTRL_MAX_ENDPOINTS)
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    caps->endpoint_count = 0u;
    for (i = 0u; i < endpoint_count; i++)
    {
        memcpy(
            &caps->endpoints[i],
            object_bytes + sizeof(LeapProfileDescriptor) +
                (i * sizeof(LeapEndpointDescriptor)),
            sizeof(LeapEndpointDescriptor));
        caps->endpoint_count++;
    }

    leap_dir_controller_capabilities_finalize(caps);
    return LEAP_DIR_CTRL_OK;
}

void leap_dir_controller_capabilities_from_identify(
    const LeapIdentifyReply*       identify,
    const uint8_t*                 payload,
    size_t                           payload_length,
    LeapDirControllerCapabilities* caps)
{
    size_t service_count;

    if (identify == NULL || caps == NULL)
    {
        return;
    }

    caps->identity                = identify->identity;
    caps->default_profile_id      = identify->default_profile_id;
    caps->active_profile_id       = identify->active_profile_id;
    caps->current_state           = identify->current_state;
    caps->locate_capability_flags = identify->locate_capability_flags;

    if (payload != NULL)
    {
        service_count = leap_disc_parse_supported_services(
            payload,
            payload_length,
            caps->supported_services,
            sizeof(caps->supported_services) / sizeof(caps->supported_services[0]));
        caps->supported_service_count = service_count;
    }
}
