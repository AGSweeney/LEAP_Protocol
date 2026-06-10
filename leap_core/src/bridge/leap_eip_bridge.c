/*
 * leap_eip_bridge.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_eip_bridge.h"

#include "leap/leap_protocol.h"

#include <string.h>

static uint16_t
read_le16(const uint8_t* buf, uint16_t byte_off, uint8_t bit_off, uint8_t width_bits)
{
    uint16_t value = 0u;
    unsigned bit;

    if (width_bits == 0u || width_bits > 16u)
    {
        return 0u;
    }

    for (bit = 0u; bit < width_bits; ++bit)
    {
        unsigned abs_bit = (unsigned)bit_off + bit;
        unsigned byte_index = (unsigned)byte_off + (abs_bit / 8u);
        unsigned bit_index = abs_bit % 8u;
        uint8_t mask = (uint8_t)(1u << bit_index);

        if ((buf[byte_index] & mask) != 0u)
        {
            value = (uint16_t)(value | (uint16_t)(1u << bit));
        }
    }

    return value;
}

static void
write_le16(
    uint8_t* buf,
    size_t   buf_len,
    uint16_t byte_off,
    uint8_t  bit_off,
    uint8_t  width_bits,
    uint16_t value)
{
    unsigned bit;

    if (width_bits == 0u || width_bits > 16u)
    {
        return;
    }

    for (bit = 0u; bit < width_bits; ++bit)
    {
        unsigned abs_bit = (unsigned)bit_off + bit;
        unsigned byte_index = (unsigned)byte_off + (abs_bit / 8u);
        unsigned bit_index = abs_bit % 8u;
        uint8_t mask = (uint8_t)(1u << bit_index);

        if (byte_index >= buf_len)
        {
            continue;
        }

        if (((value >> bit) & 1u) != 0u)
        {
            buf[byte_index] = (uint8_t)(buf[byte_index] | mask);
        }
        else
        {
            buf[byte_index] = (uint8_t)(buf[byte_index] & (uint8_t)~mask);
        }
    }
}

static void
write_status_bytes(
    uint8_t* buf,
    size_t   buf_len,
    uint16_t byte_off,
    uint8_t  width_bytes,
    uint16_t status)
{
    unsigned i;

    for (i = 0u; i < width_bytes && i < 2u; ++i)
    {
        size_t index = (size_t)byte_off + i;

        if (index >= buf_len)
        {
            break;
        }
        buf[index] = (uint8_t)((status >> (8u * i)) & 0xFFu);
    }
}

void
leap_eip_bridge_init(LeapEipBridgeState* state)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->config.input_assembly_id = 100u;
    state->config.output_assembly_id = 150u;
    state->config.input_assembly_size = 32u;
    state->config.output_assembly_size = 32u;
}

void
leap_eip_bridge_set_config(
    LeapEipBridgeState*        state,
    const LeapEipBridgeConfig* config)
{
    if (state == NULL || config == NULL)
    {
        return;
    }

    state->config = *config;
    if (state->config.input_assembly_size > LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES)
    {
        state->config.input_assembly_size = LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES;
    }
    if (state->config.output_assembly_size > LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES)
    {
        state->config.output_assembly_size = LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES;
    }
    memset(state->input_assembly, 0, sizeof(state->input_assembly));
    memset(state->output_assembly, 0, sizeof(state->output_assembly));
    state->outputs_dirty = 0;
}

int
leap_eip_bridge_apply_output_assembly(
    LeapEipBridgeState* state,
    const uint8_t*      output_assembly,
    size_t              output_length)
{
    unsigned i;

    if (state == NULL || output_assembly == NULL)
    {
        return -1;
    }

    if (output_length > state->config.output_assembly_size)
    {
        output_length = state->config.output_assembly_size;
    }

    memcpy(state->output_assembly, output_assembly, output_length);

    for (i = 0u; i < state->config.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &state->config.mappings[i];

        if (!map->enabled)
        {
            continue;
        }

        state->peer_io[i].digital_outputs = read_le16(
            state->output_assembly,
            map->output.assembly_byte,
            map->output.bit,
            map->output.width_bits);
    }

    state->outputs_dirty = 1;
    return 0;
}

int
leap_eip_bridge_pack_input_assembly(
    const LeapEipBridgeState* state,
    uint8_t*                  input_assembly,
    size_t                    input_capacity,
    size_t*                   input_length_out)
{
    unsigned i;
    size_t   need;

    if (state == NULL || input_assembly == NULL || input_length_out == NULL)
    {
        return -1;
    }

    need = state->config.input_assembly_size;
    if (need > input_capacity || need > LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES)
    {
        return -1;
    }

    memcpy(input_assembly, state->input_assembly, need);

    for (i = 0u; i < state->config.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &state->config.mappings[i];
        const LeapEipBridgePeerIo*  peer = &state->peer_io[i];
        uint16_t status = peer->io_status;

        if (!map->enabled)
        {
            continue;
        }

        if (!peer->comm_ok)
        {
            status = (uint16_t)(status | LEAP_DIO_STATUS_FIELD_POWER_FAULT);
        }

        write_le16(
            input_assembly,
            need,
            map->input.assembly_byte,
            map->input.bit,
            map->input.width_bits,
            peer->digital_inputs);

        write_status_bytes(
            input_assembly,
            need,
            map->status_assembly_byte,
            map->status_width_bytes,
            status);
    }

    *input_length_out = need;
    return 0;
}

void
leap_eip_bridge_update_peer_io(
    LeapEipBridgeState* state,
    unsigned            mapping_index,
    uint16_t            digital_inputs,
    uint16_t            digital_outputs,
    uint16_t            io_status,
    int                 comm_ok)
{
    if (state == NULL || mapping_index >= LEAP_EIP_BRIDGE_MAX_MAPPINGS)
    {
        return;
    }

    state->peer_io[mapping_index].digital_inputs = digital_inputs;
    state->peer_io[mapping_index].digital_outputs = digital_outputs;
    state->peer_io[mapping_index].io_status = io_status;
    state->peer_io[mapping_index].comm_ok = comm_ok;
    state->leap_comm_ok = comm_ok;
}

int
leap_eip_bridge_peer_outputs(
    const LeapEipBridgeState* state,
    unsigned                  mapping_index,
    uint16_t*                 outputs_out)
{
    if (state == NULL || outputs_out == NULL ||
        mapping_index >= state->config.mapping_count)
    {
        return -1;
    }

    *outputs_out = state->peer_io[mapping_index].digital_outputs;
    return 0;
}
