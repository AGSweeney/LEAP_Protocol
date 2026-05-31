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
