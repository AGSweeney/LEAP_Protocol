/*
 * test_disc_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_disc_controller.h"
#include "leap/leap_protocol.h"

#include <string.h>

TEST(test_disc_controller_build_hello)
{
    uint8_t payload[32];
    size_t  length;

    length = leap_disc_controller_build_hello(payload, sizeof(payload));
    ASSERT_TRUE(length == sizeof(LeapHelloRequest));
}

TEST(test_disc_controller_build_identify_and_locate)
{
    uint8_t payload[32];
    size_t  length;
    uint8_t peer_mac[6] = { 0x94, 0x51, 0xdc, 0x21, 0xf0, 0x2f };

    length = leap_disc_controller_build_identify(
        peer_mac, 0u, payload, sizeof(payload));
    ASSERT_TRUE(length == sizeof(LeapIdentifyRequest));
    ASSERT_TRUE(memcmp(payload, peer_mac, 6) == 0);

    length = leap_disc_controller_build_locate_device(
        3000u,
        LEAP_LOCATE_PATTERN_SLOW_BLINK,
        LEAP_LOCATE_FLAG_LED,
        payload,
        sizeof(payload));
    ASSERT_TRUE(length == sizeof(LeapLocateDeviceRequest));
}

TEST(test_disc_controller_parse_hello_reply)
{
    LeapHelloReply reply;
    uint8_t        raw[sizeof(LeapHelloReply)];

    memset(&reply, 0, sizeof(reply));
    reply.current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
    reply.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    reply.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    memcpy(raw, &reply, sizeof(reply));

    memset(&reply, 0, sizeof(reply));
    ASSERT_EQ_INT(
        leap_disc_controller_on_hello_reply(raw, sizeof(raw), &reply),
        LEAP_DISC_CTRL_OK);
    ASSERT_EQ_U32(reply.active_profile_id, LEAP_PROFILE_DIGITAL_IO_16X16);
    ASSERT_EQ_INT(reply.current_state, LEAP_STATE_CONFIGURED);
}

void leap_run_disc_controller_tests(void)
{
    printf("disc controller\n");
    RUN_TEST(test_disc_controller_build_hello);
    RUN_TEST(test_disc_controller_build_identify_and_locate);
    RUN_TEST(test_disc_controller_parse_hello_reply);
}
