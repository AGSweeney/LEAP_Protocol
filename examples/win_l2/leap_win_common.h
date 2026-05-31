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

#include "leap/leap_controller_session_hub.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_controller_peer.h"
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

typedef struct LeapWinHubOptions
{
    const char* adapter;
    int         scan_ms;
    unsigned    cyclic_period_ms;
    unsigned    min_peers;
    int         promiscuous;
    int         exchange;
    int         pacing;
    int         parallel;
    int         random_peer;
    int         stats;
    unsigned    stats_interval;
    unsigned    run_sec;
    int         list_adapters;
    unsigned    peer_mac_count;
    uint8_t     peer_macs[LEAP_CTRL_MAX_PEERS][6];
    int         peer_mac_slot[LEAP_CTRL_MAX_PEERS];
} LeapWinHubOptions;

typedef struct LeapWinDiscoverOptions
{
    const char* adapter;
    int         scan_ms;
    unsigned    min_peers;
    int         promiscuous;
    int         list_adapters;
} LeapWinDiscoverOptions;

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

LeapPdControllerStatus leap_win_hub_run_round_robin_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag,
    int                       sleep_for_period);

LeapPdControllerStatus leap_win_hub_run_parallel_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag,
    int                       sleep_for_period);

LeapPdControllerStatus leap_win_hub_run_random_peer_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawWinpcapSocket*     transport,
    volatile int*             stop_flag,
    int                       sleep_for_period);

void leap_win_print_transport_error(const char* action);

void leap_win_print_transport_stats(const LeapRawWinpcapSocket* sock);

void leap_win_poll_link_and_log(LeapRawWinpcapSocket* sock);

void leap_win_controller_parse_args(
    int                        argc,
    char**                     argv,
    LeapWinControllerOptions*    options);

void leap_win_hub_parse_args(
    int               argc,
    char**            argv,
    LeapWinHubOptions* options);

LeapControllerPeerStatus leap_win_hub_discover_peers(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    const LeapWinHubOptions*     options);

void leap_win_discover_parse_args(
    int                    argc,
    char**                 argv,
    LeapWinDiscoverOptions* options);

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
