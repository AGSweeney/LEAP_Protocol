/*
 * leap_conformance_scenarios.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_scenario.h"

#include <string.h>

extern const LeapConformanceScenario leap_conformance_scenario_digital_io_bench_v1;

static const LeapConformanceScenario* k_builtin_scenarios[] = {
    &leap_conformance_scenario_digital_io_bench_v1,
};

const LeapConformanceScenario* leap_conformance_scenario_by_id(const char* id)
{
    size_t i;

    if (id == NULL)
    {
        return NULL;
    }

    for (i = 0u; i < leap_conformance_scenario_count(); i++)
    {
        const LeapConformanceScenario* scenario = leap_conformance_scenario_at(i);
        if (scenario != NULL && scenario->id != NULL &&
            strcmp(scenario->id, id) == 0)
        {
            return scenario;
        }
    }

    /* Legacy scenario IDs map to the device capability plan. */
    if (strcmp(id, "digital_io_bench_v1") == 0 ||
        strcmp(id, "glc618wl_bench_v1") == 0 ||
        strcmp(id, "kc868_a16_bench_v1") == 0)
    {
        return &leap_conformance_scenario_digital_io_bench_v1;
    }

    return NULL;
}

size_t leap_conformance_scenario_count(void)
{
    return sizeof(k_builtin_scenarios) / sizeof(k_builtin_scenarios[0]);
}

const LeapConformanceScenario* leap_conformance_scenario_at(size_t index)
{
    if (index >= leap_conformance_scenario_count())
    {
        return NULL;
    }

    return k_builtin_scenarios[index];
}
