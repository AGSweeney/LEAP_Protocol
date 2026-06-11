/*
 * clearcore_leap_locate.cpp
 *
 * Drives the on-board LED next to the USB port during LEAP LOCATE_DEVICE.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "clearcore_leap_locate.h"

#include "leap/leap_protocol.h"

#include "ClearCore.h"

static uint64_t g_locate_until_us;
static uint64_t g_locate_next_toggle_us;
static uint8_t  g_locate_led_on;
static uint8_t  g_locate_pattern;
static uint8_t  g_locate_solid;

static void clearcore_leap_locate_set_led(int on)
{
    (void)ConnectorLed.State(on != 0);
    g_locate_led_on = (uint8_t)(on != 0);
}

static void clearcore_leap_locate_stop(uint64_t now_us)
{
    (void)now_us;
    g_locate_until_us       = 0u;
    g_locate_next_toggle_us = 0u;
    g_locate_solid          = 0u;
    clearcore_leap_locate_set_led(0);
}

static uint32_t clearcore_leap_locate_toggle_us(uint8_t pattern)
{
    switch (pattern)
    {
    case LEAP_LOCATE_PATTERN_FAST_BLINK:
        return 125000u;
    case LEAP_LOCATE_PATTERN_DOUBLE_BLINK:
        return 200000u;
    case LEAP_LOCATE_PATTERN_SLOW_BLINK:
    case LEAP_LOCATE_PATTERN_DEFAULT:
    default:
        return 500000u;
    }
}

extern "C" void clearcore_leap_locate_init(void)
{
    g_locate_until_us       = 0u;
    g_locate_next_toggle_us = 0u;
    g_locate_led_on         = 0u;
    g_locate_pattern        = LEAP_LOCATE_PATTERN_DEFAULT;
    g_locate_solid          = 0u;
    clearcore_leap_locate_set_led(0);
}

extern "C" void clearcore_leap_locate_start(
    uint32_t duration_us,
    uint8_t  pattern,
    int      cancel)
{
    const uint64_t now_us = (uint64_t)Microseconds();

    if (cancel != 0)
    {
        clearcore_leap_locate_stop(now_us);
        return;
    }

    if (duration_us == 0u)
    {
        duration_us = 3000000u;
    }

    g_locate_until_us       = now_us + (uint64_t)duration_us;
    g_locate_next_toggle_us = 0u;
    g_locate_pattern        = pattern;
    g_locate_solid =
        (pattern == LEAP_LOCATE_PATTERN_SOLID) ? 1u : 0u;

    if (g_locate_solid != 0u)
    {
        clearcore_leap_locate_set_led(1);
    }
}

extern "C" void clearcore_leap_locate_update(uint64_t now_us)
{
    if (g_locate_until_us == 0u)
    {
        return;
    }

    if (now_us >= g_locate_until_us)
    {
        clearcore_leap_locate_stop(now_us);
        return;
    }

    if (g_locate_solid != 0u)
    {
        if (g_locate_led_on == 0u)
        {
            clearcore_leap_locate_set_led(1);
        }
        return;
    }

    if (g_locate_next_toggle_us == 0u || now_us >= g_locate_next_toggle_us)
    {
        clearcore_leap_locate_set_led(g_locate_led_on == 0u ? 1 : 0);
        g_locate_next_toggle_us =
            now_us + clearcore_leap_locate_toggle_us(g_locate_pattern);
    }
}
