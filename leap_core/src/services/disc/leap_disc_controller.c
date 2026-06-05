/*
 * leap_disc_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_disc_controller.h"

#include <string.h>

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
    LeapIdentifyRequest* body;

    if (payload == NULL || payload_capacity < sizeof(LeapIdentifyRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapIdentifyRequest));
    body = (LeapIdentifyRequest*)payload;
    if (target_mac != NULL)
    {
        memcpy(body->target_mac, target_mac, 6);
    }
    body->request_flags = request_flags;
    return sizeof(LeapIdentifyRequest);
}

size_t leap_disc_controller_build_locate_device(
    uint16_t duration_ms,
    uint8_t  pattern,
    uint8_t  flags,
    uint8_t* payload,
    size_t   payload_capacity)
{
    LeapLocateDeviceRequest* body;

    if (payload == NULL || payload_capacity < sizeof(LeapLocateDeviceRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapLocateDeviceRequest));
    body = (LeapLocateDeviceRequest*)payload;
    body->duration_ms = duration_ms;
    body->pattern     = pattern;
    body->flags       = flags;
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

    memcpy(out, payload, sizeof(LeapHelloReply));
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

    memcpy(out, payload, sizeof(LeapIdentifyReply));
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

    memcpy(out, payload, sizeof(LeapLocateDeviceReply));
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

    count = ((const LeapHelloReply*)payload)->supported_service_count;
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
