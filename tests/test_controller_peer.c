/*
 * test_controller_peer.c
 *
 * Unit tests for multi-device peer table and bootstrap_peer.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define PEER_TEST_FRAME_BUF 512u
#define PEER_TEST_MAX_RECV  (LEAP_CTRL_MAX_PEERS + 1u)

static const uint8_t k_peer_a[6] = { 0x02, 0x10, 0x20, 0x30, 0x40, 0x01 };
static const uint8_t k_peer_b[6] = { 0x02, 0x10, 0x20, 0x30, 0x40, 0x02 };
static const uint8_t k_ctrl_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_foreign_mac[6] = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

typedef struct CtrlStackMockIo
{
    uint8_t  recv_frames[8][512];
    size_t   recv_lengths[8];
    unsigned recv_count;
    unsigned recv_index;
    unsigned send_count;
    uint16_t last_service;
    uint16_t last_message;
    uint32_t last_ack_sequence;
} CtrlStackMockIo;

typedef struct PeerMockIo
{
    uint8_t  recv_frames[PEER_TEST_MAX_RECV][PEER_TEST_FRAME_BUF];
    uint8_t  recv_src_mac[PEER_TEST_MAX_RECV][6];
    size_t   recv_lengths[PEER_TEST_MAX_RECV];
    unsigned recv_count;
    unsigned recv_index;
    int      send_count;
    uint64_t now_us;
} PeerMockIo;

static int peer_mock_send(
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
    PeerMockIo* mock = (PeerMockIo*)user_ctx;

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

static int peer_mock_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    PeerMockIo* mock = (PeerMockIo*)user_ctx;
    size_t      copy_len;

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

static uint64_t peer_mock_monotonic(void* user_ctx)
{
    PeerMockIo* mock = (PeerMockIo*)user_ctx;

    if (mock == NULL)
    {
        return 0u;
    }

    mock->now_us += 50000u;
    return mock->now_us;
}

static void peer_mock_add_hello_reply(
    PeerMockIo*    mock,
    const uint8_t* mac,
    uint32_t       profile_id)
{
    LeapHelloReply hello;
    unsigned       idx;

    if (mock == NULL || mock->recv_count >= PEER_TEST_MAX_RECV)
    {
        return;
    }

    memset(&hello, 0, sizeof(hello));
    hello.current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
    hello.active_profile_id  = profile_id;
    hello.default_profile_id = profile_id;

    idx = mock->recv_count;
    ASSERT_TRUE(
        leap_test_frame_build(
            mock->recv_frames[idx],
            PEER_TEST_FRAME_BUF,
            &mock->recv_lengths[idx],
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO_REPLY,
            0u,
            (uint32_t)(mock->recv_count + 1u),
            (const uint8_t*)&hello,
            sizeof(hello)) == 0);
    memcpy(mock->recv_src_mac[idx], mac, 6);
    mock->recv_count++;
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

    if (mock == NULL || mock->recv_count >= 8u)
    {
        return;
    }

    idx = mock->recv_count;
    ASSERT_TRUE(
        leap_test_frame_build(
            mock->recv_frames[idx],
            512u,
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

static void peer_mock_setup_bootstrap_replies(CtrlStackMockIo* mock)
{
    LeapProfileReply     profile;
    LeapOpenSessionReply open_reply;
    LeapStateReply       state_reply;

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
    memcpy(src_mac, k_peer_a, 6);
    *payload_length = copy_len;
    mock->recv_index++;
    return 0;
}

TEST(test_controller_peer_discover_collects_two_peers)
{
    LeapControllerPeerTable table;
    LeapControllerStackIo   io;
    PeerMockIo              mock;

    leap_controller_peer_table_init(&table);

    memset(&mock, 0, sizeof(mock));
    peer_mock_add_hello_reply(&mock, k_peer_a, LEAP_PROFILE_DIGITAL_IO_16X16);
    peer_mock_add_hello_reply(&mock, k_peer_b, LEAP_PROFILE_DIGITAL_IO_16X16);

    memset(&io, 0, sizeof(io));
    io.user_ctx      = &mock;
    io.send_frame    = peer_mock_send;
    io.recv_frame    = peer_mock_recv;
    io.monotonic_us  = peer_mock_monotonic;

    ASSERT_EQ_INT(
        leap_controller_peer_table_discover(&table, &io, 500),
        LEAP_CTRL_PEER_OK);
    ASSERT_EQ_INT((int)table.count, 2);
    ASSERT_TRUE(mock.send_count >= 1);
    ASSERT_TRUE(leap_controller_peer_table_find(&table, k_peer_a) >= 0);
    ASSERT_TRUE(leap_controller_peer_table_find(&table, k_peer_b) >= 0);
}

TEST(test_controller_peer_table_full)
{
    LeapControllerPeerTable table;
    LeapControllerStackIo   io;
    PeerMockIo              mock;
    unsigned                i;

    leap_controller_peer_table_init(&table);

    memset(&mock, 0, sizeof(mock));
    for (i = 0u; i < LEAP_CTRL_MAX_PEERS + 1u; i++)
    {
        uint8_t mac[6] = { 0x02, 0x20, 0x00, 0x00, 0x00, 0x00 };

        mac[5] = (uint8_t)(i + 1u);
        peer_mock_add_hello_reply(
            &mock,
            mac,
            LEAP_PROFILE_DIGITAL_IO_16X16);
    }

    memset(&io, 0, sizeof(io));
    io.user_ctx     = &mock;
    io.send_frame   = peer_mock_send;
    io.recv_frame   = peer_mock_recv;
    io.monotonic_us = peer_mock_monotonic;

    ASSERT_EQ_INT(
        leap_controller_peer_table_discover(&table, &io, 5000),
        LEAP_CTRL_PEER_OK);
    ASSERT_EQ_INT((int)table.count, (int)LEAP_CTRL_MAX_PEERS);
}

TEST(test_controller_stack_bootstrap_peer_reaches_op)
{
    LeapControllerStack       stack;
    LeapControllerStackConfig config;
    LeapControllerStackIo     io;
    CtrlStackMockIo           mock;

    memset(&config, 0, sizeof(config));
    leap_controller_stack_init(&stack, &config);

    memset(&mock, 0, sizeof(mock));
    peer_mock_setup_bootstrap_replies(&mock);

    memset(&io, 0, sizeof(io));
    io.user_ctx   = &mock;
    io.send_frame = ctrl_stack_mock_send;
    io.recv_frame = ctrl_stack_mock_recv;

    ASSERT_EQ_INT(
        leap_controller_stack_bootstrap_peer(&stack, &io, k_peer_a, NULL),
        LEAP_CTRL_STACK_OK);
    ASSERT_EQ_INT(leap_controller_stack_get_phase(&stack), LEAP_CTRL_STACK_OP);
    ASSERT_EQ_U16(mock.last_message, LEAP_MGMT_SET_STATE);
    ASSERT_TRUE(mock.send_count >= 3u);
}

TEST(test_controller_peer_owned_by_other)
{
    LeapControllerPeerEntry entry;

    memset(&entry, 0, sizeof(entry));
    ASSERT_EQ_INT(leap_controller_peer_owned_by_other(&entry, k_ctrl_mac), 0);

    memcpy(entry.active_owner_mac, k_ctrl_mac, 6);
    ASSERT_EQ_INT(leap_controller_peer_owned_by_other(&entry, k_ctrl_mac), 0);

    memcpy(entry.active_owner_mac, k_foreign_mac, 6);
    ASSERT_EQ_INT(leap_controller_peer_owned_by_other(&entry, k_ctrl_mac), 1);
    ASSERT_EQ_INT(leap_controller_peer_owned_by_other(&entry, NULL), 1);
}

TEST(test_controller_peer_discover_ex_early_exit)
{
    LeapControllerPeerTable          table;
    LeapControllerStackIo            io;
    PeerMockIo                       mock;
    LeapControllerPeerDiscoverConfig config;

    leap_controller_peer_table_init(&table);

    memset(&mock, 0, sizeof(mock));
    peer_mock_add_hello_reply(&mock, k_peer_a, LEAP_PROFILE_DIGITAL_IO_16X16);
    peer_mock_add_hello_reply(&mock, k_peer_b, LEAP_PROFILE_DIGITAL_IO_16X16);
    peer_mock_add_hello_reply(&mock, k_peer_b, LEAP_PROFILE_DIGITAL_IO_16X16);

    memset(&io, 0, sizeof(io));
    io.user_ctx     = &mock;
    io.send_frame   = peer_mock_send;
    io.recv_frame   = peer_mock_recv;
    io.monotonic_us = peer_mock_monotonic;

    memset(&config, 0, sizeof(config));
    config.scan_duration_ms = 5000;
    config.min_peers        = 2u;

    ASSERT_EQ_INT(
        leap_controller_peer_table_discover_ex(&table, &io, &config),
        LEAP_CTRL_PEER_OK);
    ASSERT_EQ_INT((int)table.count, 2);
    ASSERT_EQ_INT((int)mock.recv_index, 2);
}

TEST(test_controller_peer_parse_mac)
{
    uint8_t mac[6];

    ASSERT_TRUE(
        leap_controller_peer_parse_mac("24:15:10:b0:5f:bc", mac) != 0);
    ASSERT_EQ_INT((int)mac[0], 0x24);
    ASSERT_EQ_INT((int)mac[5], 0xbc);

    ASSERT_TRUE(
        leap_controller_peer_parse_mac("24-15-10-b0-5f-bc", mac) != 0);
    ASSERT_EQ_INT((int)mac[3], 0xb0);

    ASSERT_EQ_INT(leap_controller_peer_parse_mac("not-a-mac", mac), 0);
}

void leap_run_controller_peer_tests(void)
{
    printf("controller peer\n");
    RUN_TEST(test_controller_peer_discover_collects_two_peers);
    RUN_TEST(test_controller_peer_discover_ex_early_exit);
    RUN_TEST(test_controller_peer_parse_mac);
    RUN_TEST(test_controller_peer_table_full);
    RUN_TEST(test_controller_stack_bootstrap_peer_reaches_op);
    RUN_TEST(test_controller_peer_owned_by_other);
}
