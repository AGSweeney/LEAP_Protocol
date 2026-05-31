/*
 * test_mgmt_process.c
 *
 * Integration tests for leap_mgmt_process_frame().
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "test_util.h"

#include "leap/leap_crc.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_mgmt_process.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_MGMT_FRAME_BUF_SIZE 256u

static const uint8_t k_mac_a[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_mac_b[6] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

static void mgmt_process_setup_ready(LeapMgmtDeviceContext* ctx)
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

static void mgmt_process_finalize_header_crc(uint8_t* frame)
{
    LeapHeader* header = (LeapHeader*)frame;
    uint8_t     temp[LEAP_HEADER_LENGTH_V1];

    memcpy(temp, frame, LEAP_HEADER_LENGTH_V1);
    temp[26] = 0u;
    temp[27] = 0u;
    header->header_crc16 = leap_crc16_xmodem(temp, LEAP_HEADER_LENGTH_V1);
}

static int mgmt_process_build_frame(
    uint8_t*       out,
    size_t         out_capacity,
    size_t*        out_length,
    uint16_t       message_type,
    uint32_t       session_id,
    uint32_t       sequence,
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
    header->flags          = LEAP_FLAG_ACK_REQUESTED;
    header->service_id     = (uint16_t)LEAP_SERVICE_MGMT;
    header->message_type   = message_type;
    header->session_id     = session_id;
    header->sequence       = sequence;
    header->payload_length = (uint16_t)payload_length;

    if (payload != NULL && payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        header->payload_crc32c = leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length);
    }

    mgmt_process_finalize_header_crc(out);
    *out_length = total_length;
    return 0;
}

static void mgmt_process_patch_session_id(uint8_t* frame, uint32_t session_id)
{
    LeapHeader* header = (LeapHeader*)frame;

    header->session_id = session_id;
    mgmt_process_finalize_header_crc(frame);
}

TEST(test_mgmt_process_rejects_bad_header_crc)
{
    static const char* const k_header_hex =
        "4c45415001002001100001001d2c3b4aed030000d6030000280063146b0623c7";
    static const char* const k_payload_hex =
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100";
    uint8_t              frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t               header_length  = 0u;
    size_t               payload_length = 0u;
    LeapMgmtDeviceContext ctx;
    LeapMgmtProcessResult result;

    mgmt_process_setup_ready(&ctx);

    ASSERT_TRUE(leap_test_hex_decode(k_header_hex, frame, TEST_MGMT_FRAME_BUF_SIZE, &header_length) == 0);
    ASSERT_TRUE(
        leap_test_hex_decode(
            k_payload_hex,
            frame + header_length,
            TEST_MGMT_FRAME_BUF_SIZE - header_length,
            &payload_length) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(
            &ctx,
            k_mac_a,
            0u,
            frame,
            header_length + payload_length,
            &result),
        LEAP_MGMT_PROCESS_FRAME_ERROR);
    ASSERT_EQ_INT(result.frame_error, LEAP_FRAME_ERR_BAD_HEADER_CRC);
}

TEST(test_mgmt_process_rejects_non_mgmt_service)
{
    static const char* const k_hex =
        "4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38"
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100";
    uint8_t              frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t               frame_length = 0u;
    LeapMgmtDeviceContext ctx;
    LeapMgmtProcessResult result;

    mgmt_process_setup_ready(&ctx);
    ASSERT_TRUE(leap_test_hex_decode(k_hex, frame, TEST_MGMT_FRAME_BUF_SIZE, &frame_length) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_MGMT_PROCESS_NOT_MGMT);
    ASSERT_EQ_U16(result.frame.header.service_id, LEAP_SERVICE_PD);
}

TEST(test_mgmt_process_ignores_mgmt_response_frame)
{
    static const char* const k_hex =
        "4c45415001002003010002001d2c3b4a6e0000000000000018005d25da2f75ba"
        "1d2c3b4a20a10700a08601000500030066778899aabb0000";
    uint8_t              frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t               frame_length = 0u;
    LeapMgmtDeviceContext ctx;
    LeapMgmtProcessResult result;

    mgmt_process_setup_ready(&ctx);
    ASSERT_TRUE(leap_test_hex_decode(k_hex, frame, TEST_MGMT_FRAME_BUF_SIZE, &frame_length) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_MGMT_PROCESS_IGNORED_RESPONSE);
}

TEST(test_mgmt_process_open_session_sets_side_effect_flags)
{
    LeapOpenSessionRequest open_req;
    LeapMgmtDeviceContext  ctx;
    LeapMgmtProcessResult  result;
    uint8_t                frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t                 frame_length = 0u;

    mgmt_process_setup_ready(&ctx);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags              = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us   = 1000000u;
    open_req.requested_watchdog_time_us = 200000u;

    ASSERT_TRUE(
        mgmt_process_build_frame(
            frame,
            TEST_MGMT_FRAME_BUF_SIZE,
            &frame_length,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            1u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_MGMT_PROCESS_OK);
    ASSERT_TRUE((result.flags & LEAP_MGMT_PROCESS_FLAG_PROCESSED) != 0u);
    ASSERT_TRUE((result.flags & LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED) != 0u);
    ASSERT_TRUE((result.flags & LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED) != 0u);
    ASSERT_TRUE((result.flags & LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED) != 0u);
    ASSERT_TRUE((result.flags & LEAP_MGMT_PROCESS_FLAG_HAS_REPLY) != 0u);
    ASSERT_EQ_U16(result.reply.message_type, LEAP_MGMT_OPEN_SESSION_REPLY);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
}

TEST(test_mgmt_process_vector6_set_state_to_op)
{
    static const char* const k_hex =
        "4c45415001002001010005001d2c3b4a140500000000000004003188347a4533"
        "040000000000000000000000000000000000";
    LeapOpenSessionRequest open_req;
    LeapMgmtDeviceContext  ctx;
    LeapMgmtProcessResult  open_result;
    LeapMgmtProcessResult  set_result;
    uint8_t                open_frame[TEST_MGMT_FRAME_BUF_SIZE];
    uint8_t                set_frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t                 open_frame_length = 0u;
    size_t                 set_frame_length  = 0u;
    uint32_t               session_id;

    mgmt_process_setup_ready(&ctx);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags              = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us = 1000000u;

    ASSERT_TRUE(
        mgmt_process_build_frame(
            open_frame,
            TEST_MGMT_FRAME_BUF_SIZE,
            &open_frame_length,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            1u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, open_frame, open_frame_length, &open_result),
        LEAP_MGMT_PROCESS_OK);

    session_id = ((const LeapOpenSessionReply*)open_result.reply.payload)->assigned_session_id;
    ASSERT_TRUE(session_id != 0u);

    ASSERT_TRUE(leap_test_hex_decode(k_hex, set_frame, TEST_MGMT_FRAME_BUF_SIZE, &set_frame_length) == 0);
    mgmt_process_patch_session_id(set_frame, session_id);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, set_frame, set_frame_length, &set_result),
        LEAP_MGMT_PROCESS_OK);
    ASSERT_TRUE((set_result.flags & LEAP_MGMT_PROCESS_FLAG_PROCESSED) != 0u);
    ASSERT_TRUE((set_result.flags & LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED) != 0u);
    ASSERT_TRUE((set_result.flags & LEAP_MGMT_PROCESS_FLAG_HAS_REPLY) != 0u);
    ASSERT_EQ_U16(set_result.reply.message_type, LEAP_MGMT_STATE_REPLY);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_OP);
}

TEST(test_mgmt_process_tick_reports_lease_expiry_flags)
{
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    LeapMgmtDeviceContext  ctx;
    LeapMgmtProcessResult  open_result;
    LeapMgmtProcessResult  set_result;
    uint8_t                open_frame[TEST_MGMT_FRAME_BUF_SIZE];
    uint8_t                set_frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t                 open_frame_length = 0u;
    size_t                 set_frame_length  = 0u;
    uint32_t               session_id;
    uint32_t               tick_flags        = 0u;

    mgmt_process_setup_ready(&ctx);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags            = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us = 100000u;

    ASSERT_TRUE(
        mgmt_process_build_frame(
            open_frame,
            TEST_MGMT_FRAME_BUF_SIZE,
            &open_frame_length,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            1u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, open_frame, open_frame_length, &open_result),
        LEAP_MGMT_PROCESS_OK);

    session_id = ((const LeapOpenSessionReply*)open_result.reply.payload)->assigned_session_id;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;

    ASSERT_TRUE(
        mgmt_process_build_frame(
            set_frame,
            TEST_MGMT_FRAME_BUF_SIZE,
            &set_frame_length,
            LEAP_MGMT_SET_STATE,
            session_id,
            2u,
            (const uint8_t*)&set_req,
            sizeof(set_req)) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, set_frame, set_frame_length, &set_result),
        LEAP_MGMT_PROCESS_OK);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_OP);

    ASSERT_EQ_INT(
        leap_mgmt_process_tick(&ctx, 150000u, &tick_flags),
        LEAP_MGMT_PROCESS_OK);
    ASSERT_TRUE((tick_flags & LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED) != 0u);
    ASSERT_TRUE((tick_flags & LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED) != 0u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
}

TEST(test_mgmt_process_spoof_during_op_forces_safe)
{
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    LeapMgmtDeviceContext  ctx;
    LeapMgmtProcessResult  open_result;
    LeapMgmtProcessResult  spoof_result;
    uint8_t                open_frame[TEST_MGMT_FRAME_BUF_SIZE];
    uint8_t                spoof_frame[TEST_MGMT_FRAME_BUF_SIZE];
    size_t                 open_frame_length  = 0u;
    size_t                 spoof_frame_length = 0u;
    uint32_t               session_id;

    mgmt_process_setup_ready(&ctx);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags              = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us = 1000000u;

    ASSERT_TRUE(
        mgmt_process_build_frame(
            open_frame,
            TEST_MGMT_FRAME_BUF_SIZE,
            &open_frame_length,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            1u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) == 0);
    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, open_frame, open_frame_length, &open_result),
        LEAP_MGMT_PROCESS_OK);

    session_id = ((const LeapOpenSessionReply*)open_result.reply.payload)->assigned_session_id;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;
    ASSERT_TRUE(
        mgmt_process_build_frame(
            spoof_frame,
            TEST_MGMT_FRAME_BUF_SIZE,
            &spoof_frame_length,
            LEAP_MGMT_SET_STATE,
            session_id,
            2u,
            (const uint8_t*)&set_req,
            sizeof(set_req)) == 0);
    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac_a, 0u, spoof_frame, spoof_frame_length, &open_result),
        LEAP_MGMT_PROCESS_OK);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_OP);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(
            &ctx,
            k_mac_b,
            0u,
            spoof_frame,
            spoof_frame_length,
            &spoof_result),
        LEAP_MGMT_PROCESS_HANDLER_ERROR);
    ASSERT_EQ_U16(spoof_result.error_code, LEAP_STATUS_NOT_OWNER);
    ASSERT_TRUE((spoof_result.flags & LEAP_MGMT_PROCESS_FLAG_SAFE_STATE_ENTERED) != 0u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&ctx), LEAP_STATE_SAFE);
}

void leap_run_mgmt_process_tests(void)
{
    printf("mgmt process\n");
    RUN_TEST(test_mgmt_process_rejects_bad_header_crc);
    RUN_TEST(test_mgmt_process_rejects_non_mgmt_service);
    RUN_TEST(test_mgmt_process_ignores_mgmt_response_frame);
    RUN_TEST(test_mgmt_process_open_session_sets_side_effect_flags);
    RUN_TEST(test_mgmt_process_vector6_set_state_to_op);
    RUN_TEST(test_mgmt_process_tick_reports_lease_expiry_flags);
    RUN_TEST(test_mgmt_process_spoof_during_op_forces_safe);
}
