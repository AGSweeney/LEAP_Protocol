/*
 * leap_crc.c
 *
 * Table-driven CRC-16/XMODEM and CRC-32C (Castagnoli) for LEAP v1.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_crc.h"

#include "leap/leap_protocol.h"

static uint16_t g_crc16_xmodem_table[256];
static uint32_t g_crc32c_table[256];
static int      g_crc_tables_initialized;

static void leap_crc_init_tables(void)
{
    unsigned int i;
    int          bit;

    if (g_crc_tables_initialized != 0)
    {
        return;
    }

    for (i = 0u; i < 256u; i++)
    {
        uint16_t crc16 = (uint16_t)(i << 8);
        uint32_t crc32 = (uint32_t)i;

        for (bit = 0; bit < 8; bit++)
        {
            if ((crc16 & 0x8000u) != 0u)
            {
                crc16 = (uint16_t)((crc16 << 1) ^ LEAP_CRC16_XMODEM_POLY);
            }
            else
            {
                crc16 = (uint16_t)(crc16 << 1);
            }

            if ((crc32 & 1u) != 0u)
            {
                crc32 = (crc32 >> 1) ^ 0x82F63B78u;
            }
            else
            {
                crc32 >>= 1;
            }
        }

        g_crc16_xmodem_table[i] = crc16;
        g_crc32c_table[i]       = crc32;
    }

    g_crc_tables_initialized = 1;
}

uint16_t leap_crc16_xmodem_update(uint16_t crc, const uint8_t* data, size_t length)
{
    size_t i;

    if (data == NULL || length == 0u)
    {
        return crc;
    }

    leap_crc_init_tables();

    for (i = 0u; i < length; i++)
    {
        const uint8_t index = (uint8_t)(((crc >> 8) ^ data[i]) & 0xFFu);
        crc = (uint16_t)((crc << 8) ^ g_crc16_xmodem_table[index]);
    }

    return crc;
}

uint16_t leap_crc16_xmodem(const uint8_t* data, size_t length)
{
    return leap_crc16_xmodem_update(LEAP_CRC16_XMODEM_INIT, data, length);
}

uint32_t leap_crc32c_update(uint32_t crc, const uint8_t* data, size_t length)
{
    size_t i;

    if (data == NULL || length == 0u)
    {
        return crc;
    }

    leap_crc_init_tables();

    for (i = 0u; i < length; i++)
    {
        const uint8_t index = (uint8_t)((crc ^ data[i]) & 0xFFu);
        crc = g_crc32c_table[index] ^ (crc >> 8);
    }

    return crc;
}

uint32_t leap_crc32c(const uint8_t* data, size_t length)
{
    uint32_t crc = leap_crc32c_update(LEAP_CRC32C_INIT, data, length);
    return crc ^ LEAP_CRC32C_XOR_OUT;
}
