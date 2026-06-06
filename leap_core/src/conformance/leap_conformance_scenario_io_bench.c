/*
 * leap_conformance_scenario_io_bench.c
 *
 * Sustained PD EXCHANGE soak for wire RTT and I/O reliability measurement.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_scenario.h"

static const LeapConformanceScenarioStep k_io_exchange_bench_steps[] = {
    /*
     * pd_outputs=0: I/O bench engine passes outputs=0 → rotating one-hot on
     * IO-0..IO-5 each EXCHANGE cycle (physical exercise + wire soak).
     */
    { "io_exchange_bench", "I/O bench", "PD EXCHANGE soak", LEAP_CONF_KIND_IO_EXCHANGE_BENCH, 0u, 10u, 0u },
};

const LeapConformanceScenario leap_conformance_scenario_io_exchange_bench_v1 = {
    "io_exchange_bench",
    "I/O EXCHANGE performance bench (wire RTT + reliability)",
    k_io_exchange_bench_steps,
    sizeof(k_io_exchange_bench_steps) / sizeof(k_io_exchange_bench_steps[0]),
};
