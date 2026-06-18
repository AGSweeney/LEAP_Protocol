/*
 * leap_disc_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_disc_controller.h"

#include "../../leap_wire.h"

#include <string.h>

static void leap_disc_read_identity(const uint8_t* payload, LeapIdentity* identity)
{
    memset(identity, 0, sizeof(*identity));
    memcpy(identity->primary_mac, payload + 0, 6);
    identity->vendor_id = leap_wire_read_le16(payload + 6);
    identity->product_code = leap_wire_read_le32(payload + 8);
    identity->serial_number = leap_wire_read_le32(payload + 12);
    identity->hardware_revision = leap_wire_read_le16(payload + 16);
    identity->firmware_revision = leap_wire_read_le16(payload + 18);
    identity->device_capability_flags = leap_wire_read_le32(payload + 20);
}

static void leap_disc_read_common_reply_fields(
    const uint8_t* payload,
    LeapIdentity*  identity,
    uint32_t*      default_profile_id,
    uint32_t*      active_profile_id,
    uint16_t*      current_state,
    uint16_t*      supported_service_count,
    uint8_t*       active_owner_mac,
    uint16_t*      locate_capability_flags)
{
    leap_disc_read_identity(payload, identity);
    *default_profile_id = leap_wire_read_le32(payload + 24);
    *active_profile_id = leap_wire_read_le32(payload + 28);
    *current_state = leap_wire_read_le16(payload + 32);
    *supported_service_count = leap_wire_read_le16(payload + 34);
    memcpy(active_owner_mac, payload + 36, 6);
    *locate_capability_flags = leap_wire_read_le16(payload + 42);
}

size_t leap_disc_controller_build_hello(
    uint8_t* payload,
    size_t   payload_capacity)
{
    if (payload == NULL || payload_capacity < sizeof(LeapHelloRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapHelloRequest));
    return sizeof(LeapHelloRequest);
}

size_t leap_disc_controller_build_identify(
    const uint8_t* target_mac,
    uint16_t       request_flags,
    uint8_t*       payload,
    size_t         payload_capacity)
{
    if (payload == NULL || payload_capacity < sizeof(LeapIdentifyRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapIdentifyRequest));
    if (target_mac != NULL)
    {
        memcpy(payload, target_mac, 6);
    }
    leap_wire_write_le16(payload + 6, request_flags);
    return sizeof(LeapIdentifyRequest);
}

size_t leap_disc_controller_build_locate_device(
    uint16_t duration_ms,
    uint8_t  pattern,
    uint8_t  flags,
    uint8_t* payload,
    size_t   payload_capacity)
{
    if (payload == NULL || payload_capacity < sizeof(LeapLocateDeviceRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapLocateDeviceRequest));
    leap_wire_write_le16(payload + 0, duration_ms);
    payload[2] = pattern;
    payload[3] = flags;
    return sizeof(LeapLocateDeviceRequest);
}

LeapDiscControllerStatus leap_disc_controller_on_hello_reply(
    const uint8_t*  payload,
    size_t          payload_length,
    LeapHelloReply* out)
{
    if (payload == NULL || out == NULL)
    {
        return LEAP_DISC_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapHelloReply))
    {
        return LEAP_DISC_CTRL_BAD_LENGTH;
    }

    leap_disc_read_common_reply_fields(
        payload,
        &out->identity,
        &out->default_profile_id,
        &out->active_profile_id,
        &out->current_state,
        &out->supported_service_count,
        out->active_owner_mac,
        &out->locate_capability_flags);
    return LEAP_DISC_CTRL_OK;
}

LeapDiscControllerStatus leap_disc_controller_on_identify_reply(
    const uint8_t*     payload,
    size_t             payload_length,
    LeapIdentifyReply* out)
{
    if (payload == NULL || out == NULL)
    {
        return LEAP_DISC_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapIdentifyReply))
    {
        return LEAP_DISC_CTRL_BAD_LENGTH;
    }

    leap_disc_read_common_reply_fields(
        payload,
        &out->identity,
        &out->default_profile_id,
        &out->active_profile_id,
        &out->current_state,
        &out->supported_service_count,
        out->active_owner_mac,
        &out->locate_capability_flags);
    return LEAP_DISC_CTRL_OK;
}

LeapDiscControllerStatus leap_disc_controller_on_locate_device_reply(
    const uint8_t*          payload,
    size_t                  payload_length,
    LeapLocateDeviceReply* out)
{
    if (payload == NULL || out == NULL)
    {
        return LEAP_DISC_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapLocateDeviceReply))
    {
        return LEAP_DISC_CTRL_BAD_LENGTH;
    }

    memset(out, 0, sizeof(*out));
    out->supported = payload[0];
    out->active = payload[1];
    out->remaining_ms = leap_wire_read_le16(payload + 2);
    return LEAP_DISC_CTRL_OK;
}

size_t leap_disc_parse_supported_services(
    const uint8_t* payload,
    size_t         payload_length,
    uint16_t*      services_out,
    size_t         services_capacity)
{
    size_t   offset;
    size_t   i;
    uint16_t count;

    if (payload == NULL || payload_length < sizeof(LeapHelloReply))
    {
        return 0u;
    }

    count = leap_wire_read_le16(payload + 34);
    offset = sizeof(LeapHelloReply);

    if (services_out == NULL || services_capacity == 0u)
    {
        return 0u;
    }

    for (i = 0u; i < (size_t)count && i < services_capacity; i++)
    {
        if (offset + 2u > payload_length)
        {
            break;
        }

        services_out[i] =
            (uint16_t)payload[offset] |
            ((uint16_t)payload[offset + 1u] << 8);
        offset += 2u;
    }

    return i;
}
