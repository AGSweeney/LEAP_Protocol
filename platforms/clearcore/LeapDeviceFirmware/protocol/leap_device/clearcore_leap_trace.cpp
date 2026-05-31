/*
 * clearcore_leap_trace.cpp
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "clearcore_leap_trace.h"

#include "ClearCore.h"

#include <string.h>

#define CLEARCORE_LEAP_TRACE_DEPTH 4u
#define CLEARCORE_LEAP_TRACE_LINE  96u

static char     g_trace_lines[CLEARCORE_LEAP_TRACE_DEPTH][CLEARCORE_LEAP_TRACE_LINE];
static uint8_t  g_trace_head;
static uint8_t  g_trace_tail;
static uint8_t  g_trace_count;

extern "C" void clearcore_leap_trace_queue(const char *line)
{
    if (line == NULL || g_trace_count >= CLEARCORE_LEAP_TRACE_DEPTH)
    {
        return;
    }

    (void)strncpy(
        g_trace_lines[g_trace_head],
        line,
        CLEARCORE_LEAP_TRACE_LINE - 1u);
    g_trace_lines[g_trace_head][CLEARCORE_LEAP_TRACE_LINE - 1u] = '\0';

    g_trace_head = (uint8_t)((g_trace_head + 1u) % CLEARCORE_LEAP_TRACE_DEPTH);
    ++g_trace_count;
}

extern "C" void clearcore_leap_trace_flush(void)
{
    while (g_trace_count > 0u)
    {
        ConnectorUsb.SendLine(g_trace_lines[g_trace_tail]);
        g_trace_tail = (uint8_t)((g_trace_tail + 1u) % CLEARCORE_LEAP_TRACE_DEPTH);
        --g_trace_count;
    }
}
