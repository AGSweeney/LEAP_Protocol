/*
 * leap_win_time.h
 *
 * High-resolution monotonic clock and sleep for Windows.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_WIN_TIME_H
#define LEAP_WIN_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)

/*
 * QueryPerformanceCounter-based monotonic time in microseconds.
 * Falls back to GetTickCount64 when QPC is unavailable.
 */
uint64_t leap_win_monotonic_us(void);

/*
 * Sleep for at least sleep_us microseconds.
 * Uses CREATE_WAITABLE_TIMER_HIGH_RESOLUTION when available, otherwise
 * timeBeginPeriod(1) + coarse Sleep + QPC spin for the remainder.
 */
void leap_win_sleep_us(uint64_t sleep_us);

#endif /* _WIN32 */

#ifdef __cplusplus
}
#endif

#endif /* LEAP_WIN_TIME_H */
