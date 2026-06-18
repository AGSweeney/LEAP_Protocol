/*
 * leap_dir_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_dir_controller.h"

#include "../../leap_wire.h"

#include <string.h>

size_t leap_dir_controller_build_select_profile(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t profile_id,
    uint32_t profile_flags)
{
    if (payload == NULL || payload_capacity < sizeof(LeapSelectProfileRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapSelectProfileRequest));
    leap_wire_write_le32(payload + 0, profile_id);
    leap_wire_write_le32(payload + 4, profile_flags);

    return sizeof(LeapSelectProfileRequest);
}

LeapDirControllerStatus leap_dir_controller_on_profile_reply(
    const uint8_t*               payload,
    size_t                         payload_length,
    LeapDirControllerProfileInfo* info)
{
    if (payload == NULL || info == NULL)
    {
        return LEAP_DIR_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapProfileReply))
    {
        return LEAP_DIR_CTRL_BAD_LENGTH;
    }

    info->active_profile_id = leap_wire_read_le32(payload + 0);
    info->endpoint_count    = leap_wire_read_le16(payload + 4);
    info->profile_flags     = leap_wire_read_le16(payload + 6);

    return LEAP_DIR_CTRL_OK;
}

size_t leap_dir_controller_build_read_directory(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t start_object_id,
    uint16_t max_bytes)
{
    if (payload == NULL || payload_capacity < sizeof(LeapReadDirectoryRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapReadDirectoryRequest));
    leap_wire_write_le32(payload + 4, start_object_id);
    leap_wire_write_le16(payload + 8, max_bytes);

    return sizeof(LeapReadDirectoryRequest);
}

size_t leap_dir_controller_build_read_object(
    uint8_t* payload,
    size_t   payload_capacity,
    uint32_t object_id,
    uint32_t offset,
    uint32_t length)
{
    if (payload == NULL || payload_capacity < sizeof(LeapReadObjectRequest))
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapReadObjectRequest));
    leap_wire_write_le32(payload + 0, object_id);
    leap_wire_write_le32(payload + 4, offset);
    leap_wire_write_le32(payload + 8, length);

    return sizeof(LeapReadObjectRequest);
}
