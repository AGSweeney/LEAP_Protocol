/*
 * leap_conformance_scenario_glc618wl.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_scenario.h"

#include <string.h>

static const LeapConformanceScenarioStep k_glc618wl_steps[] = {
    { "preflight",     "preflight",           "preflight",              LEAP_CONF_KIND_PREFLIGHT,      0u,     0u, 0u },
    { "discover",      "1. DISC HELLO",       "DISC HELLO",             LEAP_CONF_KIND_DISCOVER,       0u,     0u, 0u },
    { "bootstrap_pd",  "2. bootstrap + PD",   "bootstrap + PD WRITE",   LEAP_CONF_KIND_BOOTSTRAP,      0x0001u, 0u, 0u },
    { "diag",          "3. DIAG",             "DIAG readback",          LEAP_CONF_KIND_DIAG_READ,      0u,     0u, 0u },
    { "lease_demo",    "4. lease demo",       "MGMT lease demo",        LEAP_CONF_KIND_LEASE_DEMO,     0u,     0u, 0u },
    { "cyclic_write",  "5. cyclic PD WRITE",  "cyclic PD WRITE",        LEAP_CONF_KIND_CYCLIC_WRITE,   0x0007u, 2u, 0u },
    { "cyclic_exch",   "6. cyclic PD EXCHANGE", "cyclic PD EXCHANGE",   LEAP_CONF_KIND_CYCLIC_EXCHANGE,  0x0007u, 2u, 0u },
    { "pd_masks",      "7. PD masks",         "PD output masks",        LEAP_CONF_KIND_PD_MASK_WALK,   0u,     0u, 0u },
    { "identify",      "8. IDENTIFY",         "DISC IDENTIFY",          LEAP_CONF_KIND_IDENTIFY,       0u,     0u, 0u },
    { "locate",        "9. LOCATE_DEVICE",    "DISC LOCATE_DEVICE",     LEAP_CONF_KIND_LOCATE,         0u,     0u, 1500u },
};

static const LeapConformanceScenario k_glc618wl_bench_v1 = {
    "glc618wl_bench_v1",
    "GL-C-618WL bench conformance v1",
    k_glc618wl_steps,
    sizeof(k_glc618wl_steps) / sizeof(k_glc618wl_steps[0]),
};

static const LeapConformanceScenario* k_builtin_scenarios[] = {
    &k_glc618wl_bench_v1,
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
