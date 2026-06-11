/*
 * leap_gateway_config.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_gateway_config.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int
parse_hex_u32(const char* text, uint32_t* out)
{
    char* end = NULL;
    unsigned long value;

    if (text == NULL || out == NULL)
    {
        return -1;
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        value = strtoul(text + 2, &end, 16);
    }
    else
    {
        value = strtoul(text, &end, 16);
    }

    if (end == text)
    {
        return -1;
    }

    *out = (uint32_t)value;
    return 0;
}

static void
trim_newline(char* line)
{
    size_t len;

    if (line == NULL)
    {
        return;
    }

    len = strlen(line);
    while (len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r'))
    {
        line[len - 1u] = '\0';
        --len;
    }
}

void
leap_gateway_config_defaults(LeapGatewayConfig* config)
{
    LeapEipBridgeMapping* map;

    if (config == NULL)
    {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->version = 1u;
    config->cyclic_ms = 50u;
    strncpy(
        config->config_path,
        "/cf/config.txt",
        sizeof(config->config_path) - 1u);

    config->network.mode = LEAP_GATEWAY_NIC_SINGLE;
    config->network.auto_ifname = 1;
    strncpy(config->network.ifname, "re0", sizeof(config->network.ifname) - 1u);
    strncpy(
        config->network.ipv4_addr,
        "192.168.1.2",
        sizeof(config->network.ipv4_addr) - 1u);
    strncpy(
        config->network.ipv4_mask,
        "255.255.255.0",
        sizeof(config->network.ipv4_mask) - 1u);
    config->network.dhcp = 0;

    config->bridge.input_assembly_id = 100u;
    config->bridge.output_assembly_id = 150u;
    config->bridge.input_assembly_size = 32u;
    config->bridge.output_assembly_size = 32u;
    config->bridge.mapping_count = 1u;

    map = &config->bridge.mappings[0];
    memset(map->leap_mac, 0, sizeof(map->leap_mac));
    map->profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
    map->input.assembly_byte = 0u;
    map->input.bit = 0u;
    map->input.width_bits = 8u;
    map->output.assembly_byte = 2u;
    map->output.bit = 0u;
    map->output.width_bits = 8u;
    map->status_assembly_byte = 4u;
    map->status_width_bytes = 2u;
    map->enabled = 0;
}

const char*
leap_gateway_leap_ifname(const LeapGatewayConfig* config)
{
    if (config == NULL)
    {
        return "re0";
    }

    if (config->network.mode == LEAP_GATEWAY_NIC_DUAL &&
        config->network.leap_ifname[0] != '\0')
    {
        return config->network.leap_ifname;
    }

    if (config->network.ifname[0] != '\0')
    {
        return config->network.ifname;
    }

    return "re0";
}

const char*
leap_gateway_eip_ifname(const LeapGatewayConfig* config)
{
    if (config == NULL)
    {
        return "re0";
    }

    if (config->network.mode == LEAP_GATEWAY_NIC_DUAL &&
        config->network.eip_ifname[0] != '\0')
    {
        return config->network.eip_ifname;
    }

    return leap_gateway_leap_ifname(config);
}

int
leap_gateway_config_validate(const LeapGatewayConfig* config)
{
    unsigned i;

    if (config == NULL)
    {
        return -1;
    }

    if (config->bridge.input_assembly_size == 0u ||
        config->bridge.input_assembly_size > LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES ||
        config->bridge.output_assembly_size == 0u ||
        config->bridge.output_assembly_size > LEAP_EIP_BRIDGE_MAX_ASSEMBLY_BYTES)
    {
        return -1;
    }

    if (config->bridge.mapping_count > LEAP_EIP_BRIDGE_MAX_MAPPINGS)
    {
        return -1;
    }

    for (i = 0u; i < config->bridge.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &config->bridge.mappings[i];

        if (!map->enabled)
        {
            continue;
        }

        if (map->leap_mac[0] == 0u &&
            map->leap_mac[1] == 0u &&
            map->leap_mac[2] == 0u &&
            map->leap_mac[3] == 0u &&
            map->leap_mac[4] == 0u &&
            map->leap_mac[5] == 0u)
        {
            return -1;
        }

        if (map->input.width_bits == 0u || map->output.width_bits == 0u)
        {
            return -1;
        }
    }

    return 0;
}

static int
apply_config_kv(
    LeapGatewayConfig*     config,
    LeapEipBridgeMapping** active_inout,
    const char*            key,
    const char*            value)
{
    LeapEipBridgeMapping* active = active_inout != NULL ? *active_inout : NULL;

    if (config == NULL || key == NULL || value == NULL)
    {
        return -1;
    }

    if (strcmp(key, "network.mode") == 0)
    {
        if (strcmp(value, "dual") == 0)
        {
            config->network.mode = LEAP_GATEWAY_NIC_DUAL;
        }
        else
        {
            config->network.mode = LEAP_GATEWAY_NIC_SINGLE;
        }
    }
    else if (strcmp(key, "network.ifname") == 0)
    {
        strncpy(config->network.ifname, value, sizeof(config->network.ifname) - 1u);
        config->network.auto_ifname = 0;
    }
    else if (strcmp(key, "network.leap_ifname") == 0)
    {
        strncpy(
            config->network.leap_ifname,
            value,
            sizeof(config->network.leap_ifname) - 1u);
    }
    else if (strcmp(key, "network.eip_ifname") == 0)
    {
        strncpy(
            config->network.eip_ifname,
            value,
            sizeof(config->network.eip_ifname) - 1u);
    }
    else if (strcmp(key, "network.ipv4") == 0)
    {
        strncpy(
            config->network.ipv4_addr,
            value,
            sizeof(config->network.ipv4_addr) - 1u);
    }
    else if (strcmp(key, "network.mask") == 0)
    {
        strncpy(
            config->network.ipv4_mask,
            value,
            sizeof(config->network.ipv4_mask) - 1u);
    }
    else if (strcmp(key, "network.dhcp") == 0)
    {
        config->network.dhcp =
            (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    }
    else if (strcmp(key, "cyclic_ms") == 0)
    {
        config->cyclic_ms = (unsigned)strtoul(value, NULL, 10);
    }
    else if (strcmp(key, "mapping.begin") == 0)
    {
        unsigned index = (unsigned)strtoul(value, NULL, 10);

        if (index < LEAP_EIP_BRIDGE_MAX_MAPPINGS)
        {
            active = &config->bridge.mappings[index];
            if (index + 1u > config->bridge.mapping_count)
            {
                config->bridge.mapping_count = index + 1u;
            }
            memset(active, 0, sizeof(*active));
            active->profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
            active->input.assembly_byte = index;
            active->input.bit = 0u;
            active->input.width_bits = 8u;
            active->output.assembly_byte = index + 2u;
            active->output.bit = 0u;
            active->output.width_bits = 8u;
            active->status_assembly_byte = index + 4u;
            active->status_width_bytes = 2u;
        }
        else
        {
            active = NULL;
        }
    }
    else if (active != NULL && strcmp(key, "mapping.mac") == 0)
    {
        (void)leap_controller_peer_parse_mac(value, active->leap_mac);
    }
    else if (active != NULL && strcmp(key, "mapping.profile") == 0)
    {
        (void)parse_hex_u32(value, &active->profile_id);
    }
    else if (active != NULL && strcmp(key, "mapping.input.byte") == 0)
    {
        active->input.assembly_byte = (uint16_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.output.byte") == 0)
    {
        active->output.assembly_byte = (uint16_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.status.byte") == 0)
    {
        active->status_assembly_byte = (uint16_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.input.bit") == 0)
    {
        active->input.bit = (uint8_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.output.bit") == 0)
    {
        active->output.bit = (uint8_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.input.width") == 0)
    {
        active->input.width_bits = (uint8_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.output.width") == 0)
    {
        active->output.width_bits = (uint8_t)strtoul(value, NULL, 10);
    }
    else if (active != NULL && strcmp(key, "mapping.enabled") == 0)
    {
        active->enabled =
            (strcmp(value, "1") == 0 || strcmp(value, "true") == 0);
    }

    if (active_inout != NULL)
    {
        *active_inout = active;
    }

    return 0;
}

static int
parse_config_line(
    LeapGatewayConfig*     config,
    LeapEipBridgeMapping** active_inout,
    char*                  line)
{
    char key[64];
    char value[160];

    trim_newline(line);
    if (line[0] == '\0' || line[0] == '#')
    {
        return 0;
    }

    if (sscanf(line, " %63[^=]= %159[^\n]", key, value) != 2)
    {
        return 0;
    }

    return apply_config_kv(config, active_inout, key, value);
}

int
leap_gateway_config_load_file(LeapGatewayConfig* config, const char* path)
{
    FILE* fp;
    char  line[256];
    LeapEipBridgeMapping* active = NULL;

    if (config == NULL || path == NULL)
    {
        return -1;
    }

    leap_gateway_config_defaults(config);
    strncpy(config->config_path, path, sizeof(config->config_path) - 1u);

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        return -1;
    }

    while (fgets(line, (int)sizeof(line), fp) != NULL)
    {
        (void)parse_config_line(config, &active, line);
    }

    fclose(fp);
    return leap_gateway_config_validate(config);
}

int
leap_gateway_config_load_text(LeapGatewayConfig* config, const char* text)
{
    char                  line[256];
    LeapEipBridgeMapping* active = NULL;
    const char*           cursor;
    size_t                i;

    if (config == NULL || text == NULL)
    {
        return -1;
    }

    leap_gateway_config_defaults(config);

    cursor = text;
    while (*cursor != '\0')
    {
        i = 0u;
        while (cursor[i] != '\0' && cursor[i] != '\n' && i + 1u < sizeof(line))
        {
            line[i] = cursor[i];
            ++i;
        }
        line[i] = '\0';
        (void)parse_config_line(config, &active, line);
        cursor += i;
        if (*cursor == '\n')
        {
            ++cursor;
        }
    }

    return leap_gateway_config_validate(config);
}

static int
export_append(
    char*       buffer,
    size_t      capacity,
    size_t*     used,
    const char* fmt,
    ...)
{
    va_list args;
    int     n;

    if (buffer == NULL || used == NULL || *used >= capacity)
    {
        return -1;
    }

    va_start(args, fmt);
    n = vsnprintf(buffer + *used, capacity - *used, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n >= capacity - *used)
    {
        return -1;
    }

    *used += (size_t)n;
    return 0;
}

int
leap_gateway_config_export_text(
    const LeapGatewayConfig* config,
    char*                    buffer,
    size_t                   capacity)
{
    unsigned    i;
    const char* mode_text;
    size_t      used = 0u;

    if (config == NULL || buffer == NULL || capacity == 0u)
    {
        return -1;
    }

    if (leap_gateway_config_validate(config) != 0)
    {
        return -1;
    }

    buffer[0] = '\0';
    mode_text = (config->network.mode == LEAP_GATEWAY_NIC_DUAL) ? "dual" : "single";

    if (export_append(
            buffer,
            capacity,
            &used,
            "# LeapOS-Gateway configuration v%u\n",
            config->version) != 0)
    {
        return -1;
    }
    if (export_append(buffer, capacity, &used, "network.mode=%s\n", mode_text) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "network.ifname=%s\n",
            config->network.ifname) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "network.leap_ifname=%s\n",
            config->network.leap_ifname) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "network.eip_ifname=%s\n",
            config->network.eip_ifname) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "network.ipv4=%s\n",
            config->network.ipv4_addr) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "network.mask=%s\n",
            config->network.ipv4_mask) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "network.dhcp=%d\n",
            config->network.dhcp ? 1 : 0) != 0 ||
        export_append(
            buffer,
            capacity,
            &used,
            "cyclic_ms=%u\n",
            config->cyclic_ms) != 0)
    {
        return -1;
    }

    for (i = 0u; i < config->bridge.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &config->bridge.mappings[i];

        if (export_append(
                buffer,
                capacity,
                &used,
                "mapping.begin=%u\n",
                i) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.enabled=%d\n",
                map->enabled ? 1 : 0) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                map->leap_mac[0],
                map->leap_mac[1],
                map->leap_mac[2],
                map->leap_mac[3],
                map->leap_mac[4],
                map->leap_mac[5]) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.profile=0x%08X\n",
                (unsigned)map->profile_id) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.input.byte=%u\n",
                (unsigned)map->input.assembly_byte) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.input.bit=%u\n",
                (unsigned)map->input.bit) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.input.width=%u\n",
                (unsigned)map->input.width_bits) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.output.byte=%u\n",
                (unsigned)map->output.assembly_byte) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.output.bit=%u\n",
                (unsigned)map->output.bit) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.output.width=%u\n",
                (unsigned)map->output.width_bits) != 0 ||
            export_append(
                buffer,
                capacity,
                &used,
                "mapping.status.byte=%u\n",
                (unsigned)map->status_assembly_byte) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int
leap_gateway_config_save_file(
    const LeapGatewayConfig* config,
    const char*              path)
{
    FILE* fp;
    unsigned i;
    const char* mode_text;

    if (config == NULL || path == NULL)
    {
        return -1;
    }

    if (leap_gateway_config_validate(config) != 0)
    {
        return -1;
    }

    fp = fopen(path, "w");
    if (fp == NULL)
    {
        return -1;
    }

    mode_text = (config->network.mode == LEAP_GATEWAY_NIC_DUAL) ? "dual" : "single";
    fprintf(fp, "# LeapOS-Gateway configuration v%u\n", config->version);
    fprintf(fp, "network.mode=%s\n", mode_text);
    fprintf(fp, "network.ifname=%s\n", config->network.ifname);
    fprintf(fp, "network.leap_ifname=%s\n", config->network.leap_ifname);
    fprintf(fp, "network.eip_ifname=%s\n", config->network.eip_ifname);
    fprintf(fp, "network.ipv4=%s\n", config->network.ipv4_addr);
    fprintf(fp, "network.mask=%s\n", config->network.ipv4_mask);
    fprintf(fp, "network.dhcp=%d\n", config->network.dhcp ? 1 : 0);
    fprintf(fp, "cyclic_ms=%u\n", config->cyclic_ms);

    for (i = 0u; i < config->bridge.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &config->bridge.mappings[i];

        fprintf(fp, "mapping.begin=%u\n", i);
        fprintf(fp, "mapping.enabled=%d\n", map->enabled ? 1 : 0);
        fprintf(
            fp,
            "mapping.mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
            map->leap_mac[0],
            map->leap_mac[1],
            map->leap_mac[2],
            map->leap_mac[3],
            map->leap_mac[4],
            map->leap_mac[5]);
        fprintf(fp, "mapping.profile=0x%08X\n", (unsigned)map->profile_id);
        fprintf(fp, "mapping.input.byte=%u\n", (unsigned)map->input.assembly_byte);
        fprintf(fp, "mapping.input.bit=%u\n", (unsigned)map->input.bit);
        fprintf(fp, "mapping.input.width=%u\n", (unsigned)map->input.width_bits);
        fprintf(fp, "mapping.output.byte=%u\n", (unsigned)map->output.assembly_byte);
        fprintf(fp, "mapping.output.bit=%u\n", (unsigned)map->output.bit);
        fprintf(fp, "mapping.output.width=%u\n", (unsigned)map->output.width_bits);
        fprintf(fp, "mapping.status.byte=%u\n", (unsigned)map->status_assembly_byte);
    }

    fclose(fp);
    return 0;
}
