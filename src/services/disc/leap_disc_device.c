/*
 * leap_disc_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_disc_device.h"

#include <string.h>

static void leap_disc_fill_identity(
    LeapIdentity*                  dst,
    const LeapDiscDeviceConfig*    config)
{
    *dst = config->identity;
}

static size_t leap_disc_build_hello_reply(
    const LeapDiscDeviceContext* disc,
    const LeapMgmtDeviceContext* mgmt,
    uint8_t*                       out,
    size_t                         out_capacity)
{
    LeapHelloReply* body;
    size_t          service_bytes;
    size_t          total;
    size_t          i;
    uint16_t*       services;

    if (disc->config.supported_service_count > LEAP_DISC_DEVICE_MAX_SERVICES)
    {
        return 0u;
    }

    service_bytes = disc->config.supported_service_count * sizeof(uint16_t);
    total         = sizeof(LeapHelloReply) + service_bytes;

    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    body = (LeapHelloReply*)out;
    leap_disc_fill_identity(&body->identity, &disc->config);
    body->default_profile_id       = disc->config.default_profile_id;
    body->active_profile_id        = disc->config.active_profile_id;
    body->current_state            = (uint16_t)leap_mgmt_device_get_state(mgmt);
    body->supported_service_count  = (uint16_t)disc->config.supported_service_count;

    if (mgmt->owner_active != 0u)
    {
        (void)memcpy(body->active_owner_mac, mgmt->owner_mac, 6);
    }

    services = (uint16_t*)(out + sizeof(LeapHelloReply));
    for (i = 0u; i < disc->config.supported_service_count; i++)
    {
        services[i] = disc->config.supported_services[i];
    }

    return total;
}

static size_t leap_disc_build_identify_reply(
    const LeapDiscDeviceContext* disc,
    const LeapMgmtDeviceContext* mgmt,
    uint8_t*                       out,
    size_t                         out_capacity)
{
    LeapIdentifyReply* body;
    size_t             service_bytes;
    size_t             total;
    size_t             i;
    uint16_t*          services;

    service_bytes = disc->config.supported_service_count * sizeof(uint16_t);
    total         = sizeof(LeapIdentifyReply) + service_bytes;

    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    body = (LeapIdentifyReply*)out;
    leap_disc_fill_identity(&body->identity, &disc->config);
    body->default_profile_id      = disc->config.default_profile_id;
    body->active_profile_id       = disc->config.active_profile_id;
    body->current_state           = (uint16_t)leap_mgmt_device_get_state(mgmt);
    body->supported_service_count = (uint16_t)disc->config.supported_service_count;

    if (mgmt->owner_active != 0u)
    {
        (void)memcpy(body->active_owner_mac, mgmt->owner_mac, 6);
    }

    services = (uint16_t*)(out + sizeof(LeapIdentifyReply));
    for (i = 0u; i < disc->config.supported_service_count; i++)
    {
        services[i] = disc->config.supported_services[i];
    }

    return total;
}

void leap_disc_device_init(LeapDiscDeviceContext* ctx, const LeapDiscDeviceConfig* config)
{
    static const uint16_t k_default_services[] = {
        (uint16_t)LEAP_SERVICE_MGMT,
        (uint16_t)LEAP_SERVICE_DISC,
        (uint16_t)LEAP_SERVICE_DIR,
        (uint16_t)LEAP_SERVICE_PD,
    };

    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    if (config != NULL)
    {
        ctx->config = *config;
    }

    if (ctx->config.supported_service_count == 0u)
    {
        size_t i;

        ctx->config.supported_service_count = 4u;
        for (i = 0u; i < 4u; i++)
        {
            ctx->config.supported_services[i] = k_default_services[i];
        }
    }
}

LeapDiscDeviceStatus leap_disc_device_process_frame(
    const LeapDiscDeviceContext* disc,
    const LeapMgmtDeviceContext* mgmt,
    const uint8_t*               source_mac,
    const uint8_t*               data,
    size_t                         length,
    LeapDiscDeviceResult*        result)
{
    LeapFrameParseResult parse_result;
    size_t               reply_length;

    (void)source_mac;

    if (result == NULL || disc == NULL || mgmt == NULL || data == NULL)
    {
        return LEAP_DISC_DEVICE_ERROR;
    }

    memset(result, 0, sizeof(*result));

    parse_result = leap_frame_parse(data, length, &result->frame);
    if (parse_result != LEAP_FRAME_OK)
    {
        result->status = LEAP_DISC_DEVICE_ERROR;
        return LEAP_DISC_DEVICE_ERROR;
    }

    if (result->frame.header.service_id != (uint16_t)LEAP_SERVICE_DISC)
    {
        result->status = LEAP_DISC_DEVICE_NOT_DISC;
        return LEAP_DISC_DEVICE_NOT_DISC;
    }

    if ((result->frame.header.flags & LEAP_FLAG_RESPONSE) != 0u)
    {
        result->status = LEAP_DISC_DEVICE_IGNORED_RESPONSE;
        return LEAP_DISC_DEVICE_IGNORED_RESPONSE;
    }

    if ((result->frame.header.flags & LEAP_FLAG_FRAGMENTED) != 0u)
    {
        result->status     = LEAP_DISC_DEVICE_ERROR;
        result->error_code = LEAP_STATUS_BAD_LENGTH;
        return LEAP_DISC_DEVICE_ERROR;
    }

    switch (result->frame.header.message_type)
    {
    case LEAP_DISC_HELLO:
        if (result->frame.payload_length < sizeof(LeapHelloRequest))
        {
            result->status     = LEAP_DISC_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DISC_DEVICE_BAD_LENGTH;
        }

        reply_length = leap_disc_build_hello_reply(
            disc,
            mgmt,
            result->payload,
            LEAP_DISC_DEVICE_MAX_REPLY);
        if (reply_length == 0u)
        {
            result->status = LEAP_DISC_DEVICE_ERROR;
            return LEAP_DISC_DEVICE_ERROR;
        }

        result->message_type   = LEAP_DISC_HELLO_REPLY;
        result->payload_length = reply_length;
        result->status         = LEAP_DISC_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DISC_DEVICE_OK;

    case LEAP_DISC_IDENTIFY:
        if (result->frame.payload_length < sizeof(LeapIdentifyRequest))
        {
            result->status     = LEAP_DISC_DEVICE_BAD_LENGTH;
            result->error_code = LEAP_STATUS_BAD_LENGTH;
            return LEAP_DISC_DEVICE_BAD_LENGTH;
        }

        reply_length = leap_disc_build_identify_reply(
            disc,
            mgmt,
            result->payload,
            LEAP_DISC_DEVICE_MAX_REPLY);
        if (reply_length == 0u)
        {
            result->status = LEAP_DISC_DEVICE_ERROR;
            return LEAP_DISC_DEVICE_ERROR;
        }

        result->message_type   = LEAP_DISC_IDENTIFY_REPLY;
        result->payload_length = reply_length;
        result->status         = LEAP_DISC_DEVICE_OK;
        result->error_code     = LEAP_STATUS_OK;
        return LEAP_DISC_DEVICE_OK;

    default:
        result->status     = LEAP_DISC_DEVICE_UNSUPPORTED_MESSAGE;
        result->error_code = LEAP_STATUS_UNSUPPORTED_MESSAGE;
        return LEAP_DISC_DEVICE_UNSUPPORTED_MESSAGE;
    }
}
