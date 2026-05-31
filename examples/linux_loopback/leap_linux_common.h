/*
 * leap_linux_common.h
 *
 * Shared helpers for Linux loopback examples.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LINUX_COMMON_H
#define LEAP_LINUX_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_raw_linux.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_pd_controller.h"

typedef struct LeapLinuxControllerOptions
{
    const char* ifname;
    int         lease_demo;
    int         cyclic;
    unsigned    cyclic_period_ms;
    int         promiscuous;
    int         exchange;
    int         stats;
    unsigned    stats_interval;
    int         diag;
} LeapLinuxControllerOptions;

#ifdef __cplusplus
extern "C" {
#endif

int leap_linux_link_stop_on_down(
    LeapRawLinuxSocket* sock,
    volatile int*       stop_flag);

LeapPdControllerStatus leap_linux_controller_run_cyclic_pd_with_link_watch(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    LeapRawLinuxSocket*       transport,
    volatile int*             stop_flag);

LeapPdControllerStatus leap_linux_hub_run_round_robin_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawLinuxSocket*       transport,
    volatile int*             stop_flag);

LeapPdControllerStatus leap_linux_hub_run_parallel_with_link_watch(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* pd_io,
    LeapRawLinuxSocket*       transport,
    volatile int*             stop_flag,
    int                       sleep_for_period);

void leap_linux_print_mac(const char* label, const uint8_t* mac);

void leap_linux_print_transport_error(const char* action);

void leap_linux_print_transport_stats(const LeapRawLinuxSocket* sock);

void leap_linux_poll_link_and_log(LeapRawLinuxSocket* sock);

uint64_t leap_linux_send_retry_count(void);

void leap_linux_reset_send_retry_count(void);

int leap_linux_send_leap(
    LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    uint8_t                   flags,
    uint16_t                  service_id,
    uint16_t                  message_type,
    uint32_t                  session_id,
    uint32_t                  sequence,
    uint32_t                  ack_sequence,
    const uint8_t*            payload,
    size_t                    payload_length);

int leap_linux_send_leap_retry(
    LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    uint8_t                   flags,
    uint16_t                  service_id,
    uint16_t                  message_type,
    uint32_t                  session_id,
    uint32_t                  sequence,
    uint32_t                  ack_sequence,
    const uint8_t*            payload,
    size_t                    payload_length,
    int                       max_attempts);

int leap_linux_recv_leap(
    LeapRawLinuxSocket* sock,
    uint8_t*                  src_mac,
    uint8_t*                  payload,
    size_t                    payload_capacity,
    size_t*                   payload_length,
    int                       timeout_ms);

void leap_linux_controller_parse_args(
    int                       argc,
    char**                    argv,
    LeapLinuxControllerOptions* options);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_LINUX_COMMON_H */
