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
static const uint8_t k_peer_c[6] = { 0x02, 0x50, 0x00, 0x00, 0x00, 0x03 };
static const uint8_t k_ctrl_mac[6] = { 0x02, 0x01, 0x02, 0x03, 0x04, 0x05 };
static const uint8_t k_foreign_owner[6] = { 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x01 };

typedef struct HubMockStackIo
{
    uint8_t  recv_frames[32][HUB_TEST_FRAME_BUF];
    uint8_t  recv_src_mac[32][6];
    size_t   recv_lengths[32];
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
    memcpy(src_mac, mock->recv_src_mac[mock->recv_index], 6);
    *payload_length = copy_len;
    mock->recv_index++;
    return 0;
}

static void hub_mock_add_reply(
    HubMockStackIo* mock,
    const uint8_t*  src_mac,
    uint16_t        service_id,
    uint16_t        message_type,
    uint32_t        session_id,
    const uint8_t*  payload,
    size_t          payload_length)
{
    unsigned idx;

    if (mock == NULL || mock->recv_count >= 32u)
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
    if (src_mac != NULL)
    {
        memcpy(mock->recv_src_mac[idx], src_mac, 6);
    }
    else
    {
        memcpy(mock->recv_src_mac[idx], k_peer_a, 6);
    }
    mock->recv_count++;
}

static void hub_mock_add_bootstrap_replies_safe(
    HubMockStackIo* mock,
    const uint8_t*  peer_mac,
    uint32_t        session_id)
{
    LeapOpenSessionReply open_reply;
    LeapStateReply       state_reply;

    memset(&open_reply, 0, sizeof(open_reply));
    open_reply.assigned_session_id      = session_id;
    open_reply.granted_lease_time_us    = 1000000u;
    open_reply.granted_watchdog_time_us = 200000u;
    open_reply.current_state            = (uint16_t)LEAP_STATE_SAFE;
    hub_mock_add_reply(
        mock,
        peer_mac,
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
        peer_mac,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_STATE_REPLY,
        session_id,
        (const uint8_t*)&state_reply,
        sizeof(state_reply));
}

static void hub_mock_add_bootstrap_replies(
    HubMockStackIo* mock,
    const uint8_t*  peer_mac,
    uint32_t        session_id)
{
    LeapProfileReply     profile;
    LeapOpenSessionReply open_reply;
    LeapStateReply       state_reply;

    memset(&profile, 0, sizeof(profile));
    profile.active_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    profile.endpoint_count    = 2u;
    hub_mock_add_reply(
        mock,
        peer_mac,
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
        peer_mac,
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
        peer_mac,
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
    hub_mock_add_bootstrap_replies(&mock, k_peer_a, 101u);

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
    hub_mock_add_bootstrap_replies(&mock, k_peer_b, 102u);

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
    hub_mock_add_bootstrap_replies(&mock, k_peer_a, 101u);

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

    hub_mock_add_bootstrap_replies(&mock, k_peer_a, 101u);
    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_a, NULL, &slot_a),
        LEAP_CTRL_STACK_OK);

    hub_mock_reset_recv(&mock);
    hub_mock_add_bootstrap_replies(&mock, k_peer_b, 102u);
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

typedef struct HubPdMockIo
{
    volatile int* stop_flag;
    unsigned      pd_send_a;
    unsigned      pd_send_b;
    uint64_t      now_us;
} HubPdMockIo;

static int g_hub_pd_exchange_parallel_ok = 0;

static int hub_pd_exchange_mock_wait(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t           reply_capacity,
    size_t*          reply_length,
    int              timeout_ms,
    uint64_t*        reply_recv_us_out)
{
    HubPdMockIo* mock = (HubPdMockIo*)user_ctx;

    (void)peer_mac;
    (void)reply_payload;
    (void)reply_capacity;
    (void)timeout_ms;

    if (mock == NULL || reply_length == NULL)
    {
        return -1;
    }

    if (reply_recv_us_out != NULL)
    {
        *reply_recv_us_out = mock->now_us;
    }

    if (mock->pd_send_a >= 1u && mock->pd_send_b >= 1u)
    {
        g_hub_pd_exchange_parallel_ok = 1;
    }

    *reply_length = 0u;
    return -1;
}

static int hub_pd_exchange_mock_parallel_ok(void)
{
    return g_hub_pd_exchange_parallel_ok;
}

