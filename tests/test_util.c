/*
 * test_util.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_util.h"

#include <ctype.h>

static int leap_test_hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

int leap_test_hex_decode(const char* hex, uint8_t* out, size_t out_capacity, size_t* out_length)
{
    size_t out_index = 0u;
    int    hi;
    int    lo;

    if (hex == NULL || out == NULL || out_length == NULL)
    {
        return -1;
    }

    *out_length = 0u;

    while (*hex != '\0')
    {
        while (*hex != '\0' && isspace((unsigned char)*hex))
        {
            hex++;
        }

        if (*hex == '\0')
        {
            break;
        }

        hi = leap_test_hex_nibble(hex[0]);
        lo = leap_test_hex_nibble(hex[1]);
        if (hi < 0 || lo < 0)
        {
            return -1;
        }

        if (out_index >= out_capacity)
        {
            return -1;
        }

        out[out_index++] = (uint8_t)((hi << 4) | lo);
        hex += 2;
    }

    *out_length = out_index;
    return 0;
}
