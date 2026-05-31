/*
 * test_frame_fuzz.c
 *
 * Simple single-byte mutation fuzzing for leap_frame_parse().
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "test_util.h"
#include "leap_test_frame.h"

#include "leap/leap_frame.h"

#include <string.h>

#define TEST_FUZZ_BUF_SIZE 256u
#define TEST_FUZZ_ITERATIONS 512u

TEST(test_frame_fuzz_golden_pd_no_crash)
{
    static const char* const k_hex =
        "4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38"
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100";
    uint8_t       original[TEST_FUZZ_BUF_SIZE];
    uint8_t       mutated[TEST_FUZZ_BUF_SIZE];
    size_t        frame_length = 0u;
    LeapFrameView view;
    uint32_t      i;
    int           ok_count = 0;

    ASSERT_TRUE(leap_test_hex_decode(k_hex, original, TEST_FUZZ_BUF_SIZE, &frame_length) == 0);

    for (i = 0u; i < TEST_FUZZ_ITERATIONS; i++)
    {
        memcpy(mutated, original, frame_length);
        leap_test_frame_mutate_byte(mutated, frame_length, i);

        memset(&view, 0, sizeof(view));
        if (leap_frame_parse(mutated, frame_length, &view) == LEAP_FRAME_OK)
        {
            ok_count++;
        }
    }

    ASSERT_TRUE(ok_count <= 1);
}

void leap_run_frame_fuzz_tests(void)
{
    printf("frame fuzz\n");
    RUN_TEST(test_frame_fuzz_golden_pd_no_crash);
}
