/*
 * leap_pd_common.h
 *
 * Shared LEAP-PD endpoint packing, parsing, and profile helpers.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_PD_COMMON_H
#define LEAP_PD_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_dir_device.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_PD_COMMON_MAX_ENDPOINT_PAYLOAD 64u

typedef struct LeapPdEndpointView
{
    const LeapEndpointDataHeader* header;
    const uint8_t*                data;
    size_t                        data_length;
} LeapPdEndpointView;

typedef struct LeapPdExchangeView
{
    const LeapExchangeHeader* header;
    const uint8_t*            write_data;
    size_t                    write_length;
    const uint8_t*            read_reservation;
    size_t                    read_length;
} LeapPdExchangeView;

typedef struct LeapPdBuildParams
{
    uint16_t endpoint_id;
    uint32_t profile_id;
    uint32_t process_sequence;
    uint32_t cycle_time_us;
    uint64_t controller_timestamp_us;
    uint32_t max_frame_age_us;
    uint16_t endpoint_flags;
} LeapPdBuildParams;

/*
 * Resolved PD endpoint layout for the active profile (typically from LEAP-DIR).
 */
typedef struct LeapPdProfileMap
{
    uint32_t profile_id;
    uint16_t write_endpoint_id;
    uint16_t read_endpoint_id;
    size_t   endpoint_payload_size;
    int      valid;
} LeapPdProfileMap;

typedef enum LeapPdCommonStatus
{
    LEAP_PD_COMMON_OK = 0,
    LEAP_PD_COMMON_BAD_LENGTH,
    LEAP_PD_COMMON_PROFILE_MISMATCH,
    LEAP_PD_COMMON_BUFFER_TOO_SMALL,
    LEAP_PD_COMMON_SEQUENCE_MISMATCH,
    LEAP_PD_COMMON_ERROR
} LeapPdCommonStatus;

void leap_pd_profile_map_init_default(LeapPdProfileMap* out);

LeapPdCommonStatus leap_pd_profile_map_from_profile_id(
    uint32_t          profile_id,
    LeapPdProfileMap* out);

LeapPdCommonStatus leap_pd_profile_map_from_dir(
    const LeapDirDeviceContext* dir,
    LeapPdProfileMap*           out);

size_t leap_pd_endpoint_payload_size(uint32_t profile_id, uint16_t endpoint_id);

LeapPdCommonStatus leap_pd_endpoint_view(
    const uint8_t*     payload,
    size_t             payload_length,
    LeapPdEndpointView* view);

LeapPdCommonStatus leap_pd_exchange_view(
    const uint8_t*      payload,
    size_t              payload_length,
    LeapPdExchangeView* view);

size_t leap_pd_build_write_endpoint(
    uint8_t*                   out,
    size_t                     out_capacity,
    const LeapPdBuildParams*   params,
    const uint8_t*             endpoint_data,
    size_t                     endpoint_data_length);

size_t leap_pd_build_digital_write(
    uint8_t*                 out,
    size_t                   out_capacity,
    const LeapPdBuildParams* params,
    uint16_t                 digital_outputs);

size_t leap_pd_build_digital_exchange(
    uint8_t*                 out,
    size_t                   out_capacity,
    uint32_t                 process_sequence,
    uint32_t                 cycle_time_us,
    uint32_t                 profile_id,
    uint16_t                 digital_outputs);

size_t leap_pd_build_digital_exchange_mapped(
    uint8_t*                      out,
    size_t                        out_capacity,
    uint32_t                      process_sequence,
    uint32_t                      cycle_time_us,
    const LeapPdProfileMap*       profile,
    uint16_t                      digital_outputs);

size_t leap_pd_build_exchange_reply(
    uint8_t*                      out,
    size_t                        out_capacity,
    const LeapExchangeHeader*     request,
    const uint8_t*                write_echo,
    size_t                        write_length,
    const uint8_t*                read_data,
    size_t                        read_length,
    const LeapExchangeStatus*     status);

LeapPdCommonStatus leap_pd_unpack_digital16x16_outputs(
    const LeapPdEndpointView* view,
    uint16_t*                 digital_outputs);

LeapPdCommonStatus leap_pd_unpack_digital16x16_inputs(
    const LeapPdEndpointView* view,
    uint16_t*                 digital_inputs);

LeapPdCommonStatus leap_pd_profile_validate_write(
    const LeapPdProfileMap* profile,
    uint32_t                profile_id,
    uint16_t                endpoint_id,
    uint16_t                data_length);

LeapPdCommonStatus leap_pd_profile_validate_exchange(
    const LeapPdProfileMap* profile,
    uint32_t                profile_id,
    uint16_t                write_endpoint_id,
    uint16_t                read_endpoint_id,
    uint16_t                write_length,
    uint16_t                read_length);

/*
 * Parse and validate an EXCHANGE_REPLY: profile/endpoints/lengths plus optional
 * process_sequence match against the command the controller just sent.
 * Pass expected_process_sequence=0 to skip sequence check.
 */
LeapPdCommonStatus leap_pd_validate_exchange_reply(
    const uint8_t*            payload,
    size_t                    payload_length,
    const LeapPdProfileMap*   profile,
    uint32_t                  expected_process_sequence,
    LeapPdExchangeView*       view_out,
    LeapExchangeStatus*       status_out);

LeapPdCommonStatus leap_pd_pack_digital16x16(
    uint8_t*  out,
    size_t    out_capacity,
    uint16_t  digital_outputs,
    uint16_t  digital_inputs,
    uint16_t  io_status);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_PD_COMMON_H */
