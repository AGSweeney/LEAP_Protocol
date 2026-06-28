/*
 * LEAP Gateway EtherNet/IP assembly bridge hooks.
 *
 * Must live in its own translation unit. Do not use weak stubs in leapgateway.c
 * and call them from the same file — GCC will keep the local weak definitions.
 */

#include "leap_gateway_eip_conf.h"

#include "gateway_global.h"
#include "leap/leap_eip_bridge.h"

#include <stddef.h>
#include <stdint.h>

extern void leap_gateway_runtime_lock(void);
extern void leap_gateway_runtime_unlock(void);

void
leap_gateway_eip_apply_output_assembly(const uint8_t* data, size_t length)
{
    leap_gateway_runtime_lock();
    (void)leap_eip_bridge_apply_output_assembly(&g_gateway.bridge, data, length);
    leap_gateway_runtime_unlock();
}

void
leap_gateway_eip_pack_input_assembly(
    uint8_t* data,
    size_t   capacity,
    size_t*  length)
{
    leap_gateway_runtime_lock();
    (void)leap_eip_bridge_pack_input_assembly(
      &g_gateway.bridge,
      data,
      capacity,
      length);
    leap_gateway_runtime_unlock();
}

void
leap_gateway_eip_force_assembly_sizes(void)
{
    g_gateway.config.bridge.input_assembly_id = LEAP_GATEWAY_INPUT_ASSEMBLY_NUM;
    g_gateway.config.bridge.output_assembly_id = LEAP_GATEWAY_OUTPUT_ASSEMBLY_NUM;
    g_gateway.config.bridge.input_assembly_size = LEAP_GATEWAY_IO_ASSEMBLY_BYTES;
    g_gateway.config.bridge.output_assembly_size = LEAP_GATEWAY_IO_ASSEMBLY_BYTES;
    leap_eip_bridge_set_config(&g_gateway.bridge, &g_gateway.config.bridge);
}
