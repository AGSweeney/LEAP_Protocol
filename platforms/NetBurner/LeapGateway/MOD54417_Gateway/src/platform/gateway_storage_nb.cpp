/*
 * gateway_storage_nb.cpp - Config persistence in NetBurner NNDK config storage.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gateway_storage.h"

#include "gateway_config.h"

#include "leap/leap_gateway_config.h"

#include <config_obj.h>
#include <config_server.h>

#include <cstdio>
#include <cstring>

static config_string g_leap_gateway_config_text(
    appdata,
    "",
    "leap_gateway_config",
    "LEAP Gateway mapping configuration");

static int g_storage_ready = 0;

int leap_gateway_storage_init(void)
{
    g_storage_ready = 1;
    return 0;
}

int leap_gateway_storage_retry_after_pci(void)
{
    return leap_gateway_storage_init();
}

int leap_gateway_storage_ready(void)
{
    return g_storage_ready;
}

const char* leap_gateway_storage_mount_point(void)
{
    return LEAP_GATEWAY_STORAGE_MOUNT;
}

int leap_gateway_storage_load_config(LeapGatewayConfig* config)
{
    if (config == nullptr)
    {
        return -1;
    }

    const char* text = g_leap_gateway_config_text.c_str();
    if (text == nullptr || text[0] == '\0')
    {
        leap_gateway_config_defaults(config);
        snprintf(config->config_path, sizeof(config->config_path), "%s", LEAP_GATEWAY_CONFIG_PATH);
        return -1;
    }

    if (leap_gateway_config_load_text(config, text) != 0)
    {
        leap_gateway_config_defaults(config);
        snprintf(config->config_path, sizeof(config->config_path), "%s", LEAP_GATEWAY_CONFIG_PATH);
        return -1;
    }

    snprintf(config->config_path, sizeof(config->config_path), "%s", LEAP_GATEWAY_CONFIG_PATH);
    return 0;
}

int leap_gateway_storage_save_config(const LeapGatewayConfig* config)
{
    static char text[8192];

    if (config == nullptr)
    {
        return -1;
    }

    if (leap_gateway_config_export_text(config, text, sizeof(text)) != 0)
    {
        return -1;
    }

    g_leap_gateway_config_text = text;
    SaveConfigToStorage();
    return 0;
}
