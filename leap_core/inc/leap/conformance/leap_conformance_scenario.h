/*
 * leap_conformance_scenario.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_SCENARIO_H
#define LEAP_CONFORMANCE_SCENARIO_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_pd_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pass/fail threshold for io_exchange_bench wire RTT (microseconds). */
#define LEAP_CONF_IO_BENCH_MAX_RTT_US 5000u
/* Lifetime max is an outlier ceiling; p99 is the paced-run SLO gate. */
#define LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US 5000u
/* Freerun: paced max gate is too strict; use p99 plus an absolute ceiling. */
#define LEAP_CONF_IO_BENCH_P99_RTT_FREERUN_US 2000u
#define LEAP_CONF_IO_BENCH_MAX_RTT_FREERUN_US 5000u

static inline uint32_t leap_conf_io_bench_max_rtt_us(unsigned cyclic_period_ms)
{
    return cyclic_period_ms == 0u ? LEAP_CONF_IO_BENCH_MAX_RTT_FREERUN_US
                                  : LEAP_CONF_IO_BENCH_MAX_RTT_US;
}

static inline uint32_t leap_conf_io_bench_p99_rtt_us(unsigned cyclic_period_ms)
{
    return cyclic_period_ms == 0u ? LEAP_CONF_IO_BENCH_P99_RTT_FREERUN_US : 0u;
}

static inline int leap_conf_io_bench_wire_rtt_pass(
    const LeapPdControllerStats* stats,
    unsigned                     cyclic_period_ms)
{
    if (stats == NULL || stats->network_rtt_samples == 0u)
    {
        return 0;
    }

    if (cyclic_period_ms == 0u)
    {
        const uint32_t p99_us =
            leap_pd_stats_network_rtt_percentile_us(stats, 99u);

        return (p99_us <= LEAP_CONF_IO_BENCH_P99_RTT_FREERUN_US &&
                stats->max_network_rtt_us <=
                    LEAP_CONF_IO_BENCH_MAX_RTT_FREERUN_US) ?
                   1 :
                   0;
    }

    return (leap_pd_stats_network_rtt_percentile_us(stats, 99u) <=
                LEAP_CONF_IO_BENCH_MAX_RTT_US &&
            stats->max_network_rtt_us <=
                LEAP_CONF_IO_BENCH_MAX_RTT_CEILING_US) ?
               1 :
               0;
}

typedef enum LeapConformanceStepKind
{
    LEAP_CONF_KIND_PREFLIGHT = 0,
    LEAP_CONF_KIND_DISCOVER,
    LEAP_CONF_KIND_PROBE_CAPS,
    LEAP_CONF_KIND_BOOTSTRAP,
    LEAP_CONF_KIND_PD_WRITE,
    LEAP_CONF_KIND_DIAG_READ,
    LEAP_CONF_KIND_LEASE_DEMO,
    LEAP_CONF_KIND_CYCLIC_WRITE,
    LEAP_CONF_KIND_CYCLIC_EXCHANGE,
    LEAP_CONF_KIND_IO_EXCHANGE_BENCH,
    LEAP_CONF_KIND_PD_MASK_WALK,
    LEAP_CONF_KIND_IDENTIFY,
    LEAP_CONF_KIND_LOCATE
} LeapConformanceStepKind;

/* pd_outputs=0 on io_exchange_bench => rotating physical output exercise. */
#define LEAP_CONF_PD_OUTPUTS_ROTATE 0u

typedef struct LeapConformanceScenarioStep
{
    const char*              id;
    const char*              phase;
    const char*              name;
    LeapConformanceStepKind  kind;
    uint16_t                 pd_outputs;
    uint32_t                 cyclic_seconds;
    uint32_t                 locate_duration_ms;
} LeapConformanceScenarioStep;

typedef struct LeapConformanceScenario
{
    const char*                        id;
    const char*                        title;
    const LeapConformanceScenarioStep* steps;
    size_t                             step_count;
} LeapConformanceScenario;

const LeapConformanceScenario* leap_conformance_scenario_by_id(const char* id);

size_t leap_conformance_scenario_count(void);
const LeapConformanceScenario* leap_conformance_scenario_at(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_SCENARIO_H */
