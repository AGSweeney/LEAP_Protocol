/*
 * test_controller_stack.c
 *
 * Mock-I/O unit tests for leap_controller_stack bootstrap FSM.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_controller_stack.h"
#include "leap/leap_controller_sequence.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define CTRL_STACK_TEST_FRAME_BUF 512u
#define CTRL_STACK_TEST_MAX_RECV  8u

static const uint8_t k_device_mac[6] = { 0x02, 0x66, 0x77, 0x88, 0x99, 0xAA };

typedef struct CtrlStackMockIo
{
    uint8_t  recv_frames[CTRL_STACK_TEST_MAX_RECV][CTRL_STACK_TEST_FRAME_BUF];
    size_t   recv_lengths[CTRL_STACK_TEST_MAX_RECV];
    unsigned recv_count;
    unsigned recv_index;
    unsigned send_count;
    uint16_t last_service;
    uint16_t last_message;
    uint32_t last_ack_sequence;
} CtrlStackMockIo;

static int ctrl_stack_mock_send(
    void*          user_ctx,
    const uint8_t* dst_mac,
    uint8_t        flags,
    uint16_t       service_id,
    uint16_t       message_type,
    uint32_t       session_id,
    uint32_t       sequence,
    uint32_t       ack_sequence,
    const uint8_t* payload,
    size_t         payload_length)
{
    CtrlStackMockIo* mock = (CtrlStackMockIo*)user_ctx;

    (void)dst_mac;
    (void)flags;
    (void)session_id;
    (void)sequence;
    (void)ack_sequence;
    (void)payload;
    (void)payload_length;

    if (mock == NULL)
    {
        return -1;
    }

    mock->send_count++;
    mock->last_service = service_id;
    mock->last_message = message_type;
    mock->last_ack_sequence = ack_sequence;
    return 0;
}

static int ctrl_stack_mock_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    CtrlStackMockIo* mock = (CtrlStackMockIo*)user_ctx;
    size_t           copy_len;

    (void)parsed;
    (void)timeout_ms;

    if (mock == NULL || payload_length == NULL)
    {
        return -1;
    }

    if (mock->recv_count == 0u)
    {
        return -1;
    }

    if (mock->recv_index >= mock->recv_count)
    {
        mock->recv_index = 0u;
    }

    copy_len = mock->recv_lengths[mock->recv_index];
    if (copy_len > payload_capacity)
    {
        return -1;
    }

    memcpy(payload_buf, mock->recv_frames[mock->recv_index], copy_len);
    memcpy(src_mac, k_device_mac, 6);
    *payload_length = copy_len;
    mock->recv_index++;
    return 0;
}

static void ctrl_stack_mock_add_reply(
    CtrlStackMockIo* mock,
    uint16_t         service_id,
    uint16_t         message_type,
    uint32_t         session_id,
    const uint8_t*   payload,
    size_t           payload_length)
{
    unsigned idx;

    if (mock == NULL || mock->recv_count >= CTRL_STACK_TEST_MAX_RECV)
    {
        return;
    }

    idx = mock->recv_count;
    ASSERT_TRUE(
        leap_test_frame_build(
            mock->recv_frames[idx],
            CTRL_STACK_TEST_FRAME_BUF,
            &mock->recv_lengths[idx],
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            session_id,
            1u,
            payload,
            payload_length) == 0);
    mock->recv_count++;
}

static void ctrl_stack_mock_add_error_reply(
    CtrlStackMockIo* mock,
    uint16_t         service_id,
    uint16_t         message_type,
    uint32_t         session_id,
    uint32_t         sequence,
    uint16_t         status_code)
{
    LeapErrorPayload err;
    unsigned         idx;

    if (mock == NULL || mock->recv_count >= CTRL_STACK_TEST_MAX_RECV)
    {
        return;
    }

    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

    idx = mock->recv_count;
    ASSERT_TRUE(
        leap_test_frame_build(
            mock->recv_frames[idx],
            CTRL_STACK_TEST_FRAME_BUF,
            &mock->recv_lengths[idx],
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR),
            service_id,
            message_type,
            session_id,
            sequence,
            (const uint8_t*)&err,
            sizeof(err)) == 0);
    mock->recv_count++;
}

static void ctrl_stack_put_op_state(LeapControllerStack* stack)
{
    memcpy(stack->peer_mac, k_device_mac, 6);
    stack->peer_bound = 1;
    stack->phase      = LEAP_CTRL_STACK_OP;
    stack->mgmt.session_id = 42u;
    stack->mgmt.state      = LEAP_MGMT_CTRL_OP;
    leap_controller_frame_sequence_bind_session(&stack->frame_seq, 42u);
}

static void ctrl_stack_setup_replies(CtrlStackMockIo* mock)
{
    LeapHelloReply        hello;
    LeapProfileReply      profile;
    LeapOpenSessionReply  open_reply;
    LeapStateReply        state_reply;

    memset(&hello, 0, sizeof(hello));
    hello.current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
    hello.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    hello.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    ctrl_stack_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_DISC,
        LEAP_DISC_HELLO_REPLY,
        0u,
        (const uint8_t*)&hello,
        sizeof(hello));

    memset(&profile, 0, sizeof(profile));
    profile.active_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    profile.endpoint_count    = 2u;
    ctrl_stack_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_PROFILE_REPLY,
        0u,
        (const uint8_t*)&profile,
        sizeof(profile));

    memset(&open_reply, 0, sizeof(open_reply));
    open_reply.assigned_session_id      = 42u;
    open_reply.granted_lease_time_us    = 1000000u;
    open_reply.granted_watchdog_time_us = 200000u;
    open_reply.current_state            = (uint16_t)LEAP_STATE_SAFE;
    ctrl_stack_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_OPEN_SESSION_REPLY,
        0u,
        (const uint8_t*)&open_reply,
        sizeof(open_reply));

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.accepted_state = (uint16_t)LEAP_STATE_OP;
    state_reply.current_state  = (uint16_t)LEAP_STATE_OP;
    ctrl_stack_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_STATE_REPLY,
        42u,
        (const uint8_t*)&state_reply,
        sizeof(state_reply));
}

TEST(test_controller_stack_bootstrap_reaches_op)
{
    LeapControllerStack       stack;
    LeapControllerStackConfig config;
    LeapControllerStackIo     io;
    CtrlStackMockIo           mock;
    uint8_t                   peer_mac[6];

    memset(&config, 0, sizeof(config));
    leap_controller_stack_init(&stack, &config);

    memset(&mock, 0, sizeof(mock));
    ctrl_stack_setup_replies(&mock);

    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = ctrl_stack_mock_send;
    io.recv_frame = ctrl_stack_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_stack_bootstrap(&stack, &io, peer_mac),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT(leap_controller_stack_get_phase(&stack), LEAP_CTRL_STACK_OP);
    ASSERT_TRUE(mock.send_count >= 4u);
    ASSERT_EQ_INT(memcmp(peer_mac, k_device_mac, 6), 0);
    ASSERT_EQ_INT(leap_mgmt_controller_get_state(&stack.mgmt), LEAP_MGMT_CTRL_OP);
}

TEST(test_controller_stack_step_idle_sends_hello)
{
    LeapControllerStack   stack;
    LeapControllerStackIo io;
    CtrlStackMockIo       mock;
    LeapControllerStackEvent event;

    leap_controller_stack_init(&stack, NULL);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = ctrl_stack_mock_send;
    io.recv_frame = ctrl_stack_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_stack_step(&stack, &io, &event),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT(stack.phase, LEAP_CTRL_STACK_DISCOVERING);
    ASSERT_EQ_U16(mock.last_service, (uint16_t)LEAP_SERVICE_DISC);
    ASSERT_EQ_U16(mock.last_message, LEAP_DISC_HELLO);
}

TEST(test_controller_stack_on_frame_mgmt_error_faults)
{
    LeapControllerStack       stack;
    LeapControllerStackEvent  event;
    LeapFrameView             view;
    uint8_t                   frame[CTRL_STACK_TEST_FRAME_BUF];
    size_t                    frame_length;
    LeapErrorPayload          err;

    leap_controller_stack_init(&stack, NULL);
    ctrl_stack_put_op_state(&stack);

    memset(&err, 0, sizeof(err));
    err.status_code = (uint16_t)LEAP_STATUS_NOT_OWNER;
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            sizeof(frame),
            &frame_length,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR),
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            42u,
            10u,
            (const uint8_t*)&err,
            sizeof(err)) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);

    ASSERT_EQ_INT(
        leap_controller_stack_on_frame(&stack, k_device_mac, &view, &event),
        LEAP_CTRL_STACK_MGMT_ERROR);
    ASSERT_EQ_INT(leap_controller_stack_get_phase(&stack), LEAP_CTRL_STACK_FAULT);
    ASSERT_EQ_U16(event.error_code, (uint16_t)LEAP_STATUS_NOT_OWNER);
    ASSERT_TRUE((event.flags & LEAP_CTRL_STACK_FLAG_FAULT) != 0u);
}

TEST(test_controller_stack_on_frame_duplicate_ignored)
{
    LeapControllerStack      stack;
    LeapControllerStackEvent event;
    LeapFrameView            view;
    uint8_t                  frame[CTRL_STACK_TEST_FRAME_BUF];
    size_t                   frame_length;
    LeapStateReply           state_reply;

    leap_controller_stack_init(&stack, NULL);
    ctrl_stack_put_op_state(&stack);

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.current_state = (uint16_t)LEAP_STATE_OP;
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            sizeof(frame),
            &frame_length,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            42u,
            7u,
            (const uint8_t*)&state_reply,
            sizeof(state_reply)) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);

    ASSERT_EQ_INT(
        leap_controller_stack_on_frame(&stack, k_device_mac, &view, &event),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_U32(stack.frame_seq.highest_peer_sequence, 7u);

    ASSERT_EQ_INT(
        leap_controller_stack_on_frame(&stack, k_device_mac, &view, &event),
        LEAP_CTRL_STACK_IGNORED);
    ASSERT_EQ_U32(stack.frame_seq.duplicate_frames, 1u);
    ASSERT_TRUE((event.flags & LEAP_CTRL_STACK_FLAG_DUPLICATE_FRAME) != 0u);
}

TEST(test_controller_stack_rejects_replayed_old_sequence)
{
    LeapControllerStack      stack;
    LeapControllerStackEvent event;
    LeapFrameView            view;
    uint8_t                  frame[CTRL_STACK_TEST_FRAME_BUF];
    size_t                   frame_length;
    LeapStateReply           state_reply;

    leap_controller_stack_init(&stack, NULL);
    ctrl_stack_put_op_state(&stack);

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.current_state = (uint16_t)LEAP_STATE_OP;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            sizeof(frame),
            &frame_length,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            42u,
            10u,
            (const uint8_t*)&state_reply,
            sizeof(state_reply)) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);
    ASSERT_EQ_INT(
        leap_controller_stack_on_frame(&stack, k_device_mac, &view, &event),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_U32(stack.frame_seq.highest_peer_sequence, 10u);

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            sizeof(frame),
            &frame_length,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            42u,
            3u,
            (const uint8_t*)&state_reply,
            sizeof(state_reply)) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);
    ASSERT_EQ_INT(
        leap_controller_stack_on_frame(&stack, k_device_mac, &view, &event),
        LEAP_CTRL_STACK_IGNORED);
    ASSERT_EQ_U32(stack.frame_seq.duplicate_frames, 1u);
    ASSERT_TRUE((event.flags & LEAP_CTRL_STACK_FLAG_DUPLICATE_FRAME) != 0u);
}

TEST(test_controller_stack_ignores_foreign_session)
{
    LeapControllerStack      stack;
    LeapControllerStackEvent event;
    LeapFrameView            view;
    uint8_t                  frame[CTRL_STACK_TEST_FRAME_BUF];
    size_t                   frame_length;
    LeapStateReply           state_reply;

    leap_controller_stack_init(&stack, NULL);
    ctrl_stack_put_op_state(&stack);

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.current_state = (uint16_t)LEAP_STATE_OP;
    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            sizeof(frame),
            &frame_length,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            99u,
            5u,
            (const uint8_t*)&state_reply,
            sizeof(state_reply)) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);

    ASSERT_EQ_INT(
        leap_controller_stack_on_frame(&stack, k_device_mac, &view, &event),
        LEAP_CTRL_STACK_IGNORED);
    ASSERT_EQ_U32(stack.frame_seq.session_mismatches, 1u);
    ASSERT_TRUE((event.flags & LEAP_CTRL_STACK_FLAG_SESSION_MISMATCH) != 0u);
}

TEST(test_controller_stack_send_acks_peer_sequence)
{
    LeapControllerStack   stack;
    LeapControllerStackIo io;
    CtrlStackMockIo       mock;

    leap_controller_stack_init(&stack, NULL);
    ctrl_stack_put_op_state(&stack);
    stack.frame_seq.highest_peer_sequence = 12u;

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = ctrl_stack_mock_send;

    ASSERT_EQ_INT(
        leap_controller_stack_release(&stack, &io),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_U32(mock.last_ack_sequence, 12u);
    ASSERT_EQ_U16(mock.last_message, LEAP_MGMT_OWNER_RELEASE);
    ASSERT_EQ_INT(leap_controller_stack_get_phase(&stack), LEAP_CTRL_STACK_IDLE);
}

TEST(test_controller_stack_release_without_session_resets)
{
    LeapControllerStack   stack;
    LeapControllerStackIo io;
    CtrlStackMockIo       mock;

    leap_controller_stack_init(&stack, NULL);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = ctrl_stack_mock_send;

    ASSERT_EQ_INT(
        leap_controller_stack_release(&stack, &io),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT(mock.send_count, 0u);
    ASSERT_EQ_INT(leap_controller_stack_get_phase(&stack), LEAP_CTRL_STACK_IDLE);
}

TEST(test_controller_stack_read_diag_op)
{
    LeapControllerStack           stack;
    LeapControllerStackIo         io;
    CtrlStackMockIo               mock;
    LeapControllerStackDiagResult result;
    uint8_t                       counters_payload[64];
    LeapCountersReply*            counters_hdr;
    LeapCounterEntry*             counter_entries;
    LeapTimingReply               timing;
    size_t                        counters_payload_len;

    leap_controller_stack_init(&stack, NULL);
    ctrl_stack_put_op_state(&stack);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx    = &mock;
    io.send_frame  = ctrl_stack_mock_send;
    io.recv_frame  = ctrl_stack_mock_recv;

    counters_hdr = (LeapCountersReply*)counters_payload;
    counter_entries =
        (LeapCounterEntry*)(counters_payload + sizeof(LeapCountersReply));
    counters_hdr->counter_count = 2u;
    counters_hdr->reserved      = 0u;
    counter_entries[0].counter_id = (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED;
    counter_entries[0].value    = 42u;
    counter_entries[1].counter_id = (uint16_t)LEAP_COUNTER_CRC_FAILURES;
    counter_entries[1].value    = 3u;
    counters_payload_len =
        sizeof(LeapCountersReply) + (2u * sizeof(LeapCounterEntry));

    ctrl_stack_mock_add_reply(
        &mock,
        (uint16_t)LEAP_SERVICE_DIAG,
        LEAP_DIAG_COUNTERS_REPLY,
        42u,
        counters_payload,
        counters_payload_len);

    memset(&timing, 0, sizeof(timing));
    timing.last_cycle_time_us            = 1000u;
    timing.max_cycle_time_us             = 2000u;
    timing.min_cycle_time_us             = 800u;
    timing.last_reply_latency_us         = 50u;
    timing.max_reply_latency_us          = 120u;
    timing.process_watchdog_remaining_us = 50000u;
    timing.owner_lease_remaining_us      = 4000000u;

    ctrl_stack_mock_add_reply(
        &mock,
        (uint16_t)LEAP_SERVICE_DIAG,
        LEAP_DIAG_TIMING_REPLY,
        42u,
        (const uint8_t*)&timing,
        sizeof(timing));

    ASSERT_EQ_INT(
        leap_controller_stack_read_diag(&stack, &io, &result),
        LEAP_CTRL_STACK_DIAG_OK);
    ASSERT_EQ_INT(result.has_counters, 1);
    ASSERT_EQ_INT(result.has_timing, 1);
    ASSERT_EQ_U16(result.counter_count, 2u);
    ASSERT_EQ_U16(result.counters[0].counter_id, LEAP_COUNTER_RX_FRAMES_ACCEPTED);
    ASSERT_TRUE(result.counters[0].value == 42u);
    ASSERT_EQ_U32(result.timing.last_cycle_time_us, 1000u);
    ASSERT_EQ_U32(result.timing.owner_lease_remaining_us, 4000000u);
    ASSERT_EQ_INT(mock.send_count, 2u);
    ASSERT_EQ_U16(mock.last_service, (uint16_t)LEAP_SERVICE_DIAG);
    ASSERT_EQ_U16(mock.last_message, LEAP_DIAG_READ_TIMING);
}

void leap_run_controller_stack_tests(void)
{
    printf("controller stack\n");
    RUN_TEST(test_controller_stack_step_idle_sends_hello);
    RUN_TEST(test_controller_stack_bootstrap_reaches_op);
    RUN_TEST(test_controller_stack_on_frame_mgmt_error_faults);
    RUN_TEST(test_controller_stack_on_frame_duplicate_ignored);
    RUN_TEST(test_controller_stack_rejects_replayed_old_sequence);
    RUN_TEST(test_controller_stack_ignores_foreign_session);
    RUN_TEST(test_controller_stack_send_acks_peer_sequence);
    RUN_TEST(test_controller_stack_release_without_session_resets);
    RUN_TEST(test_controller_stack_read_diag_op);
}
