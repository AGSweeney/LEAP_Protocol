/*
 * test_pd_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "test_util.h"

#include "leap/leap_crc.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_PD_FRAME_BUF_SIZE 256u

static const uint8_t k_mac_a[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_mac_b[6] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

static void pd_setup_ready_op(LeapMgmtDeviceContext* ctx, uint32_t* session_id_out)
{
    LeapMgmtDeviceConfig   config;
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    LeapMgmtDeviceRequest  request;
    LeapMgmtDeviceReply    reply;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;
    config.max_lease_us        = 5000000u;
    config.max_watchdog_us     = 1000000u;

    leap_mgmt_device_init(ctx, &config);
    leap_mgmt_device_on_transport_ready(ctx);
    leap_mgmt_device_on_profile_selected(ctx);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags                 = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us    = 1000000u;
    open_req.requested_watchdog_time_us = 200000u;

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_mac_a;
    request.message_type   = LEAP_MGMT_OPEN_SESSION;
    request.payload        = (const uint8_t*)&open_req;
    request.payload_length = sizeof(open_req);
    ASSERT_EQ_INT(
        leap_mgmt_device_handle(ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
    *session_id_out = ((const LeapOpenSessionReply*)reply.payload)->assigned_session_id;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;
    request.session_id      = *session_id_out;
    request.message_type    = LEAP_MGMT_SET_STATE;
    request.payload         = (const uint8_t*)&set_req;
    request.payload_length  = sizeof(set_req);
    ASSERT_EQ_INT(
        leap_mgmt_device_handle(ctx, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
}

static void pd_finalize_header_crc(uint8_t* frame)
{
    LeapHeader* header = (LeapHeader*)frame;
    uint8_t     temp[LEAP_HEADER_LENGTH_V1];

    memcpy(temp, frame, LEAP_HEADER_LENGTH_V1);
    temp[26] = 0u;
    temp[27] = 0u;
    header->header_crc16 = leap_crc16_xmodem(temp, LEAP_HEADER_LENGTH_V1);
}

static int pd_build_write_frame(
    uint8_t*       out,
    size_t         out_capacity,
    size_t*        out_length,
    uint32_t       session_id,
    const uint8_t* payload,
    size_t         payload_length)
{
    LeapHeader* header;
    size_t      total_length;

    if (out == NULL || out_length == NULL ||
        out_capacity < (size_t)LEAP_HEADER_LENGTH_V1 + payload_length)
    {
        return -1;
    }

    total_length = (size_t)LEAP_HEADER_LENGTH_V1 + payload_length;
    memset(out, 0, total_length);

    header = (LeapHeader*)out;
    header->magic          = LEAP_MAGIC_U32;
    header->version_major  = LEAP_VERSION_MAJOR;
    header->version_minor  = LEAP_VERSION_MINOR;
    header->header_length  = LEAP_HEADER_LENGTH_V1;
    header->service_id     = (uint16_t)LEAP_SERVICE_PD;
    header->message_type   = LEAP_PD_WRITE_ENDPOINT;
    header->session_id     = session_id;
    header->sequence       = 100u;
    header->payload_length = (uint16_t)payload_length;

    if (payload != NULL && payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        header->payload_crc32c = leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length);
    }

    pd_finalize_header_crc(out);
    *out_length = total_length;
    return 0;
}

TEST(test_pd_rejects_without_op)
{
    LeapMgmtDeviceContext ctx;
    LeapPdDeviceResult    result;
    LeapEndpointDataHeader endpoint;
    uint8_t               frame[TEST_PD_FRAME_BUF_SIZE];
    size_t                frame_length = 0u;
    uint32_t              session_id;

    pd_setup_ready_op(&ctx, &session_id);
    ctx.device_state = LEAP_STATE_SAFE;

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.endpoint_flags = LEAP_PD_FLAG_APPLY_OUTPUTS;
    endpoint.data_length    = 0u;

    ASSERT_TRUE(
        pd_build_write_frame(
            frame,
            TEST_PD_FRAME_BUF_SIZE,
            &frame_length,
            session_id,
            (const uint8_t*)&endpoint,
            sizeof(endpoint)) == 0);

    ASSERT_EQ_INT(
        leap_pd_device_process_frame(&ctx, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_PD_DEVICE_REJECTED);
    ASSERT_EQ_U16(result.error_code, LEAP_STATUS_NOT_OWNER);
}

TEST(test_pd_accepts_owner_write_in_op)
{
    LeapMgmtDeviceContext ctx;
    LeapPdDeviceResult    result;
    LeapEndpointDataHeader endpoint;
    uint8_t               frame[TEST_PD_FRAME_BUF_SIZE];
    size_t                frame_length = 0u;
    uint32_t              session_id;

    pd_setup_ready_op(&ctx, &session_id);

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.endpoint_flags = LEAP_PD_FLAG_APPLY_OUTPUTS;
    endpoint.data_length    = 0u;

    ASSERT_TRUE(
        pd_build_write_frame(
            frame,
            TEST_PD_FRAME_BUF_SIZE,
            &frame_length,
            session_id,
            (const uint8_t*)&endpoint,
            sizeof(endpoint)) == 0);

    ASSERT_EQ_INT(
        leap_pd_device_process_frame(&ctx, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_PD_DEVICE_OK);
    ASSERT_TRUE((result.flags & LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED) != 0u);
    ASSERT_TRUE((result.flags & LEAP_PD_DEVICE_FLAG_LEASE_REFRESHED) != 0u);
}

TEST(test_pd_spoof_forces_safe)
{
    LeapMgmtDeviceContext ctx;
    LeapPdDeviceResult    result;
    LeapEndpointDataHeader endpoint;
    uint8_t               frame[TEST_PD_FRAME_BUF_SIZE];
    size_t                frame_length = 0u;
    uint32_t              session_id;

    pd_setup_ready_op(&ctx, &session_id);

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.endpoint_flags = LEAP_PD_FLAG_APPLY_OUTPUTS;

    ASSERT_TRUE(
        pd_build_write_frame(
            frame,
            TEST_PD_FRAME_BUF_SIZE,
            &frame_length,
            session_id,
            (const uint8_t*)&endpoint,
            sizeof(endpoint)) == 0);

    ASSERT_EQ_INT(
        leap_pd_device_process_frame(&ctx, k_mac_b, 0u, frame, frame_length, &result),
        LEAP_PD_DEVICE_REJECTED);
    ASSERT_EQ_U16(result.error_code, LEAP_STATUS_NOT_OWNER);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
}

TEST(test_pd_watchdog_refresh_survives_tick)
{
    LeapMgmtDeviceContext ctx;
    LeapPdDeviceResult    result;
    LeapEndpointDataHeader endpoint;
    uint8_t               frame[TEST_PD_FRAME_BUF_SIZE];
    size_t                frame_length = 0u;
    uint32_t              session_id;

    pd_setup_ready_op(&ctx, &session_id);
    ctx.granted_watchdog_us = 100000u;
    leap_mgmt_device_refresh_process_watchdog(&ctx, 0u);

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.endpoint_flags = LEAP_PD_FLAG_APPLY_OUTPUTS;

    ASSERT_TRUE(
        pd_build_write_frame(
            frame,
            TEST_PD_FRAME_BUF_SIZE,
            &frame_length,
            session_id,
            (const uint8_t*)&endpoint,
            sizeof(endpoint)) == 0);

    ASSERT_EQ_INT(
        leap_pd_device_process_frame(&ctx, k_mac_a, 80000u, frame, frame_length, &result),
        LEAP_PD_DEVICE_OK);

    leap_mgmt_device_tick(&ctx, 150000u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_OP);
}

void leap_run_pd_device_tests(void)
{
    printf("pd device\n");
    RUN_TEST(test_pd_rejects_without_op);
    RUN_TEST(test_pd_accepts_owner_write_in_op);
    RUN_TEST(test_pd_spoof_forces_safe);
    RUN_TEST(test_pd_watchdog_refresh_survives_tick);
}
