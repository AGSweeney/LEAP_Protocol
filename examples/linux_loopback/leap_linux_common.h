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
} LeapLinuxControllerOptions;

void leap_linux_print_mac(const char* label, const uint8_t* mac);

void leap_linux_print_transport_error(const char* action);

void leap_linux_print_transport_stats(const LeapRawLinuxSocket* sock);

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

#endif /* LEAP_LINUX_COMMON_H */
