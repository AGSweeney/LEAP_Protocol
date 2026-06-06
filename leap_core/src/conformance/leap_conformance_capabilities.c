/*
 * leap_conformance_capabilities.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_conformance_capabilities.h"

#include <stdio.h>
#include <string.h>

void leap_conformance_device_caps_init(LeapConformanceDeviceCaps* caps)
{
    if (caps == NULL)
    {
        return;
    }

    memset(caps, 0, sizeof(*caps));
    leap_dir_controller_capabilities_init(&caps->dir);
}

static void leap_conf_build_pd_masks(LeapConformanceDeviceCaps* caps)
{
    uint16_t bit;
    uint16_t count;

    if (caps == NULL || !caps->dir.has_digital_outputs ||
        caps->dir.output_bit_count == 0u)
    {
        return;
    }

    count = caps->dir.output_bit_count;
    if (count > LEAP_CONF_MAX_PD_MASKS)
    {
        count = (uint16_t)LEAP_CONF_MAX_PD_MASKS;
    }

    caps->pd_mask_count = 0u;
    for (bit = 0u; bit < count; bit++)
    {
        LeapConformancePdMaskStep* step = &caps->pd_masks[caps->pd_mask_count];

        step->mask = (uint16_t)(1u << bit);
        (void)snprintf(step->label, sizeof(step->label), "output ch %u", bit + 1u);
        caps->pd_mask_count++;
    }
}

static void leap_conf_build_detail_strings(LeapConformanceDeviceCaps* caps)
{
    if (caps == NULL)
    {
        return;
    }

    if (caps->dir.has_digital_inputs && caps->dir.input_bit_count > 0u)
    {
        (void)snprintf(
            caps->cyclic_exchange_detail,
            sizeof(caps->cyclic_exchange_detail),
            "PD EXCHANGE: %u digital input channel(s) from LEAP-DIR",
            caps->dir.input_bit_count);
        caps->skip_cyclic_exchange = 0;
    }
    else
    {
        (void)snprintf(
            caps->cyclic_exchange_detail,
            sizeof(caps->cyclic_exchange_detail),
            "no digital inputs endpoint in LEAP-DIR");
        caps->skip_cyclic_exchange = 1;
    }

    (void)snprintf(
        caps->identify_detail,
        sizeof(caps->identify_detail),
        "profile=0x%08X outputs=%u inputs=%u",
        caps->dir.active_profile_id != 0u ?
            caps->dir.active_profile_id : caps->dir.default_profile_id,
        caps->dir.output_bit_count,
        caps->dir.input_bit_count);

    if (caps->dir.has_locate)
    {
        (void)snprintf(
            caps->locate_detail,
            sizeof(caps->locate_detail),
            "locate flags=0x%04X",
            caps->dir.locate_capability_flags);
        caps->skip_locate = 0;
    }
    else
    {
        (void)snprintf(
            caps->locate_detail,
            sizeof(caps->locate_detail),
            "device reports no locate capability");
        caps->skip_locate = 1;
    }
}

void leap_conformance_device_caps_from_dir(
    const LeapDirControllerCapabilities* dir,
    LeapConformanceDeviceCaps*           out)
{
    if (dir == NULL || out == NULL)
    {
        return;
    }

    leap_conformance_device_caps_init(out);
    out->dir = *dir;
    leap_conf_build_pd_masks(out);
    leap_conf_build_detail_strings(out);
    out->bootstrap_outputs = leap_conformance_default_bootstrap_outputs(out);
    out->cyclic_outputs    = leap_conformance_default_cyclic_outputs(out);
    out->valid             = dir->valid;
}

uint16_t leap_conformance_default_bootstrap_outputs(
    const LeapConformanceDeviceCaps* caps)
{
    if (caps == NULL || !caps->dir.has_digital_outputs ||
        caps->dir.output_bit_count == 0u)
    {
        return 0x0001u;
    }

    return 0x0001u;
}

uint16_t leap_conformance_default_cyclic_outputs(
    const LeapConformanceDeviceCaps* caps)
{
    uint16_t mask;
    uint16_t bits;

    if (caps == NULL || !caps->dir.has_digital_outputs ||
        caps->dir.output_bit_count == 0u)
    {
        return 0x0007u;
    }

    bits = caps->dir.output_bit_count;
    if (bits > 3u)
    {
        bits = 3u;
    }

    mask = (uint16_t)((1u << bits) - 1u);
    return mask != 0u ? mask : 0x0001u;
}
