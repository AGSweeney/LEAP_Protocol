/*
 * test_frame_fragment.c
 *
 * Fragmented frames are rejected at service layers (reassembly not implemented).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_mgmt_device.h"
#include "leap/leap_mgmt_process.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_FRAG_BUF_SIZE 256u

static const uint8_t k_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };

TEST(test_fragment_rejected_by_mgmt_process)
{
    LeapFragmentHeader     frag;
    LeapMgmtDeviceContext  ctx;
    LeapMgmtProcessResult  result;
    uint8_t                frame[TEST_FRAG_BUF_SIZE];
    size_t                 frame_length = 0u;

    leap_mgmt_device_init(&ctx, NULL);

    memset(&frag, 0, sizeof(frag));
    frag.fragment_group_id = 1u;
    frag.fragment_index    = 0u;
    frag.fragment_count    = 2u;
    frag.total_length      = 64u;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_FRAG_BUF_SIZE,
            &frame_length,
            (uint8_t)(LEAP_FLAG_ACK_REQUESTED | LEAP_FLAG_FRAGMENTED),
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_HEARTBEAT,
            1u,
            1u,
            (const uint8_t*)&frag,
            sizeof(frag)) == 0);

    ASSERT_EQ_INT(
        leap_mgmt_process_frame(&ctx, k_mac, 0u, frame, frame_length, &result),
        LEAP_MGMT_PROCESS_UNSUPPORTED_FRAME);
}

TEST(test_fragment_rejected_by_pd_device)
{
    LeapFragmentHeader     frag;
    LeapMgmtDeviceContext  ctx;
    LeapPdDeviceResult     result;
    uint8_t                frame[TEST_FRAG_BUF_SIZE];
    size_t                 frame_length = 0u;

    leap_mgmt_device_init(&ctx, NULL);

    memset(&frag, 0, sizeof(frag));
    frag.fragment_count = 2u;
    frag.total_length   = 128u;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_FRAG_BUF_SIZE,
            &frame_length,
            LEAP_FLAG_FRAGMENTED,
            (uint16_t)LEAP_SERVICE_PD,
            LEAP_PD_WRITE_ENDPOINT,
            1u,
            1u,
            (const uint8_t*)&frag,
            sizeof(frag)) == 0);

    ASSERT_EQ_INT(
        leap_pd_device_process_frame(&ctx, NULL, k_mac, 0u, frame, frame_length, &result),
        LEAP_PD_DEVICE_REJECTED);
}

void leap_run_frame_fragment_tests(void)
{
    printf("frame fragment\n");
    RUN_TEST(test_fragment_rejected_by_mgmt_process);
    RUN_TEST(test_fragment_rejected_by_pd_device);
}
