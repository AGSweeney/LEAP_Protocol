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

/*
 * Raise the current thread to THREAD_PRIORITY_TIME_CRITICAL and pin it to one
 * CPU for cyclic PD / WinPcap recv loops. Restores previous settings in
 * leap_win_thread_priority_end.
 *
 * Affinity defaults to the highest-index CPU in the process mask. Override with
 * LEAP_WIN_CYCLIC_CPU=<n> (0-based core index).
 */
typedef struct LeapWinThreadPriorityScope
{
    int      active;
    int      previous_priority;
    int      affinity_active;
    uint64_t affinity_mask;
    uint64_t previous_affinity_mask;
} LeapWinThreadPriorityScope;

void leap_win_thread_priority_begin_critical(LeapWinThreadPriorityScope* scope);
void leap_win_thread_priority_end(LeapWinThreadPriorityScope* scope);

#endif /* _WIN32 */

#ifdef __cplusplus
}
#endif

#endif /* LEAP_WIN_TIME_H */
