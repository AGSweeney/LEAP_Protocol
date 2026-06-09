/*
 * leap_time.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_time.h"

#include <stdio.h>
#include <time.h>

uint64_t leap_rtems_monotonic_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0u;
    }

    return ((uint64_t)ts.tv_sec * 1000000u) + ((uint64_t)ts.tv_nsec / 1000u);
}

const char *leap_rtems_uptime_str(void)
{
    static char buf[20];
    const uint64_t us = leap_rtems_monotonic_us();
    const unsigned long sec = (unsigned long)(us / 1000000u);
    const unsigned long ms = (unsigned long)((us / 1000u) % 1000u);

    (void)snprintf(buf, sizeof(buf), "%5lu.%03lu", sec, ms);
    return buf;
}
