/*
 * test_pd_controller.c
 *
 * Mock-I/O unit tests for leap_pd_controller.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/conformance/leap_conformance_scenario.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_protocol.h"

#include <string.h>

static const uint8_t k_peer_mac[6] = { 0x02, 0x66, 0x77, 0x88, 0x99, 0xAA };

typedef struct PdCtrlMockIo
{
    uint64_t now_us;
    uint32_t pd_send_count;
    uint32_t hb_send_count;
    uint32_t last_message_type;
    uint32_t exchange_reply_count;
    uint16_t last_outputs;
} PdCtrlMockIo;

static int mock_send_pd(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    PdCtrlMockIo* mock = (PdCtrlMockIo*)user_ctx;
    const LeapEndpointDataHeader* hdr;
    const LeapProfileDigital16x16* profile;

    (void)peer_mac;
    (void)session_id;
    (void)sequence;

    if (mock == NULL || payload == NULL)
    {
        return -1;
    }

    mock->pd_send_count++;
    mock->last_message_type = message_type;
    mock->now_us += 3000u;

    if (message_type == LEAP_PD_WRITE_ENDPOINT &&
        payload_length >= sizeof(LeapEndpointDataHeader) + sizeof(LeapProfileDigital16x16))
    {
        hdr = (const LeapEndpointDataHeader*)payload;
        profile = (const LeapProfileDigital16x16*)(payload + sizeof(LeapEndpointDataHeader));
        (void)hdr;
        mock->last_outputs = profile->digital_outputs;
    }

    return 0;
}

static int mock_send_heartbeat(
    void*          user_ctx,
    const uint8_t* peer_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    PdCtrlMockIo* mock = (PdCtrlMockIo*)user_ctx;

    (void)peer_mac;
    (void)payload;
    (void)payload_length;
    (void)session_id;
    (void)sequence;

    if (mock == NULL)
    {
        return -1;
    }

    mock->hb_send_count++;
    return 0;
}

static uint64_t mock_monotonic_us(void* user_ctx)
{
    PdCtrlMockIo* mock = (PdCtrlMockIo*)user_ctx;

    if (mock == NULL)
    {
        return 0u;
    }

    return mock->now_us;
}

static void pd_ctrl_setup_session(
    LeapMgmtControllerContext* mgmt,
    LeapPdControllerContext*     pd)
{
    LeapMgmtControllerEvent event;
    LeapOpenSessionReply    open_reply;
    LeapStateReply          state_reply;
    LeapPdControllerConfig  pd_config;

    memset(&pd_config, 0, sizeof(pd_config));
    pd_config.cycle_period_ms          = 10u;
    pd_config.heartbeat_every_n_cycles = 2u;
    leap_pd_controller_init(pd, &pd_config);

    memset(&open_reply, 0, sizeof(open_reply));
    open_reply.assigned_session_id      = 7u;
    open_reply.granted_lease_time_us    = 1000000u;
    open_reply.granted_watchdog_time_us = 200000u;
    open_reply.current_state            = (uint16_t)LEAP_STATE_SAFE;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_open_session_reply(
            mgmt,
            (const uint8_t*)&open_reply,
            sizeof(open_reply),
            &event),
        LEAP_MGMT_CTRL_OK);

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.accepted_state = (uint16_t)LEAP_STATE_OP;
    state_reply.current_state  = (uint16_t)LEAP_STATE_OP;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_state_reply(
            mgmt,
            (const uint8_t*)&state_reply,
            sizeof(state_reply),
            &event),
        LEAP_MGMT_CTRL_OK);

    (void)event;
}

TEST(test_pd_controller_single_write_ok)
{
    LeapMgmtControllerContext mgmt;
    LeapMgmtControllerConfig  mgmt_config;
    LeapPdControllerContext   pd;
    LeapPdControllerIo        io;
    PdCtrlMockIo              mock;
    volatile int              stop = 0;

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    leap_mgmt_controller_init(&mgmt, &mgmt_config);
    pd_ctrl_setup_session(&mgmt, &pd);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx     = &mock;
    io.send_pd      = mock_send_pd;
    io.monotonic_us = mock_monotonic_us;

    ASSERT_EQ_INT(
        leap_pd_controller_send_single_write(&pd, &mgmt, &io, k_peer_mac, 0x0015u),
        LEAP_PD_CTRL_OK);
    ASSERT_EQ_U32(mock.pd_send_count, 1u);
    ASSERT_EQ_U16(mock.last_outputs, 0x0015u);
    ASSERT_EQ_U32(pd.stats.pd_sent_ok, 1u);

    ASSERT_EQ_INT(
        leap_pd_controller_run_one_cycle(&pd, &mgmt, &io, k_peer_mac, &stop, 0),
        LEAP_PD_CTRL_OK);
    (void)stop;
}

TEST(test_pd_controller_cycle_metrics_and_heartbeat)
{
    LeapMgmtControllerContext mgmt;
    LeapMgmtControllerConfig  mgmt_config;
    LeapPdControllerContext   pd;
    LeapPdControllerIo        io;
    PdCtrlMockIo              mock;
    volatile int              stop = 0;
    int                       i;

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    leap_mgmt_controller_init(&mgmt, &mgmt_config);
    pd_ctrl_setup_session(&mgmt, &pd);
    mgmt.granted_lease_us       = 1000000u;
    mgmt.last_lease_refresh_us  = 0u;

    memset(&mock, 0, sizeof(mock));
    mock.now_us = 1000u;
    memset(&io, 0, sizeof(io));
    io.user_ctx        = &mock;
    io.send_pd         = mock_send_pd;
    io.send_heartbeat  = mock_send_heartbeat;
    io.monotonic_us    = mock_monotonic_us;

    for (i = 0; i < 3; i++)
    {
        mock.now_us += 12000u;
        ASSERT_EQ_INT(
            leap_pd_controller_run_one_cycle(&pd, &mgmt, &io, k_peer_mac, &stop, 0),
            LEAP_PD_CTRL_OK);
    }

    ASSERT_EQ_U32(pd.stats.cycles_completed, 3u);
    ASSERT_TRUE(pd.stats.last_cycle_work_us > 0u);
    ASSERT_TRUE(pd.stats.min_cycle_period_us > 0u);
    ASSERT_TRUE(pd.stats.last_cycle_jitter_us > 0u);
    ASSERT_TRUE(mock.hb_send_count >= 1u);
}

TEST(test_pd_controller_send_failure_status)
{
    LeapMgmtControllerContext mgmt;
    LeapMgmtControllerConfig  mgmt_config;
    LeapPdControllerContext   pd;
    LeapPdControllerIo        io;

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    leap_mgmt_controller_init(&mgmt, &mgmt_config);
    pd_ctrl_setup_session(&mgmt, &pd);

    memset(&io, 0, sizeof(io));

    ASSERT_EQ_INT(
        leap_pd_controller_send_single_write(&pd, &mgmt, &io, k_peer_mac, 0x0001u),
        LEAP_PD_CTRL_IO_MISSING);
}

TEST(test_pd_controller_random_output_single_bit)
{
    LeapMgmtControllerContext mgmt;
    LeapMgmtControllerConfig  mgmt_config;
    LeapPdControllerContext   pd;
    LeapPdControllerIo        io;
    PdCtrlMockIo              mock;
    volatile int              stop = 0;
    int                       i;

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    leap_mgmt_controller_init(&mgmt, &mgmt_config);
    pd_ctrl_setup_session(&mgmt, &pd);
    pd.config.random_output = 1;
    leap_pd_controller_seed_rand(0x12345678u);

    memset(&mock, 0, sizeof(mock));
    memset(&io, 0, sizeof(io));
    io.user_ctx     = &mock;
    io.send_pd      = mock_send_pd;
    io.monotonic_us = mock_monotonic_us;

    for (i = 0; i < 24; i++)
    {
        ASSERT_EQ_INT(
            leap_pd_controller_run_one_cycle(&pd, &mgmt, &io, k_peer_mac, &stop, 0),
            LEAP_PD_CTRL_OK);
        ASSERT_TRUE(mock.last_outputs != 0u);
        ASSERT_TRUE((mock.last_outputs & (mock.last_outputs - 1u)) == 0u);
    }
}

TEST(test_pd_controller_network_rtt_percentile)
{
    LeapPdControllerStats stats;

    memset(&stats, 0, sizeof(stats));

    stats.network_rtt_samples = 100u;
    stats.network_rtt_hist[0] = 98u;
    stats.network_rtt_hist[2] = 2u;
    ASSERT_TRUE(
        leap_pd_stats_network_rtt_percentile_us(&stats, 99u) == 2000u);
    ASSERT_TRUE(
        leap_pd_stats_network_rtt_percentile_permille_us(&stats, 999u) == 2000u);

    memset(&stats, 0, sizeof(stats));
    stats.network_rtt_samples = 274214u;
    stats.network_rtt_hist[0] = 273000u;
    stats.network_rtt_hist[1] = 1000u;
    stats.network_rtt_hist[2] = 200u;
    stats.network_rtt_hist[3] = 14u;
    ASSERT_TRUE(
        leap_pd_stats_network_rtt_percentile_us(&stats, 99u) <= 2000u);
    stats.max_network_rtt_us = 3280u;
    ASSERT_TRUE(leap_conf_io_bench_wire_rtt_pass(&stats, 0u) != 0);

    memset(&stats, 0, sizeof(stats));
    stats.network_rtt_samples = 100u;
    stats.network_rtt_hist[LEAP_PD_NETWORK_RTT_HIST_BUCKETS - 1u] = 100u;
    stats.max_network_rtt_us = 99773u;
    ASSERT_TRUE(
        leap_pd_stats_network_rtt_percentile_us(&stats, 99u) == 99773u);
    ASSERT_TRUE(
        leap_pd_stats_network_rtt_percentile_us(&stats, 99u) != UINT32_MAX);
}

void leap_run_pd_controller_tests(void)
{
    printf("pd controller\n");
    RUN_TEST(test_pd_controller_single_write_ok);
    RUN_TEST(test_pd_controller_cycle_metrics_and_heartbeat);
    RUN_TEST(test_pd_controller_send_failure_status);
    RUN_TEST(test_pd_controller_random_output_single_bit);
    RUN_TEST(test_pd_controller_network_rtt_percentile);
}
