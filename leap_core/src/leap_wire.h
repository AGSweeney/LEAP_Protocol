/*
 * leap_wire.h
 *
 * Internal little-endian wire helpers for LEAP payload serialization.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_WIRE_H
#define LEAP_WIRE_H

#include <stdint.h>

static inline uint16_t leap_wire_read_le16(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static inline uint32_t leap_wire_read_le32(const uint8_t* p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint64_t leap_wire_read_le64(const uint8_t* p)
{
    return ((uint64_t)leap_wire_read_le32(p)) |
           ((uint64_t)leap_wire_read_le32(p + 4) << 32);
}

static inline void leap_wire_write_le16(uint8_t* p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static inline void leap_wire_write_le32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
    p[2] = (uint8_t)((value >> 16) & 0xFFu);
    p[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static inline void leap_wire_write_le64(uint8_t* p, uint64_t value)
{
    leap_wire_write_le32(p, (uint32_t)(value & 0xFFFFFFFFu));
    leap_wire_write_le32(p + 4, (uint32_t)((value >> 32) & 0xFFFFFFFFu));
}

#endif /* LEAP_WIRE_H */
