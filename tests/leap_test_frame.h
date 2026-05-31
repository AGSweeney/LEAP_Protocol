/*
 * leap_test_frame.h
 *
 * Frame build/compare helpers for conformance tests.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_TEST_FRAME_H
#define LEAP_TEST_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

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
    size_t         payload_length);

int leap_test_frame_compare_header(const LeapHeader* expected, const LeapHeader* actual);

void leap_test_frame_mutate_byte(uint8_t* frame, size_t frame_length, uint32_t seed);

#endif /* LEAP_TEST_FRAME_H */
