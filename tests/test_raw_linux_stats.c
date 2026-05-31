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
#include "leap/leap_protocol.h"

#include <stdio.h>
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
    ASSERT_TRUE(stats.link_transitions == 0u);
}

#if defined(__linux__)

TEST(test_raw_linux_link_query_lo)
{
    LeapRawLinuxSocket    sock;
    LeapRawLinuxLinkState state;
    int                   changed;

    if (leap_raw_linux_open(
            &sock,
            "lo",
            LEAP_ETHERTYPE_DEVELOPMENT) != 0)
    {
        printf("  (skipped: AF_PACKET open failed on this host)\n");
        return;
    }

    ASSERT_EQ_INT(leap_raw_linux_query_link(&sock, &state), 0);
    ASSERT_EQ_INT(state.interface_up, 1);
    ASSERT_EQ_INT(state.link_up, 1);

    ASSERT_EQ_INT(leap_raw_linux_poll_link(&sock, &changed, &state), 0);
    ASSERT_EQ_INT(changed, 0);

    sock.cached_link_up = 0;
    ASSERT_EQ_INT(leap_raw_linux_poll_link(&sock, &changed, &state), 0);
    ASSERT_EQ_INT(changed, 1);
    ASSERT_TRUE(sock.stats.link_transitions == 1u);
    ASSERT_EQ_INT(sock.cached_link_up, 1);

    leap_raw_linux_close(&sock);
}

#endif

void leap_run_raw_linux_stats_tests(void)
{
    printf("raw linux stats\n");
    RUN_TEST(test_raw_linux_stats_reset_clears_counters);
#if defined(__linux__)
    RUN_TEST(test_raw_linux_link_query_lo);
#endif
}
