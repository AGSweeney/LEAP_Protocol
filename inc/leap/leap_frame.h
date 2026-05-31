/*
 * leap_frame.h
 *
 * LEAP v1 frame validation and header parsing.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_FRAME_H
#define LEAP_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapFrameParseResult
{
    LEAP_FRAME_OK = 0,
    LEAP_FRAME_ERR_TOO_SHORT,
    LEAP_FRAME_ERR_BAD_MAGIC,
    LEAP_FRAME_ERR_UNSUPPORTED_VERSION,
    LEAP_FRAME_ERR_BAD_LENGTH,
    LEAP_FRAME_ERR_BAD_HEADER_CRC,
    LEAP_FRAME_ERR_BAD_PAYLOAD_CRC
} LeapFrameParseResult;

typedef struct LeapFrameView
{
    LeapHeader      header;
    const uint8_t*  payload;
    uint16_t        payload_length;
    size_t          leap_byte_count;
} LeapFrameView;

/*
 * Validate and parse a LEAP Ethernet payload.
 *
 * data/length may include transport padding beyond header + payload_length.
 * Padding bytes are ignored for CRC and length checks.
 *
 * On success, out->payload points into data. out->header is a copy of the wire
 * header. No heap allocation; no side effects beyond filling out on success.
 */
LeapFrameParseResult leap_frame_parse(
    const uint8_t* data,
    size_t         length,
    LeapFrameView* out);

const char* leap_frame_parse_result_string(LeapFrameParseResult result);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_FRAME_H */
