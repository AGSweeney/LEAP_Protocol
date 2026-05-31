/*
 * test_controller_session_hub.c
 *
 * Unit tests for per-peer concurrent session hub.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_controller_session_hub.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define HUB_TEST_FRAME_BUF 512u

static const uint8_t k_peer_a[6] = { 0x02, 0x50, 0x00, 0x00, 0x00, 0x01 };
static const uint8_t k_peer_b[6] = { 0x02, 0x50, 0x00, 0x00, 0x00, 0x02 };

typedef struct HubMockStackIo
{
    uint8_t  recv_frames[16][HUB_TEST_FRAME_BUF];
    size_t   recv_lengths[16];
    unsigned recv_count;
    unsigned recv_index;
    unsigned send_count;
} HubMockStackIo;

static int hub_mock_send(
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
    HubMockStackIo* mock = (HubMockStackIo*)user_ctx;

    (void)dst_mac;
    (void)flags;
    (void)service_id;
    (void)message_type;
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
    return 0;
}

static int hub_mock_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    HubMockStackIo* mock = (HubMockStackIo*)user_ctx;
    size_t          copy_len;

    (void)parsed;
    (void)timeout_ms;

    if (mock == NULL || payload_length == NULL)
    {
        return -1;
    }

    if (mock->recv_index >= mock->recv_count)
    {
        return -1;
    }

    copy_len = mock->recv_lengths[mock->recv_index];
    if (copy_len > payload_capacity)
    {
        return -1;
    }

    memcpy(payload_buf, mock->recv_frames[mock->recv_index], copy_len);
    memcpy(src_mac, k_peer_a, 6);
    *payload_length = copy_len;
    mock->recv_index++;
    return 0;
}

static void hub_mock_add_reply(
    HubMockStackIo* mock,
    uint16_t        service_id,
    uint16_t        message_type,
    uint32_t        session_id,
    const uint8_t*  payload,
    size_t          payload_length)
{
    unsigned idx;

    if (mock == NULL || mock->recv_count >= 16u)
    {
        return;
    }

    idx = mock->recv_count;
    ASSERT_TRUE(
        leap_test_frame_build(
            mock->recv_frames[idx],
            HUB_TEST_FRAME_BUF,
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

static void hub_mock_add_bootstrap_replies(HubMockStackIo* mock, uint32_t session_id)
{
    LeapProfileReply     profile;
    LeapOpenSessionReply open_reply;
    LeapStateReply       state_reply;

    memset(&profile, 0, sizeof(profile));
    profile.active_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    profile.endpoint_count    = 2u;
    hub_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_PROFILE_REPLY,
        0u,
        (const uint8_t*)&profile,
        sizeof(profile));

    memset(&open_reply, 0, sizeof(open_reply));
    open_reply.assigned_session_id      = session_id;
    open_reply.granted_lease_time_us    = 1000000u;
    open_reply.granted_watchdog_time_us = 200000u;
    open_reply.current_state            = (uint16_t)LEAP_STATE_SAFE;
    hub_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_OPEN_SESSION_REPLY,
        0u,
        (const uint8_t*)&open_reply,
        sizeof(open_reply));

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.accepted_state = (uint16_t)LEAP_STATE_OP;
    state_reply.current_state  = (uint16_t)LEAP_STATE_OP;
    hub_mock_add_reply(
        mock,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_STATE_REPLY,
        session_id,
        (const uint8_t*)&state_reply,
        sizeof(state_reply));
}

static void hub_mock_reset_recv(HubMockStackIo* mock)
{
    if (mock == NULL)
    {
        return;
    }

    mock->recv_index = 0u;
    mock->recv_count = 0u;
}

TEST(test_session_hub_two_independent_sessions)
{
    LeapControllerSessionHub       hub;
    LeapControllerSessionHubConfig config;
    LeapControllerStackIo          io;
    HubMockStackIo                 mock;
    int                            slot_a;
    int                            slot_b;
    LeapControllerStack*           stack_a;
    LeapControllerStack*           stack_b;

    memset(&config, 0, sizeof(config));
    leap_controller_session_hub_init(&hub, &config);

    memset(&mock, 0, sizeof(mock));
    hub_mock_add_bootstrap_replies(&mock, 101u);

    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = hub_mock_send;
    io.recv_frame = hub_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_a, NULL, &slot_a),
        LEAP_CTRL_STACK_OK);
    ASSERT_TRUE(leap_controller_session_hub_is_op(&hub, slot_a));

    hub_mock_reset_recv(&mock);
    hub_mock_add_bootstrap_replies(&mock, 102u);

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_b, NULL, &slot_b),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT((int)leap_controller_session_hub_active_count(&hub), 2);
    ASSERT_TRUE(slot_a != slot_b);

    stack_a = leap_controller_session_hub_stack(&hub, slot_a);
    stack_b = leap_controller_session_hub_stack(&hub, slot_b);
    ASSERT_TRUE(stack_a != NULL && stack_b != NULL);
    ASSERT_TRUE(
        leap_mgmt_controller_session_id(&stack_a->mgmt) !=
        leap_mgmt_controller_session_id(&stack_b->mgmt));
}

TEST(test_session_hub_on_frame_routes_by_mac)
{
    LeapControllerSessionHub      hub;
    LeapControllerStackIo         io;
    HubMockStackIo                mock;
    LeapControllerStackEvent      event;
    LeapFrameView                 view;
    uint8_t                       frame[HUB_TEST_FRAME_BUF];
    size_t                        frame_length;
    LeapStateReply                state_reply;
    int                           slot_out;

    memset(&mock, 0, sizeof(mock));
    hub_mock_add_bootstrap_replies(&mock, 101u);

    leap_controller_session_hub_init(&hub, NULL);
    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = hub_mock_send;
    io.recv_frame = hub_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(&hub, &io, k_peer_a, NULL, NULL),
        LEAP_CTRL_STACK_OK);

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
            leap_mgmt_controller_session_id(
                &leap_controller_session_hub_stack(&hub, 0)->mgmt),
            3u,
            (const uint8_t*)&state_reply,
            sizeof(state_reply)) == 0);
    ASSERT_EQ_INT(leap_frame_parse(frame, frame_length, &view), LEAP_FRAME_OK);

    ASSERT_EQ_INT(
        leap_controller_session_hub_on_frame(
            &hub, k_peer_a, &view, &event, &slot_out),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT(slot_out, 0);

    ASSERT_EQ_INT(
        leap_controller_session_hub_on_frame(
            &hub, k_peer_b, &view, &event, &slot_out),
        LEAP_CTRL_STACK_IGNORED);
}

TEST(test_session_hub_release_one_keeps_other)
{
    LeapControllerSessionHub hub;
    LeapControllerStackIo    io;
    HubMockStackIo           mock;
    int                      slot_a;
    int                      slot_b;

    leap_controller_session_hub_init(&hub, NULL);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = hub_mock_send;
    io.recv_frame = hub_mock_recv;

    hub_mock_add_bootstrap_replies(&mock, 101u);
    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_a, NULL, &slot_a),
        LEAP_CTRL_STACK_OK);

    hub_mock_reset_recv(&mock);
    hub_mock_add_bootstrap_replies(&mock, 102u);
    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_b, NULL, &slot_b),
        LEAP_CTRL_STACK_OK);

    ASSERT_EQ_INT(
        leap_controller_session_hub_release(&hub, slot_a, &io),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT((int)leap_controller_session_hub_active_count(&hub), 1);
    ASSERT_TRUE(leap_controller_session_hub_is_op(&hub, slot_b));
    ASSERT_EQ_INT(leap_controller_session_hub_find(&hub, k_peer_a), -1);
    ASSERT_TRUE(leap_controller_session_hub_find(&hub, k_peer_b) >= 0);
}

void leap_run_controller_session_hub_tests(void)
{
    printf("controller session hub\n");
    RUN_TEST(test_session_hub_two_independent_sessions);
    RUN_TEST(test_session_hub_on_frame_routes_by_mac);
    RUN_TEST(test_session_hub_release_one_keeps_other);
}
