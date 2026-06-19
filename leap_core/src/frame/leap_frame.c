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

static uint16_t leap_read_le16(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static uint32_t leap_read_le32(const uint8_t* p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void leap_write_le16(uint8_t* p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void leap_write_le32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
    p[2] = (uint8_t)((value >> 16) & 0xFFu);
    p[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void leap_header_read_wire(const uint8_t* data, LeapHeader* header)
{
    memset(header, 0, sizeof(*header));
    header->magic = leap_read_le32(data + 0);
    header->version_major = data[4];
    header->version_minor = data[5];
    header->header_length = data[6];
    header->flags = data[7];
    header->service_id = leap_read_le16(data + 8);
    header->message_type = leap_read_le16(data + 10);
    header->session_id = leap_read_le32(data + 12);
    header->sequence = leap_read_le32(data + 16);
    header->ack_sequence = leap_read_le32(data + 20);
    header->payload_length = leap_read_le16(data + 24);
    header->header_crc16 = leap_read_le16(data + 26);
    header->payload_crc32c = leap_read_le32(data + 28);
}

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

    leap_header_read_wire(data, &header);

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

int leap_frame_write(
    uint8_t*       out,
    size_t         out_capacity,
    size_t*        out_length,
    uint8_t        flags,
    uint16_t       service_id,
    uint16_t       message_type,
    uint32_t       session_id,
    uint32_t       sequence,
    uint32_t       ack_sequence,
    const uint8_t* payload,
    size_t         payload_length)
{
    size_t      total_length;

    if (out == NULL || out_length == NULL ||
        out_capacity < (size_t)LEAP_HEADER_LENGTH_V1 + payload_length ||
        payload_length > LEAP_MAX_PAYLOAD_V1)
    {
        return -1;
    }

    if (payload_length > 0u && payload == NULL)
    {
        return -1;
    }

    total_length = (size_t)LEAP_HEADER_LENGTH_V1 + payload_length;
    memset(out, 0, total_length);

    leap_write_le32(out + 0, LEAP_MAGIC_U32);
    out[4] = LEAP_VERSION_MAJOR;
    out[5] = LEAP_VERSION_MINOR;
    out[6] = LEAP_HEADER_LENGTH_V1;
    out[7] = flags;
    leap_write_le16(out + 8, service_id);
    leap_write_le16(out + 10, message_type);
    leap_write_le32(out + 12, session_id);
    leap_write_le32(out + 16, sequence);
    leap_write_le32(out + 20, ack_sequence);
    leap_write_le16(out + 24, (uint16_t)payload_length);

    if (payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        leap_write_le32(out + 28, leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length));
    }

    leap_write_le16(out + 26, leap_header_crc16_compute(out, LEAP_HEADER_LENGTH_V1));
    *out_length          = total_length;
    return 0;
}
