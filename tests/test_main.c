/*
 * test_main.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

void leap_run_crc_tests(void);
void leap_run_frame_vector_tests(void);
void leap_run_mgmt_device_tests(void);

int main(void)
{
    printf("LEAP conformance tests\n");

    leap_run_crc_tests();
    leap_run_frame_vector_tests();
    leap_run_mgmt_device_tests();

    return leap_test_summary();
}
