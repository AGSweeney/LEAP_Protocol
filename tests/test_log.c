/*
 * test_log.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_log.h"

#include <string.h>

static uint64_t g_test_log_now_us = 0u;

static uint64_t test_log_monotonic(void)
{
    return g_test_log_now_us;
}

TEST(test_log_timestamp_relative)
{
    char buf[32];

    leap_log_set_monotonic_us_fn(test_log_monotonic);
    g_test_log_now_us = 1000000u;
    leap_log_reset_origin();

    g_test_log_now_us = 3456789u;
    ASSERT_TRUE(leap_log_format_timestamp(buf, sizeof(buf)) > 0);
    ASSERT_TRUE(strstr(buf, "[+") != NULL);
    ASSERT_TRUE(strstr(buf, "2.456s]") != NULL);

    leap_log_set_monotonic_us_fn(NULL);
}

void leap_run_log_tests(void)
{
    printf("leap log\n");
    RUN_TEST(test_log_timestamp_relative);
}
