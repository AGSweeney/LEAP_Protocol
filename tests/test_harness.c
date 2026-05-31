/*
 * test_harness.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

int g_tests_run;
int g_tests_failed;
int g_current_test_failed;

int leap_test_summary(void)
{
    printf("\n%d test(s), %d failure(s)\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
