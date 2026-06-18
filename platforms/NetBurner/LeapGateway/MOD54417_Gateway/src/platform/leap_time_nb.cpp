/*
 * leap_time_nb.cpp - Monotonic uptime for LEAP Gateway on NetBurner.
 *
 * SPDX-License-Identifier: MIT
 */

#include "leap_time.h"

#include <cstdio>

#include <hal.h>
#include <init.h>
#include <nbrtos.h>
#include <predef.h>

uint64_t leap_rtems_monotonic_us(void)
{
    uint32_t ticks1;
    uint32_t ticks2;
    uint16_t fraction;
    uint64_t us_per_tick;
    uint64_t fraction_us;

    do
    {
        ticks1 = static_cast<uint32_t>(TimeTick);
        fraction = HalGetTickFraction();
        ticks2 = static_cast<uint32_t>(TimeTick);
    } while (ticks1 != ticks2);

    us_per_tick = 1000000ULL / static_cast<uint64_t>(TICKS_PER_SECOND);
    fraction_us = static_cast<uint64_t>(fraction) * us_per_tick;
    if (HalTickMaxCount > 0U)
    {
        fraction_us /= static_cast<uint64_t>(HalTickMaxCount);
    }
    return static_cast<uint64_t>(ticks1) * us_per_tick + fraction_us;
}

const char* leap_rtems_uptime_str(void)
{
    static char buf[16];
    const uint64_t us = leap_rtems_monotonic_us();
    const unsigned sec = static_cast<unsigned>(us / 1000000ULL);
    const unsigned ms = static_cast<unsigned>((us / 1000ULL) % 1000ULL);
    snprintf(buf, sizeof(buf), "%4u.%03u", sec, ms);
    return buf;
}
