/*
 * leap_conformance_result.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_RESULT_H
#define LEAP_CONFORMANCE_RESULT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_CONF_MAX_STEPS       32u
#define LEAP_CONF_DETAIL_MAX      256u
#define LEAP_CONF_PHASE_MAX       64u
#define LEAP_CONF_NAME_MAX        96u
#define LEAP_CONF_PCAP_PATH_MAX   260u

typedef enum LeapConformanceStepStatus
{
    LEAP_CONF_STEP_PENDING = 0,
    LEAP_CONF_STEP_RUNNING,
    LEAP_CONF_STEP_PASS,
    LEAP_CONF_STEP_FAIL,
    LEAP_CONF_STEP_SKIP
} LeapConformanceStepStatus;

typedef struct LeapConformanceStepResult
{
    char                       step_id[LEAP_CONF_NAME_MAX];
    char                       phase[LEAP_CONF_PHASE_MAX];
    char                       name[LEAP_CONF_NAME_MAX];
    LeapConformanceStepStatus  status;
    uint32_t                   duration_ms;
    char                       detail[LEAP_CONF_DETAIL_MAX];
} LeapConformanceStepResult;

typedef struct LeapConformanceRunSummary
{
    unsigned passed;
    unsigned failed;
    unsigned skipped;
    unsigned total;
    uint32_t elapsed_ms;
    char     adapter[LEAP_CONF_DETAIL_MAX];
    char     peer_mac[32];
    char     scenario_id[LEAP_CONF_NAME_MAX];
    char     pcap_path[LEAP_CONF_PCAP_PATH_MAX];
} LeapConformanceRunSummary;

typedef struct LeapConformanceRunResult
{
    LeapConformanceStepResult steps[LEAP_CONF_MAX_STEPS];
    size_t                    step_count;
    LeapConformanceRunSummary summary;
} LeapConformanceRunResult;

const char* leap_conformance_step_status_text(LeapConformanceStepStatus status);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_RESULT_H */
