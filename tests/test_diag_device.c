/*
 * test_diag_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_device_stack.h"
#include "leap/leap_diag_controller.h"
#include "leap/leap_device_host_perf.h"
#include "leap/leap_diag_device.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_DIAG_BUF_SIZE 1024u

static const uint8_t k_controller_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

static void diag_setup(LeapDiagDeviceContext* diag, LeapMgmtDeviceContext* mgmt)
{
    leap_diag_device_init(diag, NULL);
    leap_mgmt_device_init(mgmt, NULL);
    leap_mgmt_device_on_transport_ready(mgmt);
    leap_diag_device_on_transport_ready(diag, 1000u);
}

static void diag_open_owner_session(LeapMgmtDeviceContext* mgmt, uint32_t* session_id)
{
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    LeapMgmtDeviceRequest  request;
    LeapMgmtDeviceReply    reply;

    leap_mgmt_device_on_profile_selected(mgmt);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_controller_mac, 6);
    open_req.open_flags                 = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us    = 1000000u;
    open_req.requested_watchdog_time_us = 200000u;

    memset(&request, 0, sizeof(request));
    request.source_mac     = k_controller_mac;
    request.message_type   = LEAP_MGMT_OPEN_SESSION;
    request.payload        = (const uint8_t*)&open_req;
    request.payload_length = sizeof(open_req);
    request.now_us         = 2000u;
    ASSERT_EQ_INT(
        leap_mgmt_device_handle(mgmt, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
    *session_id = ((const LeapOpenSessionReply*)reply.payload)->assigned_session_id;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;
    memset(&request, 0, sizeof(request));
    request.source_mac     = k_controller_mac;
    request.session_id     = *session_id;
    request.message_type   = LEAP_MGMT_SET_STATE;
    request.payload        = (const uint8_t*)&set_req;
    request.payload_length = sizeof(set_req);
    request.now_us         = 3000u;
    ASSERT_EQ_INT(
        leap_mgmt_device_handle(mgmt, &request, &reply),
        LEAP_MGMT_DEVICE_HANDLE_OK);
}

TEST(test_diag_read_counters_in_init)
{
    LeapDiagDeviceContext diag;
    LeapMgmtDeviceContext mgmt;
    LeapDiagDeviceResult  result;
    uint8_t               payload[32];
    uint8_t               frame[TEST_DIAG_BUF_SIZE];
    size_t                payload_length;
    size_t                frame_length = 0u;
    const LeapCountersReply* reply_hdr;
    const LeapCounterEntry*  entry;

    diag_setup(&diag, &mgmt);
    diag.rx_frames_accepted = 7u;

    payload_length = leap_diag_controller_build_read_counters(
        payload,
        sizeof(payload),
        (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED,
        2u,
        0u);
    ASSERT_TRUE(payload_length > 0u);

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIAG_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIAG,
            LEAP_DIAG_READ_COUNTERS,
            0u,
            1u,
            payload,
            payload_length) == 0);

    ASSERT_EQ_INT(
        leap_diag_device_process_frame(
            &diag, &mgmt, k_controller_mac, 5000u, frame, frame_length, &result),
        LEAP_DIAG_DEVICE_OK);
    ASSERT_EQ_U16(result.message_type, LEAP_DIAG_COUNTERS_REPLY);
    ASSERT_TRUE(result.payload_length >= sizeof(LeapCountersReply) + sizeof(LeapCounterEntry));

    reply_hdr = (const LeapCountersReply*)result.payload;
    ASSERT_EQ_U16(reply_hdr->counter_count, 2u);
    entry = (const LeapCounterEntry*)(result.payload + sizeof(LeapCountersReply));
    ASSERT_EQ_U16(entry[0].counter_id, (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED);
    ASSERT_TRUE(entry[0].value == 7u);
}

TEST(test_diag_read_events_after_trace_mark)
{
    LeapDiagDeviceContext diag;
    LeapMgmtDeviceContext mgmt;
    LeapDiagDeviceResult  result;
    uint8_t               payload[32];
    uint8_t               frame[TEST_DIAG_BUF_SIZE];
    size_t                payload_length;
    size_t                frame_length = 0u;
    const LeapEventsReply* reply_hdr;

    diag_setup(&diag, &mgmt);

    payload_length = leap_diag_controller_build_trace_mark(
        payload, sizeof(payload), 0x1234u, 10u, 20u);
    ASSERT_TRUE(payload_length > 0u);
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIAG_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIAG,
            LEAP_DIAG_TRACE_MARK,
            0u,
            1u,
            payload,
            payload_length) == 0);
    ASSERT_EQ_INT(
        leap_diag_device_process_frame(
            &diag, &mgmt, k_controller_mac, 6000u, frame, frame_length, &result),
        LEAP_DIAG_DEVICE_NO_REPLY);

    frame_length = 0u;
    payload_length = leap_diag_controller_build_read_events(
        payload, sizeof(payload), 0u, 8u, 0u);
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIAG_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIAG,
            LEAP_DIAG_READ_EVENTS,
            0u,
            2u,
            payload,
            payload_length) == 0);

    ASSERT_EQ_INT(
        leap_diag_device_process_frame(
            &diag, &mgmt, k_controller_mac, 7000u, frame, frame_length, &result),
        LEAP_DIAG_DEVICE_OK);
    ASSERT_EQ_U16(result.message_type, LEAP_DIAG_EVENTS_REPLY);

    reply_hdr = (const LeapEventsReply*)result.payload;
    ASSERT_TRUE(reply_hdr->event_count >= 2u);
}

TEST(test_diag_rejects_invalid_session_in_op)
{
    LeapDiagDeviceContext diag;
    LeapMgmtDeviceContext mgmt;
    LeapDiagDeviceResult  result;
    uint32_t              session_id = 0u;
    uint8_t               payload[32];
    uint8_t               frame[TEST_DIAG_BUF_SIZE];
    size_t                payload_length;
    size_t                frame_length = 0u;

    diag_setup(&diag, &mgmt);
    diag_open_owner_session(&mgmt, &session_id);

    payload_length = leap_diag_controller_build_read_timing(
        payload, sizeof(payload), 0u);
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIAG_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIAG,
            LEAP_DIAG_READ_TIMING,
            session_id + 1u,
            3u,
            payload,
            payload_length) == 0);

    ASSERT_EQ_INT(
        leap_diag_device_process_frame(
            &diag, &mgmt, k_controller_mac, 8000u, frame, frame_length, &result),
        LEAP_DIAG_DEVICE_INVALID_STATE);
    ASSERT_EQ_U16(result.error_code, LEAP_STATUS_INVALID_STATE);
}

TEST(test_diag_tx_and_timing_counters)
{
    LeapDiagDeviceContext diag;

    uint32_t i;

    leap_diag_device_init(&diag, NULL);

    for (i = 0u; i < LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL - 1u; i++)
    {
        leap_diag_device_on_frame_transmitted(&diag, 0u);
    }
    leap_diag_device_on_frame_transmitted(&diag, 150u);
    ASSERT_TRUE(diag.tx_frames_accepted == LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL);
    ASSERT_TRUE(diag.last_reply_latency_us == 150u);
    ASSERT_TRUE(diag.max_reply_latency_us == 150u);

    leap_diag_device_on_frame_transmitted(&diag, 500u);
    ASSERT_TRUE(diag.tx_frames_accepted == LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL + 1u);
    ASSERT_TRUE(diag.max_reply_latency_us == 500u);

    diag.tx_frames_accepted = 0u;
    leap_diag_device_on_pd_cycle_time(&diag, 1200u);
    leap_diag_device_on_pd_cycle_time(&diag, 900u);
    ASSERT_TRUE(diag.min_cycle_time_us == 900u);
    ASSERT_TRUE(diag.max_cycle_time_us == 1200u);
    ASSERT_TRUE(diag.last_cycle_time_us == 900u);

    leap_diag_device_on_frame_tx_dropped(&diag);
    ASSERT_TRUE(diag.tx_frames_dropped == 1u);
}

TEST(test_device_stack_notify_tx_updates_diag)
{
    LeapDeviceStack stack;

    uint32_t i;

    leap_device_stack_init_full(&stack, NULL);
    stack.last_frame_rx_us      = 1000u;
    stack.last_frame_service_id = (uint16_t)LEAP_SERVICE_PD;

    for (i = 0u; i < LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL - 1u; i++)
    {
        leap_device_stack_notify_tx_ok(&stack, 2000u + i);
    }
    leap_device_stack_notify_tx_ok(&stack, 2500u);
    ASSERT_TRUE(stack.diag.tx_frames_accepted ==
                LEAP_DEVICE_PERF_DIAG_SAMPLE_INTERVAL);
    ASSERT_TRUE(stack.diag.last_reply_latency_us == 1500u);
    ASSERT_TRUE(stack.diag.last_cycle_time_us == 1500u);
    ASSERT_TRUE(stack.diag.min_cycle_time_us == 1500u);
    ASSERT_TRUE(stack.diag.max_cycle_time_us == 1500u);

    leap_device_stack_notify_tx_drop(&stack);
    ASSERT_TRUE(stack.diag.tx_frames_dropped == 1u);
}

void leap_run_diag_device_tests(void)
{
    printf("diag device\n");
    RUN_TEST(test_diag_read_counters_in_init);
    RUN_TEST(test_diag_read_events_after_trace_mark);
    RUN_TEST(test_diag_rejects_invalid_session_in_op);
    RUN_TEST(test_diag_tx_and_timing_counters);
    RUN_TEST(test_device_stack_notify_tx_updates_diag);
}
