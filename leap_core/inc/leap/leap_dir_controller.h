/*
 * leap_dir_controller.h
 *
 * Controller-side LEAP-DIR request builders and reply handlers.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DIR_CONTROLLER_H
#define LEAP_DIR_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapDirControllerStatus
{
    LEAP_DIR_CTRL_OK = 0,
    LEAP_DIR_CTRL_BAD_LENGTH,
    LEAP_DIR_CTRL_ERROR
} LeapDirControllerStatus;

typedef struct LeapDirControllerProfileInfo
{
    uint32_t active_profile_id;
    uint16_t endpoint_count;
    uint16_t profile_flags;
} LeapDirControllerProfileInfo;

size_t leap_dir_controller_build_select_profile(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t profile_id,
    uint32_t profile_flags);

LeapDirControllerStatus leap_dir_controller_on_profile_reply(
    const uint8_t*               payload,
    size_t                         payload_length,
    LeapDirControllerProfileInfo* info);

size_t leap_dir_controller_build_read_directory(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t start_object_id,
    uint16_t max_bytes);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DIR_CONTROLLER_H */
