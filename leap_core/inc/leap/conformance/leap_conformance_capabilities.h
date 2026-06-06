/*
 * leap_conformance_capabilities.h
 *
 * Spec-driven conformance capability model and PD mask generation.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_CAPABILITIES_H
#define LEAP_CONFORMANCE_CAPABILITIES_H

#include <stddef.h>
#include <stdint.h>

#include "leap/conformance/leap_conformance_result.h"
#include "leap/leap_dir_controller_capabilities.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_CONF_MAX_PD_MASKS 32u

typedef struct LeapConformancePdMaskStep
{
    uint16_t mask;
    char     label[32];
} LeapConformancePdMaskStep;

typedef struct LeapConformanceDeviceCaps
{
    LeapDirControllerCapabilities dir;
    LeapConformancePdMaskStep     pd_masks[LEAP_CONF_MAX_PD_MASKS];
    size_t                        pd_mask_count;
    char                          cyclic_exchange_detail[LEAP_CONF_DETAIL_MAX];
    char                          identify_detail[LEAP_CONF_DETAIL_MAX];
    char                          locate_detail[LEAP_CONF_DETAIL_MAX];
    char                          probe_detail[LEAP_CONF_DETAIL_MAX];
    int                           skip_locate;
    int                           skip_cyclic_exchange;
    uint16_t                      bootstrap_outputs;
    uint16_t                      cyclic_outputs;
    int                           valid;
} LeapConformanceDeviceCaps;

void leap_conformance_device_caps_init(LeapConformanceDeviceCaps* caps);

void leap_conformance_device_caps_from_dir(
    const LeapDirControllerCapabilities* dir,
    LeapConformanceDeviceCaps*           out);

uint16_t leap_conformance_default_bootstrap_outputs(
    const LeapConformanceDeviceCaps* caps);

uint16_t leap_conformance_default_cyclic_outputs(
    const LeapConformanceDeviceCaps* caps);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_CAPABILITIES_H */
