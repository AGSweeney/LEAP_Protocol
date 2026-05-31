/*
 * test_raw_linux_stats.c
 *
 * Transport counter get/reset (no live socket required).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_raw_linux.h"

#include <string.h>

TEST(test_raw_linux_stats_reset_clears_counters)
{
    LeapRawLinuxSocket sock;
    LeapRawLinuxStats  stats;

    memset(&sock, 0, sizeof(sock));
    sock.stats.tx_frames_ok     = 3u;
    sock.stats.tx_errors        = 1u;
    sock.stats.tx_partial_chunks = 2u;
    sock.stats.rx_frames_ok     = 5u;
    sock.stats.rx_filtered      = 4u;
    sock.stats.rx_timeouts      = 6u;

    leap_raw_linux_reset_stats(&sock);
    leap_raw_linux_get_stats(&sock, &stats);

    ASSERT_TRUE(stats.tx_frames_ok == 0u);
    ASSERT_TRUE(stats.tx_errors == 0u);
    ASSERT_TRUE(stats.tx_partial_chunks == 0u);
    ASSERT_TRUE(stats.rx_frames_ok == 0u);
    ASSERT_TRUE(stats.rx_filtered == 0u);
    ASSERT_TRUE(stats.rx_timeouts == 0u);
}

void leap_run_raw_linux_stats_tests(void)
{
    printf("raw linux stats\n");
    RUN_TEST(test_raw_linux_stats_reset_clears_counters);
}
