/*
 * test_mgmt_boundaries.c
 *
 * Lease/watchdog boundary and tick edge cases.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

static const uint8_t k_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

static void boundary_setup(LeapMgmtDeviceContext* ctx)
{
    LeapMgmtDeviceConfig config;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;
    config.max_lease_us        = 500000u;
    config.max_watchdog_us     = 500000u;

    leap_mgmt_device_init(ctx, &config);
    leap_mgmt_device_on_transport_ready(ctx);
    leap_mgmt_device_on_profile_selected(ctx);
}

static void boundary_open(
    LeapMgmtDeviceContext* ctx,
    uint32_t               lease_us,
    uint32_t*              session_id_out)
{
    LeapOpenSessionRequest open_req;
    LeapMgmtDeviceRequest  request;
    LeapMgmtDeviceReply    reply;

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac, 6);
    open_req.open_flags                 = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us    = lease_us;
    open_req.requested_watchdog_time_us = 200000u;

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac;
    request.message_type   = LEAP_MGMT_OPEN_SESSION;
    request.payload        = (const uint8_t*)&open_req;
    request.payload_length = sizeof(open_req);

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);

    *session_id_out =
        ((const LeapOpenSessionReply*)reply.payload)->assigned_session_id;
}

static void boundary_set_op(
    LeapMgmtDeviceContext* ctx,
    uint32_t               session_id)
{
    LeapSetStateRequest   set_req;
    LeapMgmtDeviceRequest request;
    LeapMgmtDeviceReply   reply;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = LEAP_STATE_OP;

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac;
    request.session_id     = session_id;
    request.message_type   = LEAP_MGMT_SET_STATE;
    request.payload        = (const uint8_t*)&set_req;
    request.payload_length = sizeof(set_req);

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
}

TEST(test_mgmt_lease_clamped_to_max)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id;

    boundary_setup(&ctx);
    boundary_open(&ctx, 9000000u, &session_id);
    (void)session_id;

    ASSERT_EQ_U32(ctx.granted_lease_us, 500000u);
}

TEST(test_mgmt_tick_same_time_is_no_op)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id;

    boundary_setup(&ctx);
    boundary_open(&ctx, 100000u, &session_id);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);

    leap_mgmt_device_tick(&ctx, 50000u);
    leap_mgmt_device_tick(&ctx, 50000u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
    ASSERT_TRUE(ctx.owner_active != 0u);
}

TEST(test_mgmt_tick_lease_safe_owner_survives_deadline)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id;

    boundary_setup(&ctx);
    boundary_open(&ctx, 100000u, &session_id);

    leap_mgmt_device_tick(&ctx, 100000u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
    ASSERT_TRUE(ctx.owner_active != 0u);
}

TEST(test_mgmt_tick_exact_lease_deadline_expires)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id = 0u;

    boundary_setup(&ctx);
    boundary_open(&ctx, 100000u, &session_id);
    boundary_set_op(&ctx, session_id);

    leap_mgmt_device_tick(&ctx, 100000u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
    ASSERT_TRUE(ctx.owner_active == 0u);
}

TEST(test_mgmt_bad_frame_does_not_change_state)
{
    LeapMgmtDeviceContext ctx;
    LeapMgmtDeviceRequest request;
    LeapMgmtDeviceReply   reply;
    LeapSetStateRequest   set_req;
    uint32_t              session_id = 0u;
    LeapState_u16         before;

    boundary_setup(&ctx);
    boundary_open(&ctx, 1000000u, &session_id);

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = 99u;

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac;
    request.session_id     = session_id;
    request.message_type   = LEAP_MGMT_SET_STATE;
    request.payload        = (const uint8_t*)&set_req;
    request.payload_length = sizeof(set_req);

    before = leap_mgmt_device_get_state(&ctx);
    ASSERT_EQ_INT(
        leap_mgmt_device_handle(&ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_ERROR);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), before);
    ASSERT_TRUE(ctx.owner_active != 0u);
}

void leap_run_mgmt_boundary_tests(void)
{
    printf("mgmt boundaries\n");
    RUN_TEST(test_mgmt_lease_clamped_to_max);
    RUN_TEST(test_mgmt_tick_same_time_is_no_op);
    RUN_TEST(test_mgmt_tick_lease_safe_owner_survives_deadline);
    RUN_TEST(test_mgmt_tick_exact_lease_deadline_expires);
    RUN_TEST(test_mgmt_bad_frame_does_not_change_state);
}
