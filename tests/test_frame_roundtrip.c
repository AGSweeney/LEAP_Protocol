/*
 * test_frame_roundtrip.c
 *
 * Build -> parse round-trip tests.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "test_util.h"
#include "leap_test_frame.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_RT_BUF_SIZE 256u

TEST(test_frame_roundtrip_mgmt_set_state)
{
    LeapSetStateRequest set_req;
    LeapHeader          built_header;
    uint8_t             frame[TEST_RT_BUF_SIZE];
    uint8_t             copy[TEST_RT_BUF_SIZE];
    size_t              frame_length = 0u;
    LeapFrameView       view;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_RT_BUF_SIZE,
            &frame_length,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_SET_STATE,
            0x4A3B2C1Du,
            1300u,
            (const uint8_t*)&set_req,
            sizeof(set_req)) == 0);

    memcpy(&built_header, frame, sizeof(built_header));
    memcpy(copy, frame, frame_length);

    ASSERT_EQ_INT(leap_frame_parse(copy, frame_length, &view), LEAP_FRAME_OK);
    ASSERT_EQ_INT(leap_test_frame_compare_header(&built_header, &view.header), 0);
    ASSERT_EQ_U16(view.payload_length, sizeof(set_req));
    ASSERT_EQ_U16(
        ((const LeapSetStateRequest*)view.payload)->requested_state,
        (uint16_t)LEAP_STATE_OP);
}

TEST(test_frame_roundtrip_golden_vector6)
{
    static const char* const k_hex =
        "4c45415001002001010005001d2c3b4a140500000000000004003188347a4533"
        "040000000000000000000000000000000000";
    uint8_t       frame[TEST_RT_BUF_SIZE];
    size_t        frame_length = 0u;
    LeapFrameView view;

    ASSERT_TRUE(leap_test_hex_decode(k_hex, frame, TEST_RT_BUF_SIZE, &frame_length) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);
    ASSERT_EQ_U16(view.header.service_id, LEAP_SERVICE_MGMT);
    ASSERT_EQ_U16(view.header.message_type, LEAP_MGMT_SET_STATE);
    ASSERT_EQ_U16(view.payload_length, 4u);
}

TEST(test_frame_bad_parse_leaves_no_view_payload)
{
    uint8_t       frame[TEST_RT_BUF_SIZE];
    size_t        frame_length = 0u;
    LeapFrameView view;
    int           parse_ok;

    memset(frame, 0xFF, sizeof(frame));
    frame_length = LEAP_HEADER_LENGTH_V1;

    memset(&view, 0, sizeof(view));
    view.payload = (const uint8_t*)0x1u;
    parse_ok     = (leap_frame_parse(frame, frame_length, &view) == LEAP_FRAME_OK);
    ASSERT_TRUE(!parse_ok);
}

void leap_run_frame_roundtrip_tests(void)
{
    printf("frame roundtrip\n");
    RUN_TEST(test_frame_roundtrip_mgmt_set_state);
    RUN_TEST(test_frame_roundtrip_golden_vector6);
    RUN_TEST(test_frame_bad_parse_leaves_no_view_payload);
}
