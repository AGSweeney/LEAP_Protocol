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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LeapConformanceStepKind
{
    LEAP_CONF_KIND_PREFLIGHT = 0,
    LEAP_CONF_KIND_DISCOVER,
    LEAP_CONF_KIND_BOOTSTRAP,
    LEAP_CONF_KIND_PD_WRITE,
    LEAP_CONF_KIND_DIAG_READ,
    LEAP_CONF_KIND_LEASE_DEMO,
    LEAP_CONF_KIND_CYCLIC_WRITE,
    LEAP_CONF_KIND_CYCLIC_EXCHANGE,
    LEAP_CONF_KIND_PD_MASK_WALK,
    LEAP_CONF_KIND_IDENTIFY,
    LEAP_CONF_KIND_LOCATE
} LeapConformanceStepKind;

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
    const char*                    id;
    const char*                    title;
    const LeapConformanceScenarioStep* steps;
    size_t                         step_count;
} LeapConformanceScenario;

const LeapConformanceScenario* leap_conformance_scenario_by_id(const char* id);

size_t leap_conformance_scenario_count(void);
const LeapConformanceScenario* leap_conformance_scenario_at(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_SCENARIO_H */
