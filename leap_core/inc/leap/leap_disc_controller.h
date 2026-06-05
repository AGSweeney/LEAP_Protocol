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

size_t leap_disc_controller_build_identify(
    const uint8_t* target_mac,
    uint16_t       request_flags,
    uint8_t*       payload,
    size_t         payload_capacity);

size_t leap_disc_controller_build_locate_device(
    uint16_t duration_ms,
    uint8_t  pattern,
    uint8_t  flags,
    uint8_t* payload,
    size_t   payload_capacity);

LeapDiscControllerStatus leap_disc_controller_on_hello_reply(
    const uint8_t*  payload,
    size_t          payload_length,
    LeapHelloReply* out);

LeapDiscControllerStatus leap_disc_controller_on_identify_reply(
    const uint8_t*     payload,
    size_t             payload_length,
    LeapIdentifyReply* out);

LeapDiscControllerStatus leap_disc_controller_on_locate_device_reply(
    const uint8_t*          payload,
    size_t                  payload_length,
    LeapLocateDeviceReply* out);

/*
 * Parse trailing supported_service_count uint16_t IDs after fixed reply header.
 * Returns number of service IDs written (0 on parse error).
 */
size_t leap_disc_parse_supported_services(
    const uint8_t* payload,
    size_t         payload_length,
    uint16_t*      services_out,
    size_t         services_capacity);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DISC_CONTROLLER_H */