static int hub_pd_mock_send(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    HubPdMockIo* mock = (HubPdMockIo*)user_ctx;
    unsigned     total;

    (void)message_type;
    (void)payload;
    (void)payload_length;
    (void)session_id;
    (void)sequence;

    if (mock == NULL || peer_mac == NULL)
    {
        return -1;
    }

    if (memcmp(peer_mac, k_peer_a, 6) == 0)
    {
        mock->pd_send_a++;
    }
    else if (memcmp(peer_mac, k_peer_b, 6) == 0)
    {
        mock->pd_send_b++;
    }

    mock->now_us += 5000u;

    total = mock->pd_send_a + mock->pd_send_b;
    if (mock->stop_flag != NULL && total >= 4u)
    {
        *mock->stop_flag = 1;
    }

    return 0;
}

static uint64_t hub_pd_mock_monotonic(void* user_ctx)
{
    HubPdMockIo* mock = (HubPdMockIo*)user_ctx;

    if (mock == NULL)
    {
        return 0u;
    }

    return mock->now_us;
}

static void hub_bootstrap_two_peers(
    LeapControllerSessionHub* hub,
    LeapControllerStackIo*    io,
    HubMockStackIo*           mock,
    int*                      slot_a,
    int*                      slot_b)
{
    hub_mock_reset_recv(mock);
    hub_mock_add_bootstrap_replies(mock, k_peer_a, 101u);

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            hub, io, k_peer_a, NULL, slot_a),
        LEAP_CTRL_STACK_OK);

    hub_mock_reset_recv(mock);
    hub_mock_add_bootstrap_replies(mock, k_peer_b, 102u);

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            hub, io, k_peer_b, NULL, slot_b),
        LEAP_CTRL_STACK_OK);
}

TEST(test_session_hub_op_peer_index)
{
    LeapControllerSessionHub hub;
    LeapControllerStackIo    stack_io;
    HubMockStackIo           stack_mock;
    int                      slot_a = -1;
    int                      slot_b = -1;

    leap_controller_session_hub_init(&hub, NULL);

    memset(&stack_mock, 0, sizeof(stack_mock));
    memset(&stack_io, 0, sizeof(stack_io));
    stack_io.user_ctx   = &stack_mock;
    stack_io.send_frame = hub_mock_send;
    stack_io.recv_frame = hub_mock_recv;

    hub_bootstrap_two_peers(&hub, &stack_io, &stack_mock, &slot_a, &slot_b);

    ASSERT_EQ_U32(leap_controller_session_hub_count_op_peers(&hub), 2u);
    ASSERT_EQ_INT(leap_controller_session_hub_op_peer_at_index(&hub, 0u), slot_a);
    ASSERT_EQ_INT(leap_controller_session_hub_op_peer_at_index(&hub, 1u), slot_b);
    ASSERT_EQ_INT(leap_controller_session_hub_op_peer_at_index(&hub, 2u), -1);
}

TEST(test_session_hub_bootstrap_table_skips_foreign_owner)
{
    LeapControllerSessionHub       hub;
    LeapControllerSessionHubConfig config;
    LeapControllerStackIo          io;
    HubMockStackIo                 mock;
    LeapControllerPeerTable        table;
    unsigned                       boot_count = 0u;

    memset(&config, 0, sizeof(config));
    memcpy(config.default_peer.mgmt.controller_mac, k_ctrl_mac, 6);
    config.skip_foreign_owned_peers = 1;
    leap_controller_session_hub_init(&hub, &config);

    leap_controller_peer_table_init(&table);
    memcpy(table.peers[0].mac, k_peer_a, 6);
    table.peers[0].reachable          = 1;
    table.peers[0].device_state       = (uint16_t)LEAP_STATE_CONFIGURED;
    table.peers[0].active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.peers[0].default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;

    memcpy(table.peers[1].mac, k_peer_b, 6);
    table.peers[1].reachable          = 1;
    table.peers[1].device_state       = (uint16_t)LEAP_STATE_CONFIGURED;
    table.peers[1].active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.peers[1].default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    memcpy(table.peers[1].active_owner_mac, k_foreign_owner, 6);

    memcpy(table.peers[2].mac, k_peer_c, 6);
    table.peers[2].reachable          = 1;
    table.peers[2].device_state       = (uint16_t)LEAP_STATE_CONFIGURED;
    table.peers[2].active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.peers[2].default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.count = 3u;

    memset(&mock, 0, sizeof(mock));
    hub_mock_add_bootstrap_replies(&mock, k_peer_a, 201u);
    hub_mock_add_bootstrap_replies(&mock, k_peer_c, 203u);

    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = hub_mock_send;
    io.recv_frame = hub_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_table(
            &hub, &io, &table, &boot_count),
        LEAP_CTRL_HUB_OK);
    ASSERT_EQ_INT((int)boot_count, 2);
    ASSERT_EQ_INT((int)leap_controller_session_hub_active_count(&hub), 2);
    ASSERT_EQ_INT(leap_controller_session_hub_find(&hub, k_peer_a), 0);
    ASSERT_EQ_INT(leap_controller_session_hub_find(&hub, k_peer_b), -1);
    ASSERT_TRUE(leap_controller_session_hub_find(&hub, k_peer_c) >= 0);
    ASSERT_TRUE(
        leap_controller_session_hub_is_op(
            &hub, leap_controller_session_hub_find(&hub, k_peer_a)));
    ASSERT_TRUE(
        leap_controller_session_hub_is_op(
            &hub, leap_controller_session_hub_find(&hub, k_peer_c)));
}

