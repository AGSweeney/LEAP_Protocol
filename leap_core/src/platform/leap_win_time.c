/*
 * leap_win_time.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_win_time.h"

#include <stdlib.h>

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#define LEAP_WIN_SPIN_THRESHOLD_US 50u
#define LEAP_WIN_COARSE_MARGIN_US  1500u

static LARGE_INTEGER g_qpc_frequency;
static volatile LONG g_qpc_frequency_ready;
static HANDLE        g_hires_timer;
static INIT_ONCE     g_win_time_init_once = INIT_ONCE_STATIC_INIT;
static volatile LONG g_hires_timer_ready;

static BOOL WINAPI leap_win_time_init(
    PINIT_ONCE init_once,
    PVOID      parameter,
    PVOID*     context)
{
    LARGE_INTEGER frequency;
    HANDLE        timer;

    (void)init_once;
    (void)parameter;
    (void)context;

    if (QueryPerformanceFrequency(&frequency) != 0)
    {
        g_qpc_frequency = frequency;
        InterlockedExchange(&g_qpc_frequency_ready, 1L);
    }

    timer = CreateWaitableTimerExW(
        NULL,
        NULL,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
    if (timer != NULL)
    {
        g_hires_timer = timer;
        InterlockedExchange(&g_hires_timer_ready, 1L);
    }

    (void)timeBeginPeriod(1);
    return TRUE;
}

static void leap_win_time_ensure_init(void)
{
    (void)InitOnceExecuteOnce(
        &g_win_time_init_once,
        leap_win_time_init,
        NULL,
        NULL);
}

uint64_t leap_win_monotonic_us(void)
{
    LARGE_INTEGER counter;

    leap_win_time_ensure_init();

    if (InterlockedCompareExchange(&g_qpc_frequency_ready, 0L, 0L) == 0 ||
        QueryPerformanceCounter(&counter) == 0)
    {
        return (uint64_t)(GetTickCount64() * 1000ULL);
    }

    return (uint64_t)((counter.QuadPart * 1000000LL) / g_qpc_frequency.QuadPart);
}

static void leap_win_sleep_us_spin_until(uint64_t deadline_us)
{
    while (leap_win_monotonic_us() < deadline_us)
    {
        SwitchToThread();
    }
}

static int leap_win_sleep_us_timer(uint64_t sleep_us)
{
    LARGE_INTEGER due;

    if (InterlockedCompareExchange(&g_hires_timer_ready, 0L, 0L) == 0 ||
        g_hires_timer == NULL ||
        sleep_us < LEAP_WIN_SPIN_THRESHOLD_US)
    {
        return 0;
    }

    due.QuadPart = -(LONGLONG)((int64_t)sleep_us * 10LL);
    if (SetWaitableTimer(g_hires_timer, &due, 0, NULL, NULL, FALSE) == 0)
    {
        return 0;
    }

    (void)WaitForSingleObject(g_hires_timer, INFINITE);
    return 1;
}

static DWORD_PTR leap_win_pick_cyclic_affinity_mask(void)
{
    char        env_buf[16];
    DWORD_PTR   process_mask = 0;
    DWORD_PTR   system_mask  = 0;
    unsigned    cpu;
    DWORD       env_len;

    env_len = GetEnvironmentVariableA(
        "LEAP_WIN_CYCLIC_CPU",
        env_buf,
        (DWORD)sizeof(env_buf));
    if (env_len > 0u && env_len < (DWORD)sizeof(env_buf))
    {
        cpu = (unsigned)atoi(env_buf);
        if (cpu < (unsigned)(sizeof(DWORD_PTR) * 8u))
        {
            return ((DWORD_PTR)1) << cpu;
        }
    }

    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) == 0 ||
        process_mask == 0)
    {
        return (DWORD_PTR)1;
    }

    for (cpu = (unsigned)(sizeof(DWORD_PTR) * 8u); cpu-- > 0u; )
    {
        DWORD_PTR bit = ((DWORD_PTR)1) << cpu;

        if ((process_mask & bit) != 0)
        {
            return bit;
        }
    }

    return process_mask & (~process_mask + 1);
}

void leap_win_thread_priority_begin_critical(LeapWinThreadPriorityScope* scope)
{
    DWORD_PTR affinity_mask;

    if (scope == NULL)
    {
        return;
    }

    scope->active                   = 0;
    scope->affinity_active          = 0;
    scope->affinity_mask            = 0u;
    scope->previous_affinity_mask   = 0u;
    scope->previous_priority          = GetThreadPriority(GetCurrentThread());
    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) != 0)
    {
        scope->active = 1;
    }

    affinity_mask = leap_win_pick_cyclic_affinity_mask();
    if (affinity_mask != 0)
    {
        DWORD_PTR previous_mask =
            SetThreadAffinityMask(GetCurrentThread(), affinity_mask);

        if (previous_mask != 0)
        {
            scope->affinity_active        = 1;
            scope->affinity_mask            = (uint64_t)affinity_mask;
            scope->previous_affinity_mask   = (uint64_t)previous_mask;
        }
    }
}

void leap_win_thread_priority_end(LeapWinThreadPriorityScope* scope)
{
    if (scope == NULL)
    {
        return;
    }

    if (scope->affinity_active != 0)
    {
        (void)SetThreadAffinityMask(
            GetCurrentThread(),
            (DWORD_PTR)scope->previous_affinity_mask);
        scope->affinity_active = 0;
    }

    if (scope->active != 0)
    {
        (void)SetThreadPriority(GetCurrentThread(), scope->previous_priority);
        scope->active = 0;
    }
}

void leap_win_sleep_us(uint64_t sleep_us)
{
    uint64_t deadline_us;
    uint64_t coarse_us;

    if (sleep_us == 0u)
    {
        return;
    }

    leap_win_time_ensure_init();

    if (leap_win_sleep_us_timer(sleep_us) != 0)
    {
        return;
    }

    deadline_us = leap_win_monotonic_us() + sleep_us;

    if (sleep_us > LEAP_WIN_COARSE_MARGIN_US)
    {
        coarse_us = sleep_us - LEAP_WIN_COARSE_MARGIN_US;
        Sleep((DWORD)(coarse_us / 1000u));
    }

    leap_win_sleep_us_spin_until(deadline_us);
}

#endif /* _WIN32 */
