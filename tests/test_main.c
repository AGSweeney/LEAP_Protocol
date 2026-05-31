/*
 * test_main.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

void leap_run_crc_tests(void);
void leap_run_frame_vector_tests(void);
void leap_run_frame_roundtrip_tests(void);
void leap_run_frame_fuzz_tests(void);
void leap_run_frame_fragment_tests(void);
void leap_run_mgmt_device_tests(void);
void leap_run_mgmt_boundary_tests(void);
void leap_run_mgmt_process_tests(void);
void leap_run_pd_device_tests(void);
void leap_run_pd_controller_tests(void);
void leap_run_pd_common_tests(void);
void leap_run_disc_device_tests(void);
void leap_run_disc_controller_tests(void);
void leap_run_dir_device_tests(void);
void leap_run_diag_device_tests(void);
void leap_run_diag_controller_tests(void);
void leap_run_controller_stack_tests(void);
void leap_run_controller_peer_tests(void);
void leap_run_controller_sequence_tests(void);
void leap_run_controller_session_hub_tests(void);
void leap_run_mgmt_controller_tests(void);
void leap_run_device_stack_tests(void);
void leap_run_comms_loss_tests(void);
void leap_run_raw_linux_stats_tests(void);
void leap_run_win_time_tests(void);
void leap_run_log_tests(void);

int main(void)
{
    printf("LEAP conformance tests\n");

    leap_run_crc_tests();
    leap_run_frame_vector_tests();
    leap_run_frame_roundtrip_tests();
    leap_run_frame_fuzz_tests();
    leap_run_frame_fragment_tests();
    leap_run_mgmt_device_tests();
    leap_run_mgmt_boundary_tests();
    leap_run_mgmt_process_tests();
    leap_run_pd_device_tests();
    leap_run_pd_controller_tests();
    leap_run_pd_common_tests();
    leap_run_disc_device_tests();
    leap_run_disc_controller_tests();
    leap_run_dir_device_tests();
    leap_run_diag_device_tests();
    leap_run_diag_controller_tests();
    leap_run_controller_stack_tests();
    leap_run_controller_peer_tests();
    leap_run_controller_sequence_tests();
    leap_run_controller_session_hub_tests();
    leap_run_mgmt_controller_tests();
    leap_run_device_stack_tests();
    leap_run_comms_loss_tests();
    leap_run_raw_linux_stats_tests();
    leap_run_win_time_tests();
    leap_run_log_tests();

    return leap_test_summary();
}