TEST(test_session_hub_run_round_robin_two_peers)
{
    LeapControllerSessionHub hub;
    LeapControllerStackIo    stack_io;
    HubMockStackIo             stack_mock;
    HubPdMockIo                pd_mock;
    LeapPdControllerIo         pd_io;
    int                        slot_a;
    int                        slot_b;
    LeapControllerStack*       stack_a;
    LeapControllerStack*       stack_b;
    volatile int               stop = 0;

    leap_controller_session_hub_init(&hub, NULL);

    memset(&stack_mock, 0, sizeof(stack_mock));
    memset(&stack_io, 0, sizeof(stack_io));
    stack_io.user_ctx   = &stack_mock;
    stack_io.send_frame = hub_mock_send;
    stack_io.recv_frame = hub_mock_recv;

    hub_bootstrap_two_peers(&hub, &stack_io, &stack_mock, &slot_a, &slot_b);

    memset(&pd_mock, 0, sizeof(pd_mock));
    pd_mock.stop_flag = &stop;
    pd_mock.now_us    = 1000u;

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.user_ctx     = &pd_mock;
    pd_io.send_pd      = hub_pd_mock_send;
    pd_io.monotonic_us = hub_pd_mock_monotonic;

    ASSERT_EQ_INT(
        leap_controller_session_hub_run_round_robin(&hub, &pd_io, &stop),
        LEAP_PD_CTRL_OK);
    ASSERT_EQ_INT((int)pd_mock.pd_send_a, 2);
    ASSERT_EQ_INT((int)pd_mock.pd_send_b, 2);

    stack_a = leap_controller_session_hub_stack(&hub, slot_a);
    stack_b = leap_controller_session_hub_stack(&hub, slot_b);
    ASSERT_TRUE(stack_a != NULL && stack_b != NULL);
    ASSERT_TRUE(stack_a->pd.stats.cycles_completed == 2u);
    ASSERT_TRUE(stack_b->pd.stats.cycles_completed == 2u);
}

TEST(test_session_hub_run_parallel_sends_before_recv)
{
    LeapControllerSessionHub       hub;
    LeapControllerSessionHubConfig config;
    LeapControllerStackIo          stack_io;
    HubMockStackIo                 stack_mock;
    HubPdMockIo                    pd_mock;
    LeapPdControllerIo             pd_io;
    int                            slot_a;
    int                            slot_b;
    volatile int                   stop = 0;
    int                            parallel_batch_ok = 0;

    memset(&config, 0, sizeof(config));
    config.default_peer.pd.use_exchange = 1;
    leap_controller_session_hub_init(&hub, &config);

    memset(&stack_mock, 0, sizeof(stack_mock));
    memset(&stack_io, 0, sizeof(stack_io));
    stack_io.user_ctx   = &stack_mock;
    stack_io.send_frame = hub_mock_send;
    stack_io.recv_frame = hub_mock_recv;

    hub_bootstrap_two_peers(&hub, &stack_io, &stack_mock, &slot_a, &slot_b);

    g_hub_pd_exchange_parallel_ok = 0;
    memset(&pd_mock, 0, sizeof(pd_mock));
    pd_mock.stop_flag = &stop;
    pd_mock.now_us    = 1000u;

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.user_ctx     = &pd_mock;
    pd_io.send_pd      = hub_pd_mock_send;
    pd_io.monotonic_us = hub_pd_mock_monotonic;
    pd_io.wait_exchange_reply = hub_pd_exchange_mock_wait;

    ASSERT_EQ_INT(
        leap_controller_session_hub_run_parallel(&hub, &pd_io, &stop, 0),
        LEAP_PD_CTRL_OK);
    parallel_batch_ok = hub_pd_exchange_mock_parallel_ok();
    ASSERT_TRUE(parallel_batch_ok != 0);
    ASSERT_EQ_INT((int)pd_mock.pd_send_a, 2);
    ASSERT_EQ_INT((int)pd_mock.pd_send_b, 2);
}

