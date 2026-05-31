/*
 * leap_test_frame.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_test_frame.h"

#include "leap/leap_crc.h"

#include <string.h>

static void leap_test_frame_finalize_header_crc(uint8_t* frame)
{
    LeapHeader* header = (LeapHeader*)frame;
    uint8_t     temp[LEAP_HEADER_LENGTH_V1];

    memcpy(temp, frame, LEAP_HEADER_LENGTH_V1);
    temp[26] = 0u;
    temp[27] = 0u;
    header->header_crc16 = leap_crc16_xmodem(temp, LEAP_HEADER_LENGTH_V1);
}

int leap_test_frame_build(
    uint8_t*       out,
    size_t         out_capacity,
    size_t*        out_length,
    uint8_t        flags,
    uint16_t       service_id,
    uint16_t       message_type,
    uint32_t       session_id,
    uint32_t       sequence,
    const uint8_t* payload,
    size_t         payload_length)
{
    LeapHeader* header;
    size_t      total_length;

    if (out == NULL || out_length == NULL ||
        out_capacity < (size_t)LEAP_HEADER_LENGTH_V1 + payload_length)
    {
        return -1;
    }

    total_length = (size_t)LEAP_HEADER_LENGTH_V1 + payload_length;
    memset(out, 0, total_length);

    header = (LeapHeader*)out;
    header->magic          = LEAP_MAGIC_U32;
    header->version_major  = LEAP_VERSION_MAJOR;
    header->version_minor  = LEAP_VERSION_MINOR;
    header->header_length  = LEAP_HEADER_LENGTH_V1;
    header->flags          = flags;
    header->service_id     = service_id;
    header->message_type   = message_type;
    header->session_id     = session_id;
    header->sequence       = sequence;
    header->payload_length = (uint16_t)payload_length;

    if (payload != NULL && payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        header->payload_crc32c = leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length);
    }

    leap_test_frame_finalize_header_crc(out);
    *out_length = total_length;
    return 0;
}

int leap_test_frame_compare_header(const LeapHeader* expected, const LeapHeader* actual)
{
    if (expected == NULL || actual == NULL)
    {
        return -1;
    }

    if (expected->magic != actual->magic ||
        expected->version_major != actual->version_major ||
        expected->version_minor != actual->version_minor ||
        expected->header_length != actual->header_length ||
        expected->flags != actual->flags ||
        expected->service_id != actual->service_id ||
        expected->message_type != actual->message_type ||
        expected->session_id != actual->session_id ||
        expected->sequence != actual->sequence ||
        expected->ack_sequence != actual->ack_sequence ||
        expected->payload_length != actual->payload_length)
    {
        return -1;
    }

    return 0;
}

void leap_test_frame_mutate_byte(uint8_t* frame, size_t frame_length, uint32_t seed)
{
    uint32_t index;

    if (frame == NULL || frame_length == 0u)
    {
        return;
    }

    index = seed % (uint32_t)frame_length;
    frame[index] ^= (uint8_t)(0x01u << (seed & 3u));
}
