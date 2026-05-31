/*
 * test_comms_loss.c
 *
 * Automated comms-loss (owner lease expiry) through leap_device_stack_tick().
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_crc.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_COMMS_FRAME_BUF_SIZE 256u

static const uint8_t k_mac_a[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

static void comms_finalize_header_crc(uint8_t* frame)
{
    LeapHeader* header = (LeapHeader*)frame;
    uint8_t     temp[LEAP_HEADER_LENGTH_V1];

    memcpy(temp, frame, LEAP_HEADER_LENGTH_V1);
    temp[26] = 0u;
    temp[27] = 0u;
    header->header_crc16 = leap_crc16_xmodem(temp, LEAP_HEADER_LENGTH_V1);
}

static int comms_build_frame(
    uint8_t*       out,
    size_t         out_capacity,
    size_t*        out_length,
    uint16_t       service_id,
    uint16_t       message_type,
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
    header->service_id     = service_id;
    header->message_type   = message_type;
    header->session_id     = session_id;
    header->payload_length = (uint16_t)payload_length;

    if (payload != NULL && payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        header->payload_crc32c = leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length);
    }

    comms_finalize_header_crc(out);
    *out_length = total_length;
    return 0;
}

static uint32_t comms_bootstrap_op(
    LeapDeviceStack* stack,
    uint32_t         lease_us,
    uint64_t         now_us)
{
    LeapDeviceStackResult  result;
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    uint8_t                frame[TEST_COMMS_FRAME_BUF_SIZE];
    size_t                 frame_length = 0u;
    uint32_t               session_id   = 0u;

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags                 = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us    = lease_us;
    open_req.requested_watchdog_time_us = 200000u;

    ASSERT_TRUE(
        comms_build_frame(
            frame,
            TEST_COMMS_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) == 0);
    ASSERT_EQ_INT(
        leap_device_stack_process_frame(
            stack, k_mac_a, now_us, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK);
    session_id =
        ((const LeapOpenSessionReply*)result.mgmt_reply.payload)->assigned_session_id;
    ASSERT_TRUE(session_id != 0u);

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;

    ASSERT_TRUE(
        comms_build_frame(
            frame,
            TEST_COMMS_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_SET_STATE,
            session_id,
            (const uint8_t*)&set_req,
            sizeof(set_req)) == 0);
    ASSERT_EQ_INT(
        leap_device_stack_process_frame(
            stack, k_mac_a, now_us, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK);
    ASSERT_EQ_INT(result.device_state, LEAP_STATE_OP);

    return session_id;
}

TEST(test_comms_loss_lease_expiry_via_device_stack_tick)
{
    LeapDeviceStack       stack;
    LeapMgmtDeviceConfig  config;
    uint32_t              session_id;
    uint32_t              tick_flags = 0u;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;

    leap_device_stack_init(&stack, &config);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);

    session_id = comms_bootstrap_op(&stack, 100000u, 0u);
    (void)session_id;

    ASSERT_EQ_INT(
        leap_device_stack_tick(&stack, 150000u, &tick_flags),
        LEAP_DEVICE_STACK_OK);
    ASSERT_TRUE((tick_flags & LEAP_DEVICE_STACK_FLAG_OWNERSHIP_CHANGED) != 0u);
    ASSERT_TRUE((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&stack.mgmt), LEAP_STATE_SAFE);
    ASSERT_TRUE(stack.mgmt.owner_active == 0u);
}

TEST(test_comms_loss_heartbeat_prevents_lease_expiry)
{
    LeapDeviceStack        stack;
    LeapDeviceStackResult  result;
    LeapMgmtDeviceConfig   config;
    LeapHeartbeatPayload   hb;
    uint8_t                frame[TEST_COMMS_FRAME_BUF_SIZE];
    size_t                 frame_length = 0u;
    uint32_t               session_id;
    uint32_t               tick_flags   = 0u;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;

    leap_device_stack_init(&stack, &config);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);

    session_id = comms_bootstrap_op(&stack, 100000u, 0u);

    memset(&hb, 0, sizeof(hb));
    ASSERT_TRUE(
        comms_build_frame(
            frame,
            TEST_COMMS_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_HEARTBEAT,
            session_id,
            (const uint8_t*)&hb,
            sizeof(hb)) == 0);
    ASSERT_EQ_INT(
        leap_device_stack_process_frame(
            &stack, k_mac_a, 80000u, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK);

    tick_flags = 0u;
    ASSERT_EQ_INT(
        leap_device_stack_tick(&stack, 150000u, &tick_flags),
        LEAP_DEVICE_STACK_OK);
    ASSERT_TRUE((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) == 0u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&stack.mgmt), LEAP_STATE_OP);
    ASSERT_TRUE(stack.mgmt.owner_active != 0u);
}

void leap_run_comms_loss_tests(void)
{
    printf("comms loss\n");
    RUN_TEST(test_comms_loss_lease_expiry_via_device_stack_tick);
    RUN_TEST(test_comms_loss_heartbeat_prevents_lease_expiry);
}
