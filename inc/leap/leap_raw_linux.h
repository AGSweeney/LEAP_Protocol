/*
 * leap_raw_linux.h
 *
 * Minimal Linux AF_PACKET raw Ethernet transport for LEAP examples.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RAW_LINUX_H
#define LEAP_RAW_LINUX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_RAW_LINUX_MAC_LEN 6u

typedef struct LeapRawLinuxSocket
{
    int      fd;
    uint16_t ethertype;
    uint8_t  local_mac[LEAP_RAW_LINUX_MAC_LEN];
} LeapRawLinuxSocket;

int leap_raw_linux_open(LeapRawLinuxSocket* sock, const char* ifname, uint16_t ethertype);
void leap_raw_linux_close(LeapRawLinuxSocket* sock);

int leap_raw_linux_send(
    const LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    const uint8_t*            payload,
    size_t                    payload_length);

int leap_raw_linux_recv(
    const LeapRawLinuxSocket* sock,
    uint8_t*                  src_mac,
    uint8_t*                  payload,
    size_t                    payload_capacity,
    size_t*                   payload_length,
    int                       timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_RAW_LINUX_H */
