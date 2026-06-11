/*
 * gateway_global.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_global.h"
#include "gateway_leap_session.h"

#include <string.h>

LeapGatewayRuntime g_gateway;

void
leap_gateway_runtime_init(void)
{
    memset(&g_gateway, 0, sizeof(g_gateway));
    leap_gateway_config_defaults(&g_gateway.config);
    leap_eip_bridge_init(&g_gateway.bridge);
    leap_controller_peer_table_init(&g_gateway.peer_table);
    g_gateway.leap_session.op_peer_count = 0u;
}

int
leap_gateway_runtime_apply_config(const LeapGatewayConfig* config)
{
    char path[LEAP_GATEWAY_PATH_MAX];

    if (config == NULL)
    {
        return -1;
    }

    if (leap_gateway_config_validate(config) != 0)
    {
        return -1;
    }

    strncpy(path, g_gateway.config.config_path, sizeof(path) - 1u);
    path[sizeof(path) - 1u] = '\0';

    g_gateway.config = *config;
    if (path[0] != '\0')
    {
        strncpy(g_gateway.config.config_path, path, sizeof(g_gateway.config.config_path) - 1u);
    }

    leap_eip_bridge_set_config(&g_gateway.bridge, &g_gateway.config.bridge);
    g_gateway.config_dirty = 1;
    return 0;
}

int
leap_gateway_runtime_persist_config(void)
{
    const char* path = g_gateway.config.config_path;

    if (path[0] == '\0')
    {
        path = LEAP_GATEWAY_CONFIG_PATH;
    }

    return leap_gateway_config_save_file(&g_gateway.config, path);
}
