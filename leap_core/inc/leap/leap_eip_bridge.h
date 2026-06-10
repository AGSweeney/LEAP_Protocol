/*
 * leap_eip_bridge.h — E/IP assembly ↔ LEAP digital I/O bridge image.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_EIP_BRIDGE_H
#define LEAP_EIP_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES 64u
#define LEAP_EIP_BRIDGE_MAX_MAPPINGS       16u

typedef struct LeapEipBridgeFieldMap
{
    uint16_t assembly_byte;
    uint8_t  bit;
    uint8_t  width_bits;
} LeapEipBridgeFieldMap;

typedef struct LeapEipBridgeMapping
{
    uint8_t  leap_mac[6];
    uint32_t profile_id;
    LeapEipBridgeFieldMap input;
    LeapEipBridgeFieldMap output;
    uint16_t status_assembly_byte;
    uint8_t  status_width_bytes;
    int      enabled;
} LeapEipBridgeMapping;

typedef struct LeapEipBridgeConfig
{
    uint16_t input_assembly_id;
    uint16_t output_assembly_id;
    size_t   input_assembly_size;
    size_t   output_assembly_size;
    unsigned mapping_count;
    LeapEipBridgeMapping mappings[LEAP_EIP_BRIDGE_MAX_MAPPINGS];
} LeapEipBridgeConfig;

typedef struct LeapEipBridgePeerIo
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t io_status;
    int      comm_ok;
} LeapEipBridgePeerIo;

typedef struct LeapEipBridgeState
{
    LeapEipBridgeConfig config;
    uint8_t             input_assembly[LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES];
    uint8_t             output_assembly[LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES];
    LeapEipBridgePeerIo peer_io[LEAP_EIP_BRIDGE_MAX_MAPPINGS];
    int                 outputs_dirty;
    int                 leap_comm_ok;
} LeapEipBridgeState;

void leap_eip_bridge_init(LeapEipBridgeState* state);
void leap_eip_bridge_set_config(
    LeapEipBridgeState*        state,
    const LeapEipBridgeConfig* config);

int leap_eip_bridge_apply_output_assembly(
    LeapEipBridgeState* state,
    const uint8_t*      output_assembly,
    size_t              output_length);

int leap_eip_bridge_pack_input_assembly(
    const LeapEipBridgeState* state,
    uint8_t*                  input_assembly,
    size_t                    input_capacity,
    size_t*                   input_length_out);

void leap_eip_bridge_update_peer_io(
    LeapEipBridgeState* state,
    unsigned            mapping_index,
    uint16_t            digital_inputs,
    uint16_t            digital_outputs,
    uint16_t            io_status,
    int                 comm_ok);

int leap_eip_bridge_peer_outputs(
    const LeapEipBridgeState* state,
    unsigned                  mapping_index,
    uint16_t*                 outputs_out);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_EIP_BRIDGE_H */
