/*
 * leap_dir_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_dir_controller.h"

#include <string.h>

size_t leap_dir_controller_build_select_profile(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t profile_id,
    uint32_t profile_flags)
{
    LeapSelectProfileRequest* req;

    if (payload == NULL || payload_capacity < sizeof(LeapSelectProfileRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapSelectProfileRequest));
    req = (LeapSelectProfileRequest*)payload;
    req->requested_profile_id = profile_id;
    req->profile_flags        = profile_flags;

    return sizeof(LeapSelectProfileRequest);
}

LeapDirControllerStatus leap_dir_controller_on_profile_reply(
    const uint8_t*               payload,
    size_t                         payload_length,
    LeapDirControllerProfileInfo* info)
{
    const LeapProfileReply* reply;

    if (payload == NULL || info == NULL)
    {
        return LEAP_DIR_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapProfileReply))
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    reply = (const LeapProfileReply*)payload;
    info->active_profile_id = reply->active_profile_id;
    info->endpoint_count    = reply->endpoint_count;
    info->profile_flags     = reply->profile_flags;

    return LEAP_DIR_CTRL_OK;
}

size_t leap_dir_controller_build_read_directory(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t start_object_id,
    uint16_t max_bytes)
{
    LeapReadDirectoryRequest* req;

    if (payload == NULL || payload_capacity < sizeof(LeapReadDirectoryRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapReadDirectoryRequest));
    req = (LeapReadDirectoryRequest*)payload;
    req->start_object_id = start_object_id;
    req->max_bytes       = max_bytes;

    return sizeof(LeapReadDirectoryRequest);
}
