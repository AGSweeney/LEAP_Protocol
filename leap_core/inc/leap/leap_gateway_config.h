/*
 * leap_gateway_config.h — LeapOS-Gateway persisted configuration.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_CONFIG_H
#define LEAP_GATEWAY_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_eip_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_GATEWAY_IFNAME_MAX   16u
#define LEAP_GATEWAY_IPV4_STR_MAX 16u
#define LEAP_GATEWAY_PATH_MAX     128u

typedef enum LeapGatewayNicMode
{
    LEAP_GATEWAY_NIC_SINGLE = 0,
    LEAP_GATEWAY_NIC_DUAL   = 1
} LeapGatewayNicMode;

typedef struct LeapGatewayNetworkConfig
{
    LeapGatewayNicMode mode;
    char               ifname[LEAP_GATEWAY_IFNAME_MAX];
    char               leap_ifname[LEAP_GATEWAY_IFNAME_MAX];
    char               eip_ifname[LEAP_GATEWAY_IFNAME_MAX];
    char               ipv4_addr[LEAP_GATEWAY_IPV4_STR_MAX];
    char               ipv4_mask[LEAP_GATEWAY_IPV4_STR_MAX];
    int                dhcp;
    int                auto_ifname;
} LeapGatewayNetworkConfig;

typedef struct LeapGatewayConfig
{
    unsigned                 version;
    LeapGatewayNetworkConfig network;
    LeapEipBridgeConfig      bridge;
    unsigned                 cyclic_ms;
    char                     config_path[LEAP_GATEWAY_PATH_MAX];
} LeapGatewayConfig;

void leap_gateway_config_defaults(LeapGatewayConfig* config);

const char*
leap_gateway_leap_ifname(const LeapGatewayConfig* config);

const char*
leap_gateway_eip_ifname(const LeapGatewayConfig* config);

int leap_gateway_config_validate(const LeapGatewayConfig* config);

int leap_gateway_config_load_file(LeapGatewayConfig* config, const char* path);

int leap_gateway_config_load_text(LeapGatewayConfig* config, const char* text);

int leap_gateway_config_export_text(
    const LeapGatewayConfig* config,
    char*                    buffer,
    size_t                   capacity);

int leap_gateway_config_save_file(
    const LeapGatewayConfig* config,
    const char*              path);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_GATEWAY_CONFIG_H */
