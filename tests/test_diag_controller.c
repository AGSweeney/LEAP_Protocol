/*
 * test_diag_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_diag_controller.h"
#include "leap/leap_protocol.h"

#include <string.h>

TEST(test_diag_controller_build_read_counters)
{
    uint8_t payload[32];
    size_t  length;

    length = leap_diag_controller_build_read_counters(
        payload,
        sizeof(payload),
        (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED,
        4u,
        0u);
    ASSERT_TRUE(length == sizeof(LeapReadCountersRequest));
}

TEST(test_diag_controller_parse_counters_reply)
{
    uint8_t           raw[128];
    LeapCountersReply hdr;
    LeapCounterEntry  entries[4];
    size_t            count = 0u;

    memset(raw, 0, sizeof(raw));
    hdr.counter_count = 2u;
    memcpy(raw, &hdr, sizeof(hdr));

    ((LeapCounterEntry*)(raw + sizeof(hdr)))[0].counter_id = (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED;
    ((LeapCounterEntry*)(raw + sizeof(hdr)))[0].value       = 42u;
    ((LeapCounterEntry*)(raw + sizeof(hdr)))[1].counter_id = (uint16_t)LEAP_COUNTER_CRC_FAILURES;
    ((LeapCounterEntry*)(raw + sizeof(hdr)))[1].value       = 1u;

    ASSERT_EQ_INT(
        leap_diag_controller_on_counters_reply(
            raw,
            sizeof(hdr) + (2u * sizeof(LeapCounterEntry)),
            &hdr,
            entries,
            4u,
            &count),
        LEAP_DIAG_CTRL_OK);
    ASSERT_EQ_U16((uint16_t)count, 2u);
    ASSERT_EQ_U16(entries[0].counter_id, (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED);
    ASSERT_TRUE(entries[0].value == 42u);
}

TEST(test_diag_controller_parse_timing_reply)
{
    LeapTimingReply reply;
    uint8_t         raw[sizeof(LeapTimingReply)];

    memset(&reply, 0, sizeof(reply));
    reply.last_cycle_time_us            = 100000u;
    reply.process_watchdog_remaining_us = 50000u;
    memcpy(raw, &reply, sizeof(reply));

    memset(&reply, 0, sizeof(reply));
    ASSERT_EQ_INT(
        leap_diag_controller_on_timing_reply(raw, sizeof(raw), &reply),
        LEAP_DIAG_CTRL_OK);
    ASSERT_EQ_U32(reply.last_cycle_time_us, 100000u);
    ASSERT_EQ_U32(reply.process_watchdog_remaining_us, 50000u);
}

void leap_run_diag_controller_tests(void)
{
    printf("diag controller\n");
    RUN_TEST(test_diag_controller_build_read_counters);
    RUN_TEST(test_diag_controller_parse_counters_reply);
    RUN_TEST(test_diag_controller_parse_timing_reply);
}
