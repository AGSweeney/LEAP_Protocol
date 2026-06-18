/*
 * leap_log.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_WIN32)
#include "leap/leap_win_time.h"
#elif defined(__linux__)
#include "leap/leap_raw_linux.h"
#endif

static LeapLogMonotonicUsFn g_log_monotonic_us_fn = NULL;
static LeapLogSinkFn        g_log_sink_fn         = NULL;
static void*                g_log_sink_ctx        = NULL;
static uint64_t             g_log_origin_us       = 0u;
static int                  g_log_origin_valid    = 0;
static int                  g_log_stdout_enabled  = 1;

const char* leap_log_security_event_name(LeapLogSecurityEvent event)
{
    switch (event)
    {
    case LEAP_LOG_SEC_FRAME_SEQ_DUPLICATE:
        return "frame_seq_duplicate";
    case LEAP_LOG_SEC_FRAME_SEQ_GAP:
        return "frame_seq_gap";
    case LEAP_LOG_SEC_FRAME_SEQ_OUT_OF_WINDOW:
        return "frame_seq_out_of_window";
    case LEAP_LOG_SEC_FRAME_SEQ_SESSION_MISMATCH:
        return "frame_seq_session_mismatch";
    case LEAP_LOG_SEC_PD_NOT_OWNER:
        return "pd_not_owner";
    case LEAP_LOG_SEC_PD_STALE_FRAME:
        return "pd_stale_frame";
    default:
        return "unknown";
    }
}

void leap_log_reset_origin(void)
{
    g_log_origin_us    = leap_log_monotonic_us();
    g_log_origin_valid = 1;
}

void leap_log_set_monotonic_us_fn(LeapLogMonotonicUsFn fn)
{
    g_log_monotonic_us_fn = fn;
}

void leap_log_set_stdout_enabled(int enabled)
{
    g_log_stdout_enabled = (enabled != 0) ? 1 : 0;
}

void leap_log_set_sink(LeapLogSinkFn fn, void* ctx)
{
    g_log_sink_fn  = fn;
    g_log_sink_ctx = ctx;
}

uint64_t leap_log_monotonic_us(void)
{
    if (g_log_monotonic_us_fn != NULL)
    {
        return g_log_monotonic_us_fn();
    }

#if defined(_WIN32)
    return leap_win_monotonic_us();
#elif defined(__linux__)
    return leap_raw_linux_monotonic_us();
#else
    return 0u;
#endif
}

int leap_log_format_timestamp(char* buf, size_t buf_len)
{
    uint64_t now_us;
    uint64_t delta_us;
    unsigned sec;
    unsigned ms;

    if (buf == NULL || buf_len == 0u)
    {
        return 0;
    }

    if (g_log_origin_valid == 0)
    {
        leap_log_reset_origin();
    }

    now_us = leap_log_monotonic_us();
    if (now_us == 0u || g_log_origin_valid == 0)
    {
        return snprintf(buf, buf_len, "[       n/a] ");
    }

    if (now_us >= g_log_origin_us)
    {
        delta_us = now_us - g_log_origin_us;
    }
    else
    {
        delta_us = 0u;
    }

    sec = (unsigned)(delta_us / 1000000u);
    ms  = (unsigned)((delta_us / 1000u) % 1000u);
    return snprintf(buf, buf_len, "[+%7u.%03us] ", sec, ms);
}

int leap_log_format_mac(char* buf, size_t buf_len, const uint8_t* mac)
{
    if (buf == NULL || buf_len == 0u)
    {
        return 0;
    }

    if (mac == NULL)
    {
        return snprintf(buf, buf_len, "(null)");
    }

    return snprintf(
        buf,
        buf_len,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

void leap_log_vfprintf(FILE* stream, const char* fmt, va_list args)
{
    char   ts[24];
    char   line[768];
    size_t offset = 0u;
    int    written;

    if (stream == NULL)
    {
        return;
    }

    if (stream == stdout && g_log_stdout_enabled == 0)
    {
        return;
    }

    written = leap_log_format_timestamp(ts, sizeof(ts));
    if (written > 0)
    {
        offset = (size_t)written;
        (void)snprintf(line, sizeof(line), "%s", ts);
    }

    if (fmt != NULL && offset < sizeof(line))
    {
        (void)vsnprintf(line + offset, sizeof(line) - offset, fmt, args);
    }

    if (g_log_sink_fn != NULL)
    {
        g_log_sink_fn(g_log_sink_ctx, (stream == stderr) ? 1 : 0, line);
        return;
    }

    fputs(line, stream);
}

void leap_log_fprintf(FILE* stream, const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    leap_log_vfprintf(stream, fmt, args);
    va_end(args);
}

void leap_log_printf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    leap_log_vfprintf(stdout, fmt, args);
    va_end(args);
    fflush(stdout);
}

void leap_log_eprintf(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    leap_log_vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);
}
