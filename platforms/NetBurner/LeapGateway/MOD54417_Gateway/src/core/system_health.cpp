/*
 * LEAP Gateway (NetBurner MOD5441X) - lightweight system health counters.
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef LEAPGATEWAY_MAIN_TU

#include "core/system_health.h"

#include <constants.h>
#include <cstdio>
#include <cstring>

static volatile OS_TCB *g_health_leap_task = nullptr;
static volatile OS_TCB *g_health_opener_task = nullptr;
static GatewaySystemHealthSnapshot g_health_snapshot{};
static uint32_t g_health_last_tick = 0u;

#ifdef NBRTOS_TIME
static bool g_health_baseline_valid = false;
static uint32_t g_health_prev_ticks[OS_MAX_PRIOS]{};
#endif

static uint32_t HealthTenths(uint32_t part, uint32_t total)
{
    if (total == 0u)
    {
        return 0u;
    }
    return (part * 1000u + (total / 2u)) / total;
}

static const char *HealthTaskName(const volatile OS_TCB *task)
{
    if (task == nullptr)
    {
        return "unknown";
    }
    if (task->pOSTCBName != nullptr && task->pOSTCBName[0] != '\0')
    {
        return task->pOSTCBName;
    }

    switch (task->OSTCBPrio)
    {
    case OS_LO_PRIO:
        return "Idle";
    case MAIN_PRIO:
        return "Main";
    case HTTP_PRIO:
        return "HTTP";
    case CONFIG_SERVER_PRIO:
        return "Config";
    case ETHER_SEND_PRIO:
        return "EtherSend";
    default:
        return "User";
    }
}

static bool HealthIsNetworkTask(const volatile OS_TCB *task)
{
    if (task == nullptr)
    {
        return false;
    }

    return task->OSTCBPrio == ETHER_SEND_PRIO ||
           task->OSTCBPrio == HTTP_PRIO ||
           task->OSTCBPrio == CONFIG_SERVER_PRIO;
}

void GatewaySystemHealthRegisterLeapTask(OS_TCB *task)
{
    g_health_leap_task = task;
}

void GatewaySystemHealthRegisterOpenerTask(OS_TCB *task)
{
    g_health_opener_task = task;
}

void GatewaySystemHealthSample(void)
{
#ifdef NBRTOS_TIME
    GatewaySystemHealthSnapshot next{};
    uint32_t now = TimeTick;
    uint32_t nndk_total_ticks = 0u;
    uint32_t total_delta = 0u;
    uint32_t idle_delta = 0u;
    uint32_t leap_delta = 0u;
    uint32_t opener_delta = 0u;
    uint32_t network_delta = 0u;
    uint32_t max_delta = 0u;
    const char *max_name = "none";

    (void)GetCurrentTaskTime(&nndk_total_ticks);

    NBRTOS_ENTER_CRITICAL();
    for (unsigned i = 0u; i < OS_MAX_TASKS; ++i)
    {
        volatile OS_TCB *task = &OSTCBTbl[i];
        const uint32_t prio = task->OSTCBPrio;

        if (prio == 0u || prio >= OS_MAX_PRIOS)
        {
            continue;
        }

        const uint32_t current = task->runningTime;
        const uint32_t delta = g_health_baseline_valid
                                   ? (current - g_health_prev_ticks[prio])
                                   : 0u;
        g_health_prev_ticks[prio] = current;

        total_delta += delta;
        if (prio == OS_LO_PRIO)
        {
            idle_delta += delta;
        }
        else if (delta > max_delta)
        {
            max_delta = delta;
            max_name = HealthTaskName(task);
        }

        if (task == g_health_leap_task)
        {
            leap_delta += delta;
        }
        else if (task == g_health_opener_task)
        {
            opener_delta += delta;
        }
        else if (HealthIsNetworkTask(task))
        {
            network_delta += delta;
        }
    }
    NBRTOS_EXIT_CRITICAL();

    if (!g_health_baseline_valid)
    {
        g_health_baseline_valid = true;
        g_health_last_tick = now;
        snprintf(next.max_task_name, sizeof(next.max_task_name), "%s", "baseline");
        g_health_snapshot = next;
        return;
    }

    next.ready = (total_delta != 0u);
    next.sample_ms = ((now - g_health_last_tick) * 1000u) / TICKS_PER_SECOND;
    next.idle_tenths = HealthTenths(idle_delta, total_delta);
    next.cpu_load_tenths = next.ready ? (1000u - next.idle_tenths) : 0u;
    next.leap_tenths = HealthTenths(leap_delta, total_delta);
    next.eip_tenths = HealthTenths(opener_delta, total_delta);
    next.network_tenths = HealthTenths(network_delta, total_delta);

    const uint32_t known_delta = idle_delta + leap_delta + opener_delta + network_delta;
    next.other_tenths = HealthTenths(
        known_delta < total_delta ? total_delta - known_delta : 0u,
        total_delta);
    if (next.ready)
    {
        next.max_task_tenths = HealthTenths(max_delta, total_delta);
        next.max_task_ticks = max_delta;
        snprintf(next.max_task_name, sizeof(next.max_task_name), "%s", max_name);
    }
    else
    {
        snprintf(next.max_task_name, sizeof(next.max_task_name), "%s", "no delta");
    }

    g_health_last_tick = now;
    g_health_snapshot = next;
#else
    GatewaySystemHealthSnapshot next{};
    snprintf(next.max_task_name, sizeof(next.max_task_name), "%s", "NBRTOS_TIME off");
    g_health_snapshot = next;
#endif
}

GatewaySystemHealthSnapshot GatewaySystemHealthGetSnapshot(void)
{
    return g_health_snapshot;
}

#endif /* LEAPGATEWAY_MAIN_TU */