TEST(test_session_hub_table_bootstrap_round_robin)
{
    LeapControllerSessionHub       hub;
    LeapControllerSessionHubConfig config;
    LeapControllerStackIo          stack_io;
    HubMockStackIo                 stack_mock;
    HubPdMockIo                    pd_mock;
    LeapPdControllerIo             pd_io;
    LeapControllerPeerTable        table;
    volatile int                   stop = 0;
    unsigned                       boot_count = 0u;

    memset(&config, 0, sizeof(config));
    memcpy(config.default_peer.mgmt.controller_mac, k_ctrl_mac, 6);
    leap_controller_session_hub_init(&hub, &config);

    leap_controller_peer_table_init(&table);
    memcpy(table.peers[0].mac, k_peer_a, 6);
    table.peers[0].reachable          = 1;
    table.peers[0].device_state       = (uint16_t)LEAP_STATE_CONFIGURED;
    table.peers[0].active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.peers[0].default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;

    memcpy(table.peers[1].mac, k_peer_b, 6);
    table.peers[1].reachable          = 1;
    table.peers[1].device_state       = (uint16_t)LEAP_STATE_CONFIGURED;
    table.peers[1].active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.peers[1].default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    table.count = 2u;

    memset(&stack_mock, 0, sizeof(stack_mock));
    hub_mock_add_bootstrap_replies(&stack_mock, k_peer_a, 301u);
    hub_mock_add_bootstrap_replies(&stack_mock, k_peer_b, 302u);

    memset(&stack_io, 0, sizeof(stack_io));
    stack_io.user_ctx   = &stack_mock;
    stack_io.send_frame = hub_mock_send;
    stack_io.recv_frame = hub_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_table(
            &hub, &stack_io, &table, &boot_count),
        LEAP_CTRL_HUB_OK);
    ASSERT_EQ_INT((int)boot_count, 2);
    ASSERT_EQ_INT((int)leap_controller_session_hub_active_count(&hub), 2);

    memset(&pd_mock, 0, sizeof(pd_mock));
    pd_mock.stop_flag = &stop;
    pd_mock.now_us    = 1000u;

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.user_ctx     = &pd_mock;
    pd_io.send_pd      = hub_pd_mock_send;
    pd_io.monotonic_us = hub_pd_mock_monotonic;

    ASSERT_EQ_INT(
        leap_controller_session_hub_run_round_robin(&hub, &pd_io, &stop),
        LEAP_PD_CTRL_OK);
    ASSERT_EQ_INT((int)pd_mock.pd_send_a, 2);
    ASSERT_EQ_INT((int)pd_mock.pd_send_b, 2);
}

TEST(test_session_hub_bootstrap_ignores_other_peer_frames)
{
    LeapControllerSessionHub hub;
    LeapControllerStackIo    io;
    HubMockStackIo           mock;
    LeapHelloReply           hello_a;
    LeapHelloReply           hello_b;
    LeapOpenSessionReply     noise_open;
    int                      slot_a;
    int                      slot_b;

    memset(&hello_a, 0, sizeof(hello_a));
    hello_a.current_state      = (uint16_t)LEAP_STATE_SAFE;
    hello_a.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    hello_a.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;

    memset(&hello_b, 0, sizeof(hello_b));
    hello_b.current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
    hello_b.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    hello_b.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;

    leap_controller_session_hub_init(&hub, NULL);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = hub_mock_send;
    io.recv_frame = hub_mock_recv;

    hub_mock_add_bootstrap_replies_safe(&mock, k_peer_a, 101u);
    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_a, &hello_a, &slot_a),
        LEAP_CTRL_STACK_OK);

    hub_mock_reset_recv(&mock);

    memset(&noise_open, 0, sizeof(noise_open));
    noise_open.assigned_session_id = 101u;
    hub_mock_add_reply(
        &mock,
        k_peer_a,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_OPEN_SESSION_REPLY,
        0u,
        (const uint8_t*)&noise_open,
        sizeof(noise_open));
    hub_mock_add_bootstrap_replies(&mock, k_peer_b, 102u);

    ASSERT_EQ_INT(
        leap_controller_session_hub_bootstrap_peer(
            &hub, &io, k_peer_b, &hello_b, &slot_b),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT((int)leap_controller_session_hub_active_count(&hub), 2);
}

void leap_run_controller_session_hub_tests(void)
{
    printf("controller session hub\n");
    RUN_TEST(test_session_hub_two_independent_sessions);
    RUN_TEST(test_session_hub_on_frame_routes_by_mac);
    RUN_TEST(test_session_hub_release_one_keeps_other);
    RUN_TEST(test_session_hub_bootstrap_table_skips_foreign_owner);
    RUN_TEST(test_session_hub_op_peer_index);
    RUN_TEST(test_session_hub_run_round_robin_two_peers);
    RUN_TEST(test_session_hub_run_parallel_sends_before_recv);
    RUN_TEST(test_session_hub_table_bootstrap_round_robin);
    RUN_TEST(test_session_hub_bootstrap_ignores_other_peer_frames);
}
