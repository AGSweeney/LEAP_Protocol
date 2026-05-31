/*
 * leap_frame.c
 *
 * Strict LEAP v1 frame validation. Rejects malformed frames without side effects.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_frame.h"

#include "leap/leap_crc.h"

#include <string.h>

static uint16_t leap_header_crc16_compute(const uint8_t* header_bytes, uint8_t header_length)
{
    uint8_t  temp[LEAP_HEADER_LENGTH_V1];
    uint16_t crc;

    if (header_bytes == NULL || header_length < LEAP_HEADER_LENGTH_V1)
    {
        return 0u;
    }

    memcpy(temp, header_bytes, (size_t)header_length);
    temp[26] = 0u;
    temp[27] = 0u;

    crc = leap_crc16_xmodem(temp, (size_t)header_length);
    return crc;
}

static int leap_magic_valid(const uint8_t* data)
{
    return (data[0] == LEAP_MAGIC_B0 &&
            data[1] == LEAP_MAGIC_B1 &&
            data[2] == LEAP_MAGIC_B2 &&
            data[3] == LEAP_MAGIC_B3);
}

const char* leap_frame_parse_result_string(LeapFrameParseResult result)
{
    switch (result)
    {
    case LEAP_FRAME_OK:
        return "ok";
    case LEAP_FRAME_ERR_TOO_SHORT:
        return "too_short";
    case LEAP_FRAME_ERR_BAD_MAGIC:
        return "bad_magic";
    case LEAP_FRAME_ERR_UNSUPPORTED_VERSION:
        return "unsupported_version";
    case LEAP_FRAME_ERR_BAD_LENGTH:
        return "bad_length";
    case LEAP_FRAME_ERR_BAD_HEADER_CRC:
        return "bad_header_crc";
    case LEAP_FRAME_ERR_BAD_PAYLOAD_CRC:
        return "bad_payload_crc";
    default:
        return "unknown";
    }
}

LeapFrameParseResult leap_frame_parse(
    const uint8_t* data,
    size_t         length,
    LeapFrameView* out)
{
    LeapHeader     header;
    uint16_t       computed_header_crc;
    uint16_t       stored_header_crc;
    uint32_t       computed_payload_crc;
    uint32_t       stored_payload_crc;
    const uint8_t* payload;
    size_t         required_length;

    if (out != NULL)
    {
        memset(out, 0, sizeof(*out));
    }

    if (data == NULL || out == NULL)
    {
        return LEAP_FRAME_ERR_TOO_SHORT;
    }

    if (length < LEAP_HEADER_LENGTH_V1)
    {
        return LEAP_FRAME_ERR_TOO_SHORT;
    }

    if (!leap_magic_valid(data))
    {
        return LEAP_FRAME_ERR_BAD_MAGIC;
    }

    memcpy(&header, data, sizeof(header));

    if (header.version_major != LEAP_VERSION_MAJOR)
    {
        return LEAP_FRAME_ERR_UNSUPPORTED_VERSION;
    }

    if (header.header_length < LEAP_HEADER_LENGTH_V1)
    {
        return LEAP_FRAME_ERR_BAD_LENGTH;
    }

    if (length < (size_t)header.header_length)
    {
        return LEAP_FRAME_ERR_TOO_SHORT;
    }

    if (header.payload_length > LEAP_MAX_PAYLOAD_V1)
    {
        return LEAP_FRAME_ERR_BAD_LENGTH;
    }

    required_length = (size_t)header.header_length + (size_t)header.payload_length;
    if (length < required_length)
    {
        return LEAP_FRAME_ERR_BAD_LENGTH;
    }

    stored_header_crc = header.header_crc16;
    computed_header_crc = leap_header_crc16_compute(data, header.header_length);
    if (computed_header_crc != stored_header_crc)
    {
        return LEAP_FRAME_ERR_BAD_HEADER_CRC;
    }

    payload = data + header.header_length;

    if (header.payload_length == 0u)
    {
        out->header           = header;
        out->payload          = payload;
        out->payload_length   = 0u;
        out->leap_byte_count  = required_length;
        return LEAP_FRAME_OK;
    }

    if ((header.flags & LEAP_FLAG_NO_PAYLOAD_CRC) != 0u)
    {
        out->header           = header;
        out->payload          = payload;
        out->payload_length   = header.payload_length;
        out->leap_byte_count  = required_length;
        return LEAP_FRAME_OK;
    }

    stored_payload_crc = header.payload_crc32c;
    computed_payload_crc = leap_crc32c(payload, (size_t)header.payload_length);
    if (computed_payload_crc != stored_payload_crc)
    {
        return LEAP_FRAME_ERR_BAD_PAYLOAD_CRC;
    }

    out->header           = header;
    out->payload          = payload;
    out->payload_length   = header.payload_length;
    out->leap_byte_count  = required_length;
    return LEAP_FRAME_OK;
}
