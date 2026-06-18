/*
 * leap_log.h
 *
 * Timestamped logging and optional security-event diagnostics.
 * Define LEAP_LOG_SECURITY at compile time to emit security stderr lines.
 *
 * Timestamps are monotonic seconds since leap_log_reset_origin() (T+ format).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LOG_H
#define LEAP_LOG_H

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapLogSecurityEvent
{
    LEAP_LOG_SEC_FRAME_SEQ_DUPLICATE = 0,
    LEAP_LOG_SEC_FRAME_SEQ_GAP,
    LEAP_LOG_SEC_FRAME_SEQ_OUT_OF_WINDOW,
    LEAP_LOG_SEC_FRAME_SEQ_SESSION_MISMATCH,
    LEAP_LOG_SEC_PD_NOT_OWNER,
    LEAP_LOG_SEC_PD_STALE_FRAME
} LeapLogSecurityEvent;

typedef uint64_t (*LeapLogMonotonicUsFn)(void);

/*
 * Optional host sink (Qt Studio, embedded UART, etc.). When set, leap_log_*
 * delivers formatted lines here instead of stdout/stderr.
 */
typedef void (*LeapLogSinkFn)(void* ctx, int is_error, const char* line);

const char* leap_log_security_event_name(LeapLogSecurityEvent event);

/*
 * Reset the T+ origin (call once at program / session start).
 */
void leap_log_reset_origin(void);

/*
 * Optional override for monotonic clock (tests or embedded ports).
 * When NULL, uses platform default where available.
 */
void leap_log_set_monotonic_us_fn(LeapLogMonotonicUsFn fn);

void leap_log_set_stdout_enabled(int enabled);

void leap_log_set_sink(LeapLogSinkFn fn, void* ctx);

uint64_t leap_log_monotonic_us(void);

/*
 * Write "[+  sec.mmm s] " into buf. Returns characters written (excluding NUL).
 */
int leap_log_format_timestamp(char* buf, size_t buf_len);

int leap_log_format_mac(char* buf, size_t buf_len, const uint8_t* mac);

void leap_log_vfprintf(FILE* stream, const char* fmt, va_list args);

void leap_log_fprintf(FILE* stream, const char* fmt, ...);

void leap_log_printf(const char* fmt, ...);

void leap_log_eprintf(const char* fmt, ...);

#ifdef LEAP_LOG_SECURITY
#define leap_log_security(event, fmt, ...)                                \
    do                                                                    \
    {                                                                     \
        leap_log_eprintf(                                                 \
            "LEAP-SEC[%s]: " fmt,                                         \
            leap_log_security_event_name(event),                          \
            ##__VA_ARGS__);                                               \
    } while (0)
#else
#define leap_log_security(event, fmt, ...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LEAP_LOG_H */
