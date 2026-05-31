/*
 * leap_win_common.h
 *
 * Shared helpers for Windows Npcap LEAP examples.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_WIN_COMMON_H
#define LEAP_WIN_COMMON_H

#include "leap/leap_controller_stack.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_raw_winpcap.h"

typedef struct LeapWinControllerOptions
{
    const char* adapter;
    int         lease_demo;
    int         cyclic;
    unsigned    cyclic_period_ms;
    int         promiscuous;
    int         exchange;
    int         stats;
    unsigned    stats_interval;
    int         diag;
    int         list_adapters;
} LeapWinControllerOptions;

#ifdef __cplusplus
extern "C" {
#endif

int leap_win_link_stop_on_down(
    LeapRawWinpcapSocket* sock,
    volatile int*       stop_flag);

LeapPdControllerStatus leap_win_controller_run_cyclic_pd_with_link_watch(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag);

void leap_win_print_transport_error(const char* action);

void leap_win_print_transport_stats(const LeapRawWinpcapSocket* sock);

void leap_win_poll_link_and_log(LeapRawWinpcapSocket* sock);

void leap_win_controller_parse_args(
    int                        argc,
    char**                     argv,
    LeapWinControllerOptions*    options);

void leap_win_device_parse_args(
    int         argc,
    char**      argv,
    const char** adapter_out,
    int*        stats_out);

void leap_win_install_ctrl_handler(volatile int* stop_flag);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_WIN_COMMON_H */
