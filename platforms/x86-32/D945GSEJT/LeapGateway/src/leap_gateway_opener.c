/*
 * leap_gateway_opener.c — OpENer assembly bridge hooks for LeapOS-Gateway.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_global.h"

#include "leap/leap_eip_bridge.h"

#include <stddef.h>
#include <stdint.h>

void
leap_gateway_eip_apply_output_assembly(const uint8_t* data, size_t length)
{
    (void)leap_eip_bridge_apply_output_assembly(&g_gateway.bridge, data, length);
}

void
leap_gateway_eip_pack_input_assembly(
    uint8_t* data,
    size_t   capacity,
    size_t*  length)
{
    (void)leap_eip_bridge_pack_input_assembly(&g_gateway.bridge, data, capacity, length);
}
