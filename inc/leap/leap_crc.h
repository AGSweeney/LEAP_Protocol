/*
 * leap_crc.h
 *
 * LEAP CRC-16/XMODEM (header) and CRC-32C Castagnoli (payload) engines.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CRC_H
#define LEAP_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t leap_crc16_xmodem(const uint8_t* data, size_t length);
uint16_t leap_crc16_xmodem_update(uint16_t crc, const uint8_t* data, size_t length);

uint32_t leap_crc32c(const uint8_t* data, size_t length);
uint32_t leap_crc32c_update(uint32_t crc, const uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CRC_H */
