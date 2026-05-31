/*
 * test_controller_sequence.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_controller_sequence.h"

#include <string.h>

TEST(test_frame_sequence_duplicate_rejected)
{
    LeapControllerFrameSequenceState    state;
    LeapControllerFrameSequenceConfig config;

    leap_controller_frame_sequence_init(&state);
    memset(&config, 0, sizeof(config));

    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 5u),
        LEAP_CTRL_FRAME_SEQ_OK);
    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 5u),
        LEAP_CTRL_FRAME_SEQ_DUPLICATE);
    ASSERT_EQ_U32(state.duplicate_frames, 1u);
}

TEST(test_frame_sequence_gap_counted)
{
    LeapControllerFrameSequenceState    state;
    LeapControllerFrameSequenceConfig config;

    leap_controller_frame_sequence_init(&state);
    memset(&config, 0, sizeof(config));

    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 1u),
        LEAP_CTRL_FRAME_SEQ_OK);
    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 4u),
        LEAP_CTRL_FRAME_SEQ_OK);
    ASSERT_EQ_U32(state.sequence_gaps, 2u);
}

TEST(test_frame_sequence_session_mismatch)
{
    LeapControllerFrameSequenceState    state;
    LeapControllerFrameSequenceConfig config;

    leap_controller_frame_sequence_init(&state);
    memset(&config, 0, sizeof(config));
    config.enforce_session_match = 1;

    leap_controller_frame_sequence_bind_session(&state, 42u);

    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 99u, 10u),
        LEAP_CTRL_FRAME_SEQ_SESSION_MISMATCH);
    ASSERT_EQ_U32(state.session_mismatches, 1u);
}

TEST(test_frame_sequence_out_of_window_reject)
{
    LeapControllerFrameSequenceState    state;
    LeapControllerFrameSequenceConfig config;

    leap_controller_frame_sequence_init(&state);
    memset(&config, 0, sizeof(config));
    config.window_size           = 8u;
    config.reject_out_of_window  = 1;

    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 1u),
        LEAP_CTRL_FRAME_SEQ_OK);
    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 100u),
        LEAP_CTRL_FRAME_SEQ_OUT_OF_WINDOW);
    ASSERT_EQ_U32(state.out_of_window_rejects, 1u);
}

TEST(test_frame_sequence_gap_rejected)
{
    LeapControllerFrameSequenceState    state;
    LeapControllerFrameSequenceConfig config;

    leap_controller_frame_sequence_init(&state);
    memset(&config, 0, sizeof(config));
    config.reject_sequence_gaps = 1;

    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 1u),
        LEAP_CTRL_FRAME_SEQ_OK);
    ASSERT_EQ_INT(
        leap_controller_frame_sequence_accept(&state, &config, 0u, 4u),
        LEAP_CTRL_FRAME_SEQ_GAP);
    ASSERT_EQ_U32(state.gap_rejects, 1u);
    ASSERT_EQ_U32(state.highest_peer_sequence, 1u);
}

void leap_run_controller_sequence_tests(void)
{
    printf("controller sequence\n");
    RUN_TEST(test_frame_sequence_duplicate_rejected);
    RUN_TEST(test_frame_sequence_gap_counted);
    RUN_TEST(test_frame_sequence_session_mismatch);
    RUN_TEST(test_frame_sequence_out_of_window_reject);
    RUN_TEST(test_frame_sequence_gap_rejected);
}
