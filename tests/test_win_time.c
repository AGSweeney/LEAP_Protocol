/*
 * test_win_time.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#if defined(_WIN32)

#include "leap/leap_win_time.h"

TEST(test_win_monotonic_us_advances)
{
    uint64_t t0;
    uint64_t t1;

    t0 = leap_win_monotonic_us();
    leap_win_sleep_us(10000u);
    t1 = leap_win_monotonic_us();

    ASSERT_TRUE(t1 > t0);
    ASSERT_TRUE((t1 - t0) >= 8000u);
    ASSERT_TRUE((t1 - t0) <= 25000u);
}

TEST(test_win_sleep_us_50ms_accuracy)
{
    uint64_t t0;
    uint64_t elapsed_us;

    t0 = leap_win_monotonic_us();
    leap_win_sleep_us(50000u);
    elapsed_us = leap_win_monotonic_us() - t0;

    ASSERT_TRUE(elapsed_us >= 45000u);
    ASSERT_TRUE(elapsed_us <= 65000u);
}

void leap_run_win_time_tests(void)
{
    printf("win time\n");
    RUN_TEST(test_win_monotonic_us_advances);
    RUN_TEST(test_win_sleep_us_50ms_accuracy);
}

#else

void leap_run_win_time_tests(void)
{
}

#endif
