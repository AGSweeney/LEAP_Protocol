/*
 * leap_linux_stats.h
 *
 * Lightweight runtime statistics for the Linux loopback examples.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LINUX_STATS_H
#define LEAP_LINUX_STATS_H

#include <stdint.h>

#include "leap/leap_raw_linux.h"

typedef struct LeapLinuxDeviceStats
{
    uint64_t frames_rx;
    uint64_t frames_rejected;
    uint64_t pd_applied;
    uint64_t pd_replies_sent;
    uint64_t mgmt_replies_sent;
    uint64_t disc_replies_sent;
    uint64_t dir_replies_sent;
    uint64_t tx_send_retries;
} LeapLinuxDeviceStats;

void leap_linux_device_stats_init(LeapLinuxDeviceStats* stats);

void leap_linux_device_stats_on_frame(LeapLinuxDeviceStats* stats);

void leap_linux_device_stats_on_reject(LeapLinuxDeviceStats* stats);

void leap_linux_device_stats_on_result(
    LeapLinuxDeviceStats* stats,
    uint32_t              flags);

void leap_linux_device_stats_log(
    const LeapLinuxDeviceStats* stats,
    const LeapRawLinuxSocket*   transport);

#endif /* LEAP_LINUX_STATS_H */
