/*
 * test_mgmt_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

static const uint8_t k_mac_a[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_mac_b[6] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

static void mgmt_setup_ready(LeapMgmtDeviceContext* ctx)
{
    LeapMgmtDeviceConfig config;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;
    config.max_lease_us        = 5000000u;
    config.max_watchdog_us     = 1000000u;

    leap_mgmt_device_init(ctx, &config);
    leap_mgmt_device_on_transport_ready(ctx);
    leap_mgmt_device_on_profile_selected(ctx);
}

static void mgmt_open_owner_session(
    LeapMgmtDeviceContext* ctx,
    const uint8_t*         mac,
    uint64_t               now_us,
    uint32_t               lease_us,
    uint32_t*              session_id_out)
{
    LeapOpenSessionRequest open_req;
    LeapMgmtDeviceRequest  request;
    LeapMgmtDeviceReply    reply;

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, mac, 6);
    open_req.open_flags              = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us = lease_us;
    open_req.requested_watchdog_time_us = 200000u;

    memset(&request, 0, sizeof(request));
    request.source_mac     = mac;
    request.session_id     = 0u;
    request.message_type   = LEAP_MGMT_OPEN_SESSION;
    request.payload        = (const uint8_t*)&open_req;
    request.payload_length = sizeof(open_req);
    request.now_us         = now_us;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
    ASSERT_EQ_U16(reply.message_type, LEAP_MGMT_OPEN_SESSION_REPLY);

    *session_id_out = ((const LeapOpenSessionReply*)reply.payload)->assigned_session_id;
}

static void mgmt_set_state(
    LeapMgmtDeviceContext* ctx,
    const uint8_t*         mac,
    uint32_t               session_id,
    LeapState_u16          state,
    uint64_t               now_us)
{
    LeapSetStateRequest   set_req;
    LeapMgmtDeviceRequest request;
    LeapMgmtDeviceReply   reply;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)state;

    memset(&request, 0, sizeof(request));
    request.source_mac     = mac;
    request.session_id     = session_id;
    request.message_type   = LEAP_MGMT_SET_STATE;
    request.payload        = (const uint8_t*)&set_req;
    request.payload_length = sizeof(set_req);
    request.now_us         = now_us;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
}

TEST(test_mgmt_boot_to_configured)
{
    LeapMgmtDeviceContext ctx;

    leap_mgmt_device_init(&ctx, NULL);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_BOOT);

    leap_mgmt_device_on_transport_ready(&ctx);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_INIT);

    leap_mgmt_device_on_profile_selected(&ctx);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_CONFIGURED);
}

TEST(test_mgmt_open_session_grants_owner_and_enters_safe)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 500000u, &session_id);

    ASSERT_TRUE(session_id != 0u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
}

TEST(test_mgmt_owner_can_enter_op_and_apply_pd)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 1000000u, &session_id);
    mgmt_set_state(&ctx, k_mac_a, session_id, LEAP_STATE_OP, 0u);

    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_OP);
    ASSERT_TRUE(leap_mgmt_device_session_allows_owner_pd(&ctx, session_id, k_mac_a));
    ASSERT_TRUE(!leap_mgmt_device_session_allows_owner_pd(&ctx, session_id, k_mac_b));
}

TEST(test_mgmt_lease_expiry_forces_safe)
{
    LeapMgmtDeviceContext ctx;
    uint32_t              session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 100000u, &session_id);
    mgmt_set_state(&ctx, k_mac_a, session_id, LEAP_STATE_OP, 0u);

    leap_mgmt_device_tick(&ctx, 150000u);

    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
    ASSERT_TRUE(!leap_mgmt_device_session_allows_owner_pd(&ctx, session_id, k_mac_a));
}

TEST(test_mgmt_heartbeat_extends_lease)
{
    LeapMgmtDeviceContext ctx;
    LeapHeartbeatPayload  hb;
    LeapMgmtDeviceRequest request;
    LeapMgmtDeviceReply   reply;
    uint32_t              session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 100000u, &session_id);
    mgmt_set_state(&ctx, k_mac_a, session_id, LEAP_STATE_OP, 0u);

    memset(&hb, 0, sizeof(hb));
    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac_a;
    request.session_id     = session_id;
    request.message_type   = LEAP_MGMT_HEARTBEAT;
    request.payload        = (const uint8_t*)&hb;
    request.payload_length = sizeof(hb);
    request.now_us         = 80000u;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(&ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_NO_REPLY);

    leap_mgmt_device_tick(&ctx, 150000u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_OP);
}

TEST(test_mgmt_owner_release_clears_owner)
{
    LeapMgmtDeviceContext ctx;
    LeapOwnerReleaseRequest release;
    LeapMgmtDeviceRequest   request;
    LeapMgmtDeviceReply     reply;
    uint32_t                session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 1000000u, &session_id);
    mgmt_set_state(&ctx, k_mac_a, session_id, LEAP_STATE_OP, 0u);

    memset(&release, 0, sizeof(release));
    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac_a;
    request.session_id     = session_id;
    request.message_type   = LEAP_MGMT_OWNER_RELEASE;
    request.payload        = (const uint8_t*)&release;
    request.payload_length = sizeof(release);
    request.now_us         = 0u;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(&ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_NO_REPLY);

    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
    ASSERT_TRUE(!leap_mgmt_device_session_allows_owner_pd(&ctx, session_id, k_mac_a));
}

TEST(test_mgmt_rejects_second_owner_while_lease_active)
{
    LeapMgmtDeviceContext ctx;
    LeapOpenSessionRequest open_req;
    LeapMgmtDeviceRequest  request;
    LeapMgmtDeviceReply    reply;
    uint32_t               session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 1000000u, &session_id);
    (void)session_id;

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_b, 6);
    open_req.open_flags = LEAP_OPEN_FLAG_REQUEST_OWNER;

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac_b;
    request.message_type   = LEAP_MGMT_OPEN_SESSION;
    request.payload        = (const uint8_t*)&open_req;
    request.payload_length = sizeof(open_req);
    request.now_us         = 0u;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(&ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_ERROR);
    ASSERT_EQ_U16(reply.error_code, LEAP_STATUS_NOT_OWNER);
}

TEST(test_mgmt_reboot_recovery_drops_op_to_configured)
{
    LeapMgmtDeviceContext ctx;
    LeapOpenSessionRequest open_req;
    LeapMgmtDeviceRequest  request;
    LeapMgmtDeviceReply    reply;
    uint32_t               session_id;

    mgmt_setup_ready(&ctx);
    mgmt_open_owner_session(&ctx, k_mac_a, 0u, 1000000u, &session_id);
    mgmt_set_state(&ctx, k_mac_a, session_id, LEAP_STATE_OP, 0u);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags = (uint16_t)(LEAP_OPEN_FLAG_REQUEST_OWNER | LEAP_OPEN_FLAG_REBOOT_RECOVERY);

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac_a;
    request.message_type   = LEAP_MGMT_OPEN_SESSION;
    request.payload        = (const uint8_t*)&open_req;
    request.payload_length = sizeof(open_req);
    request.now_us         = 100u;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(&ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);

    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_CONFIGURED);
    ASSERT_TRUE(!leap_mgmt_device_session_allows_owner_pd(&ctx, session_id, k_mac_a));
}

TEST(test_mgmt_fault_reset_returns_to_init)
{
    LeapMgmtDeviceContext ctx;
    LeapMgmtDeviceRequest request;
    LeapMgmtDeviceReply   reply;

    mgmt_setup_ready(&ctx);
    leap_mgmt_device_enter_fault(&ctx);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_FAULT);

    memset(&request, 0, sizeof(request));
    request.message_type = LEAP_MGMT_FAULT_RESET;
    request.now_us       = 0u;

    ASSERT_EQ_INT(
        leap_mgmt_device_handle(&ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_NO_REPLY);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_INIT);
}

void leap_run_mgmt_device_tests(void)
{
    printf("mgmt device\n");
    RUN_TEST(test_mgmt_boot_to_configured);
    RUN_TEST(test_mgmt_open_session_grants_owner_and_enters_safe);
    RUN_TEST(test_mgmt_owner_can_enter_op_and_apply_pd);
    RUN_TEST(test_mgmt_lease_expiry_forces_safe);
    RUN_TEST(test_mgmt_heartbeat_extends_lease);
    RUN_TEST(test_mgmt_owner_release_clears_owner);
    RUN_TEST(test_mgmt_rejects_second_owner_while_lease_active);
    RUN_TEST(test_mgmt_reboot_recovery_drops_op_to_configured);
    RUN_TEST(test_mgmt_fault_reset_returns_to_init);
}
