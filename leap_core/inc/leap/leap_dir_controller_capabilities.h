/*
 * leap_dir_controller_capabilities.h
 *
 * Controller-side device capability model from LEAP-DISC and LEAP-DIR.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_DIR_CONTROLLER_CAPABILITIES_H
#define LEAP_DIR_CONTROLLER_CAPABILITIES_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_dir_controller.h"
#include "leap/leap_pd_common.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_DIR_CTRL_MAX_ENDPOINTS 8u

typedef struct LeapDirControllerCapabilities
{
    LeapIdentity           identity;
    uint32_t               default_profile_id;
    uint32_t               active_profile_id;
    uint16_t               current_state;
    uint16_t               locate_capability_flags;
    uint16_t               supported_services[16];
    size_t                 supported_service_count;

    LeapProfileDescriptor  profile;
    int                    has_profile_descriptor;
    LeapEndpointDescriptor endpoints[LEAP_DIR_CTRL_MAX_ENDPOINTS];
    size_t                 endpoint_count;

    uint16_t               output_byte_length;
    uint16_t               input_byte_length;
    uint16_t               output_bit_count;
    uint16_t               input_bit_count;
    int                    has_digital_outputs;
    int                    has_digital_inputs;
    int                    has_locate;
    LeapPdProfileMap       pd_map;
    int                    valid;
} LeapDirControllerCapabilities;

void leap_dir_controller_capabilities_init(LeapDirControllerCapabilities* caps);

LeapDirControllerStatus leap_dir_controller_on_profile_reply_full(
    const uint8_t*               payload,
    size_t                         payload_length,
    LeapDirControllerCapabilities* caps);

LeapDirControllerStatus leap_dir_controller_parse_directory_tlvs(
    const uint8_t*               tlv_data,
    size_t                         tlv_length,
    LeapDirControllerCapabilities* caps);

LeapDirControllerStatus leap_dir_controller_parse_profile_object(
    const uint8_t*               object_bytes,
    size_t                         object_length,
    LeapDirControllerCapabilities* caps);

void leap_dir_controller_capabilities_from_identify(
    const LeapIdentifyReply*       identify,
    const uint8_t*                 payload,
    size_t                           payload_length,
    LeapDirControllerCapabilities* caps);

void leap_dir_controller_capabilities_finalize(
    LeapDirControllerCapabilities* caps);

uint16_t leap_dir_profile_nominal_io_bits(uint32_t profile_id);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_DIR_CONTROLLER_CAPABILITIES_H */
