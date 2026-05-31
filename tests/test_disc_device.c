/*
 * test_disc_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_disc_device.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_DISC_BUF_SIZE 256u

static const uint8_t k_controller_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_device_mac[6]     = { 0x02, 0x66, 0x77, 0x88, 0x99, 0xAA };

static void disc_setup(LeapDiscDeviceContext* disc, LeapMgmtDeviceContext* mgmt)
{
    LeapDiscDeviceConfig config;

    memset(&config, 0, sizeof(config));
    memcpy(config.identity.primary_mac, k_device_mac, 6);
    config.identity.vendor_id         = 0x1234u;
    config.identity.product_code      = 0x01020304u;
    config.identity.firmware_revision = 1u;
    config.default_profile_id         = 0x00010002u;
    config.active_profile_id          = 0x00010002u;

    leap_disc_device_init(disc, &config);
    leap_mgmt_device_init(mgmt, NULL);
    leap_mgmt_device_on_transport_ready(mgmt);
    leap_mgmt_device_on_profile_selected(mgmt);
}

TEST(test_disc_hello_reply_contains_identity)
{
    LeapDiscDeviceContext disc;
    LeapMgmtDeviceContext mgmt;
    LeapDiscDeviceResult  result;
    LeapHelloRequest      hello;
    uint8_t               frame[TEST_DISC_BUF_SIZE];
    size_t                frame_length = 0u;
    const LeapHelloReply* reply;

    disc_setup(&disc, &mgmt);

    memset(&hello, 0, sizeof(hello));
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DISC_BUF_SIZE,
            &frame_length,
            LEAP_FLAG_BROADCAST,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            1u,
            (const uint8_t*)&hello,
            sizeof(hello)) == 0);

    ASSERT_EQ_INT(
        leap_disc_device_process_frame(
            &disc, &mgmt, k_controller_mac, frame, frame_length, &result),
        LEAP_DISC_DEVICE_OK);
    ASSERT_EQ_U16(result.message_type, LEAP_DISC_HELLO_REPLY);
    ASSERT_TRUE(result.payload_length >= sizeof(LeapHelloReply));

    reply = (const LeapHelloReply*)result.payload;
    ASSERT_EQ_U16(reply->current_state, LEAP_STATE_CONFIGURED);
    ASSERT_EQ_U32(reply->default_profile_id, 0x00010002u);
    ASSERT_TRUE(memcmp(reply->identity.primary_mac, k_device_mac, 6) == 0);
}

TEST(test_disc_does_not_change_mgmt_state)
{
    LeapDiscDeviceContext disc;
    LeapMgmtDeviceContext mgmt;
    LeapDiscDeviceResult  result;
    LeapHelloRequest      hello;
    uint8_t               frame[TEST_DISC_BUF_SIZE];
    size_t                frame_length = 0u;
    LeapState_u16         before;
    uint8_t               owner_before;

    disc_setup(&disc, &mgmt);
    before       = leap_mgmt_device_get_state(&mgmt);
    owner_before = mgmt.owner_active;

    memset(&hello, 0, sizeof(hello));
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DISC_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            2u,
            (const uint8_t*)&hello,
            sizeof(hello)) == 0);

    ASSERT_EQ_INT(
        leap_disc_device_process_frame(
            &disc, &mgmt, k_controller_mac, frame, frame_length, &result),
        LEAP_DISC_DEVICE_OK);

    ASSERT_EQ_INT(leap_mgmt_device_get_state(&mgmt), before);
    ASSERT_EQ_INT((int)mgmt.owner_active, (int)owner_before);
}

TEST(test_disc_stack_hello_via_device_stack)
{
    LeapDeviceStack        stack;
    LeapDeviceStackConfig  config;
    LeapHelloRequest       hello;
    LeapDeviceStackResult  result;
    uint8_t                frame[TEST_DISC_BUF_SIZE];
    size_t                 frame_length = 0u;

    memset(&config, 0, sizeof(config));
    memcpy(config.disc.identity.primary_mac, k_device_mac, 6);
    config.disc.default_profile_id = 0x00010002u;

    leap_device_stack_init_full(&stack, &config);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);

    memset(&hello, 0, sizeof(hello));
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DISC_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            1u,
            (const uint8_t*)&hello,
            sizeof(hello)) == 0);

    ASSERT_EQ_INT(
        leap_device_stack_process_frame(
            &stack, k_controller_mac, 0u, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK);
    ASSERT_TRUE((result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u);
    ASSERT_EQ_U16(result.disc_message_type, LEAP_DISC_HELLO_REPLY);
}

void leap_run_disc_device_tests(void)
{
    printf("disc device\n");
    RUN_TEST(test_disc_hello_reply_contains_identity);
    RUN_TEST(test_disc_does_not_change_mgmt_state);
    RUN_TEST(test_disc_stack_hello_via_device_stack);
}
