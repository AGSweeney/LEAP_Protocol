/*
 * leap_disc_controller.h
 *
 * Controller-side LEAP-DISC request builders and reply parsers.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DISC_CONTROLLER_H
#define LEAP_DISC_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapDiscControllerStatus
{
    LEAP_DISC_CTRL_OK = 0,
    LEAP_DISC_CTRL_BAD_LENGTH,
    LEAP_DISC_CTRL_ERROR
} LeapDiscControllerStatus;

size_t leap_disc_controller_build_hello(
    uint8_t* payload,
    size_t   payload_capacity);

LeapDiscControllerStatus leap_disc_controller_on_hello_reply(
    const uint8_t* payload,
    size_t         payload_length,
    LeapHelloReply* out);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DISC_CONTROLLER_H */
