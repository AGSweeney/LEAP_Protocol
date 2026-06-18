/*
 * gateway_leap_session_nb.cpp - NBRtos task wrapper for shared gateway_leap_session.c
 *
 * SPDX-License-Identifier: MIT
 */

#include <nbrtos.h>
#include <predef.h>

#include <cstdio>

#include "leap_time.h"

extern "C" void gateway_leap_session_loop(void);

static volatile int g_nb_leap_session_started = 0;

static void NbLeapSessionTask(void *pd)
{
    (void)pd;
    gateway_leap_session_loop();
}

extern "C" void nb_gateway_session_sleep_ms(unsigned ms)
{
    OSTimeDly((TICKS_PER_SECOND * static_cast<uint32_t>(ms)) / 1000U);
}

extern "C" int nb_gateway_leap_session_start_worker(void)
{
    if (g_nb_leap_session_started != 0)
    {
        return 0;
    }

    OSSimpleTaskCreatewName(NbLeapSessionTask, MAIN_PRIO - 3, "LEAP");
    g_nb_leap_session_started = 1;
    printf("%s Gateway: LEAP session task started\r\n", leap_rtems_uptime_str());
    return 0;
}
