/*
 * test_mgmt_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_mgmt_controller.h"
#include "leap/leap_protocol.h"

#include <string.h>

static const uint8_t k_controller_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_device_mac[6]     = { 0x02, 0x66, 0x77, 0x88, 0x99, 0xAA };

static void mgmt_ctrl_setup(LeapMgmtControllerContext* ctx)
{
    LeapMgmtControllerConfig config;

    memset(&config, 0, sizeof(config));
    memcpy(config.controller_mac, k_controller_mac, 6);
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;

    leap_mgmt_controller_init(ctx, &config);
}

TEST(test_mgmt_controller_bootstrap_to_op)
{
    LeapMgmtControllerContext ctx;
    LeapMgmtControllerEvent   event;
    LeapHelloReply              hello;
    LeapOpenSessionReply        open_reply;
    LeapStateReply              state_reply;
    uint8_t                     payload[64];
    size_t                      payload_length;

    mgmt_ctrl_setup(&ctx);

    memset(&hello, 0, sizeof(hello));
    hello.current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
    hello.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    hello.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_hello_reply(
            &ctx, k_device_mac, (const uint8_t*)&hello, sizeof(hello), &event),
        LEAP_MGMT_CTRL_OK);
    ASSERT_TRUE((event.flags & LEAP_MGMT_CTRL_FLAG_PEER_DISCOVERED) != 0u);
    ASSERT_EQ_INT(leap_mgmt_controller_get_state(&ctx), LEAP_MGMT_CTRL_DISCOVERED);

    payload_length = leap_mgmt_controller_build_open_session(
        &ctx, payload, sizeof(payload), 1000000u, 200000u, 0u);
    ASSERT_TRUE(payload_length == sizeof(LeapOpenSessionRequest));

    memset(&open_reply, 0, sizeof(open_reply));
    open_reply.assigned_session_id    = 42u;
    open_reply.granted_lease_time_us  = 1000000u;
    open_reply.granted_watchdog_time_us = 200000u;
    open_reply.current_state          = (uint16_t)LEAP_STATE_SAFE;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_open_session_reply(
            &ctx, (const uint8_t*)&open_reply, sizeof(open_reply), &event),
        LEAP_MGMT_CTRL_OK);
    ASSERT_EQ_U32(leap_mgmt_controller_session_id(&ctx), 42u);
    ASSERT_EQ_INT(leap_mgmt_controller_get_state(&ctx), LEAP_MGMT_CTRL_SESSION_OPEN);

    payload_length = leap_mgmt_controller_build_set_state(
        &ctx, payload, sizeof(payload), LEAP_STATE_OP);
    ASSERT_TRUE(payload_length == sizeof(LeapSetStateRequest));

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.accepted_state = (uint16_t)LEAP_STATE_OP;
    state_reply.current_state  = (uint16_t)LEAP_STATE_OP;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_state_reply(
            &ctx, (const uint8_t*)&state_reply, sizeof(state_reply), &event),
        LEAP_MGMT_CTRL_OK);
    ASSERT_TRUE((event.flags & LEAP_MGMT_CTRL_FLAG_OP_ENTERED) != 0u);
    ASSERT_EQ_INT(leap_mgmt_controller_get_state(&ctx), LEAP_MGMT_CTRL_OP);
}

TEST(test_mgmt_controller_heartbeat_due_after_half_lease)
{
    LeapMgmtControllerContext ctx;
    LeapOpenSessionReply        open_reply;
    LeapMgmtControllerEvent     event;

    mgmt_ctrl_setup(&ctx);
    ctx.state             = LEAP_MGMT_CTRL_OP;
    ctx.session_id        = 1u;
    ctx.granted_lease_us  = 1000000u;
    ctx.last_lease_refresh_us = 0u;

    ASSERT_TRUE(leap_mgmt_controller_should_send_heartbeat(&ctx, 0u) != 0);
    leap_mgmt_controller_on_heartbeat_sent(&ctx, 1000u);
    ASSERT_TRUE(leap_mgmt_controller_should_send_heartbeat(&ctx, 401000u) == 0);
    ASSERT_TRUE(leap_mgmt_controller_should_send_heartbeat(&ctx, 501000u) != 0);

    (void)open_reply;
    (void)event;
}

TEST(test_mgmt_controller_pd_refreshes_lease_timer)
{
    LeapMgmtControllerContext ctx;

    mgmt_ctrl_setup(&ctx);
    ctx.state               = LEAP_MGMT_CTRL_OP;
    ctx.session_id          = 1u;
    ctx.granted_lease_us    = 1000000u;
    ctx.last_lease_refresh_us = 0u;

    leap_mgmt_controller_on_pd_sent(&ctx, 1001u, 100000u);
    ASSERT_EQ_U32(ctx.latest_process_sequence, 1001u);
    ASSERT_TRUE(leap_mgmt_controller_should_send_heartbeat(&ctx, 500000u) == 0);
    ASSERT_TRUE(leap_mgmt_controller_should_send_heartbeat(&ctx, 600000u) != 0);
}

void leap_run_mgmt_controller_tests(void)
{
    printf("mgmt controller\n");
    RUN_TEST(test_mgmt_controller_bootstrap_to_op);
    RUN_TEST(test_mgmt_controller_heartbeat_due_after_half_lease);
    RUN_TEST(test_mgmt_controller_pd_refreshes_lease_timer);
}
