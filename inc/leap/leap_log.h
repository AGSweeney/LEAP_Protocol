/*
 * leap_log.h
 *
 * Optional security-event logging for field diagnostics. Define
 * LEAP_LOG_SECURITY at compile time to emit stderr lines; otherwise all
 * calls compile out.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LOG_H
#define LEAP_LOG_H

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

const char* leap_log_security_event_name(LeapLogSecurityEvent event);

#ifdef LEAP_LOG_SECURITY
#include <stdio.h>

#define leap_log_security(event, fmt, ...)                                \
    do                                                                    \
    {                                                                     \
        fprintf(                                                          \
            stderr,                                                       \
            "LEAP-SEC[%s]: " fmt "\n",                                    \
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
