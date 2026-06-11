/*
 * leap_time_linux.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_time.h"

#include <stdio.h>
#include <time.h>

uint64_t
leap_rtems_monotonic_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0u;
    }

    return ((uint64_t)ts.tv_sec * 1000000u) + ((uint64_t)ts.tv_nsec / 1000u);
}

const char*
leap_rtems_uptime_str(void)
{
    static char     buf[20];
    static uint64_t start_us;
    uint64_t        now_us;
    uint64_t        delta_ms;

    now_us = leap_rtems_monotonic_us();
    if (start_us == 0u)
    {
        start_us = now_us;
    }

    delta_ms = (now_us - start_us) / 1000u;
    (void)snprintf(
        buf,
        sizeof(buf),
        "%4u.%03u",
        (unsigned)(delta_ms / 1000u),
        (unsigned)(delta_ms % 1000u));
    return buf;
}
