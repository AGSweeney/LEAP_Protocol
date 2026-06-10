/*
 * test_eip_bridge.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_eip_bridge.h"
#include "leap/leap_gateway_config.h"
#include "leap/leap_protocol.h"

#include <string.h>

TEST(test_eip_bridge_output_to_peer)
{
    LeapEipBridgeState state;
    LeapEipBridgeConfig config;
    uint8_t            out[32];
    uint16_t           peer_out;

    leap_eip_bridge_init(&state);
    memset(&config, 0, sizeof(config));
    config.output_assembly_size = 32u;
    config.input_assembly_size = 32u;
    config.mapping_count = 1u;
    config.mappings[0].enabled = 1;
    config.mappings[0].output.assembly_byte = 2u;
    config.mappings[0].output.bit = 0u;
    config.mappings[0].output.width_bits = 8u;
    leap_eip_bridge_set_config(&state, &config);

    memset(out, 0, sizeof(out));
    out[2] = 0xA5u;
    ASSERT_EQ_INT(0, leap_eip_bridge_apply_output_assembly(&state, out, sizeof(out)));
    ASSERT_EQ_INT(0, leap_eip_bridge_peer_outputs(&state, 0u, &peer_out));
    ASSERT_EQ_U16(0xA5u, peer_out);
}

TEST(test_eip_bridge_pack_input_status)
{
    LeapEipBridgeState state;
    LeapEipBridgeConfig config;
    uint8_t            in[32];
    size_t             in_len = 0u;

    leap_eip_bridge_init(&state);
    memset(&config, 0, sizeof(config));
    config.input_assembly_size = 32u;
    config.output_assembly_size = 32u;
    config.mapping_count = 1u;
    config.mappings[0].enabled = 1;
    config.mappings[0].input.assembly_byte = 0u;
    config.mappings[0].input.width_bits = 8u;
    config.mappings[0].status_assembly_byte = 4u;
    config.mappings[0].status_width_bytes = 2u;
    leap_eip_bridge_set_config(&state, &config);

    leap_eip_bridge_update_peer_io(&state, 0u, 0x3Cu, 0u, LEAP_DIO_STATUS_OK, 1);
    memset(in, 0, sizeof(in));
    ASSERT_EQ_INT(
        0,
        leap_eip_bridge_pack_input_assembly(&state, in, sizeof(in), &in_len));
    ASSERT_EQ_U32(32u, (uint32_t)in_len);
    ASSERT_EQ_U32(0x3Cu, in[0]);
    ASSERT_EQ_U32(0x00u, in[4]);
    ASSERT_EQ_U32(0x00u, in[5]);

    leap_eip_bridge_update_peer_io(
        &state, 0u, 0u, 0u, LEAP_DIO_STATUS_OK, 0);
    ASSERT_EQ_INT(
        0,
        leap_eip_bridge_pack_input_assembly(&state, in, sizeof(in), &in_len));
    ASSERT_EQ_U32(0x01u, in[4]);
}

TEST(test_gateway_config_defaults_valid)
{
    LeapGatewayConfig config;

    leap_gateway_config_defaults(&config);
    ASSERT_EQ_INT(0, leap_gateway_config_validate(&config));
    ASSERT_TRUE(strcmp(leap_gateway_leap_ifname(&config), "re0") == 0);
}

void
leap_run_eip_bridge_tests(void)
{
    printf("\n[eip_bridge]\n");
    RUN_TEST(test_eip_bridge_output_to_peer);
    RUN_TEST(test_eip_bridge_pack_input_status);
    RUN_TEST(test_gateway_config_defaults_valid);
}
