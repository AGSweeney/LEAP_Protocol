/*
 * leap_conformance_scenario_digital_io.c
 *
 * Generic digital-I/O bench scenario. PD masks and step details are derived
 * at runtime from LEAP-DISC + LEAP-DIR device capabilities.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_scenario.h"

static const LeapConformanceScenarioStep k_digital_io_steps[] = {
    { "preflight",     "preflight",             "preflight",              LEAP_CONF_KIND_PREFLIGHT,      0u,     0u, 0u },
    { "discover",      "1. DISC HELLO",         "DISC HELLO",             LEAP_CONF_KIND_DISCOVER,       0u,     0u, 0u },
    { "probe_caps",    "1b. device caps",       "read device capabilities", LEAP_CONF_KIND_PROBE_CAPS,   0u,     0u, 0u },
    { "bootstrap_pd",  "2. bootstrap + PD",     "bootstrap + PD WRITE",   LEAP_CONF_KIND_BOOTSTRAP,      0u,     0u, 0u },
    { "diag",          "3. DIAG",               "DIAG readback",          LEAP_CONF_KIND_DIAG_READ,      0u,     0u, 0u },
    { "lease_demo",    "4. lease demo",         "MGMT lease demo",        LEAP_CONF_KIND_LEASE_DEMO,     0u,     0u, 0u },
    { "cyclic_write",  "5. cyclic PD WRITE",    "cyclic PD WRITE",        LEAP_CONF_KIND_CYCLIC_WRITE,   0u,     2u, 0u },
    { "cyclic_exch",   "6. cyclic PD EXCHANGE", "cyclic PD EXCHANGE",     LEAP_CONF_KIND_CYCLIC_EXCHANGE,  0u,   2u, 0u },
    { "pd_masks",      "7. PD masks",           "PD output masks",        LEAP_CONF_KIND_PD_MASK_WALK,   0u,     0u, 0u },
    { "identify",      "8. IDENTIFY",           "DISC IDENTIFY",          LEAP_CONF_KIND_IDENTIFY,       0u,     0u, 0u },
    { "locate",        "9. LOCATE_DEVICE",      "DISC LOCATE_DEVICE",     LEAP_CONF_KIND_LOCATE,         0u,     0u, 1500u },
};

const LeapConformanceScenario leap_conformance_scenario_digital_io_bench_v1 = {
    "device_conformance",
    "LEAP device conformance (DIR-reported capabilities)",
    k_digital_io_steps,
    sizeof(k_digital_io_steps) / sizeof(k_digital_io_steps[0]),
};
