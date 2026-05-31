/*
 * leap_linux_stats.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_stats.h"

#include "leap/leap_device_stack.h"
#include "leap/leap_raw_linux.h"

#include "leap/leap_log.h"

#include <stdio.h>
#include <string.h>

void leap_linux_device_stats_init(LeapLinuxDeviceStats* stats)
{
    if (stats == NULL)
    {
        return;
    }

    memset(stats, 0, sizeof(*stats));
}

void leap_linux_device_stats_on_frame(LeapLinuxDeviceStats* stats)
{
    if (stats != NULL)
    {
        stats->frames_rx++;
    }
}

void leap_linux_device_stats_on_reject(LeapLinuxDeviceStats* stats)
{
    if (stats != NULL)
    {
        stats->frames_rejected++;
    }
}

void leap_linux_device_stats_on_result(
    LeapLinuxDeviceStats* stats,
    uint32_t              flags)
{
    if (stats == NULL)
    {
        return;
    }

    if ((flags & LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) != 0u)
    {
        stats->pd_applied++;
    }

    if ((flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u)
    {
        stats->pd_replies_sent++;
    }

    if ((flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u)
    {
        stats->mgmt_replies_sent++;
    }

    if ((flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u)
    {
        stats->disc_replies_sent++;
    }

    if ((flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u)
    {
        stats->dir_replies_sent++;
    }
}

void leap_linux_device_stats_log(
    const LeapLinuxDeviceStats* stats,
    const LeapRawLinuxSocket*   transport)
{
    LeapRawLinuxStats tstats;

    if (stats == NULL)
    {
        return;
    }

    leap_log_printf(
        "device stats: rx=%llu rejected=%llu pd_applied=%llu "
        "pd_reply=%llu mgmt=%llu disc=%llu dir=%llu tx_retries=%llu\n",
        (unsigned long long)stats->frames_rx,
        (unsigned long long)stats->frames_rejected,
        (unsigned long long)stats->pd_applied,
        (unsigned long long)stats->pd_replies_sent,
        (unsigned long long)stats->mgmt_replies_sent,
        (unsigned long long)stats->disc_replies_sent,
        (unsigned long long)stats->dir_replies_sent,
        (unsigned long long)stats->tx_send_retries);

    if (transport != NULL)
    {
        leap_raw_linux_get_stats(transport, &tstats);
        leap_log_printf(
            "transport: tx_ok=%llu tx_err=%llu rx_ok=%llu "
            "rx_filtered=%llu rx_timeout=%llu rx_err=%llu\n",
            (unsigned long long)tstats.tx_frames_ok,
            (unsigned long long)tstats.tx_errors,
            (unsigned long long)tstats.rx_frames_ok,
            (unsigned long long)tstats.rx_filtered,
            (unsigned long long)tstats.rx_timeouts,
            (unsigned long long)tstats.rx_errors);
    }
}
