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

typedef struct LeapRawLinuxOpenOptions
{
    /*
     * Enable IFF_PROMISC on the interface. Useful on shared LAN segments when
     * the switch does not flood unicast to the controller NIC.
     */
    int promiscuous;
    /*
     * When non-zero (default), ignore received frames not addressed to the
     * local MAC, broadcast, or multicast.
     */
    int filter_dest_mac;
} LeapRawLinuxOpenOptions;

typedef struct LeapRawLinuxStats
{
    uint64_t tx_frames_ok;
    uint64_t tx_bytes;
    uint64_t tx_partial_chunks;
    uint64_t tx_errors;
    uint64_t rx_frames_ok;
    uint64_t rx_bytes;
    uint64_t rx_filtered;
    uint64_t rx_timeouts;
    uint64_t rx_errors;
    uint64_t rx_short_frames;
} LeapRawLinuxStats;

typedef struct LeapRawLinuxSocket
{
    int               fd;
    uint16_t          ethertype;
    uint8_t           local_mac[LEAP_RAW_LINUX_MAC_LEN];
    int               promiscuous;
    int               filter_dest_mac;
    LeapRawLinuxStats stats;
} LeapRawLinuxSocket;

int leap_raw_linux_open(LeapRawLinuxSocket* sock, const char* ifname, uint16_t ethertype);

int leap_raw_linux_open_ex(
    LeapRawLinuxSocket*             sock,
    const char*                     ifname,
    uint16_t                        ethertype,
    const LeapRawLinuxOpenOptions* options);

void leap_raw_linux_close(LeapRawLinuxSocket* sock);

int leap_raw_linux_send(
    LeapRawLinuxSocket* sock,
    const uint8_t*      dst_mac,
    const uint8_t*      payload,
    size_t              payload_length);

int leap_raw_linux_recv(
    LeapRawLinuxSocket* sock,
    uint8_t*            src_mac,
    uint8_t*            payload,
    size_t              payload_capacity,
    size_t*             payload_length,
    int                 timeout_ms);

/*
 * Monotonic time in microseconds (CLOCK_MONOTONIC). Returns 0 on non-Linux.
 */
uint64_t leap_raw_linux_monotonic_us(void);

/*
 * errno from the last failed leap_raw_linux_send/rec/open call (0 if none).
 */
int leap_raw_linux_last_errno(void);

void leap_raw_linux_get_stats(
    const LeapRawLinuxSocket* sock,
    LeapRawLinuxStats*        out);

void leap_raw_linux_reset_stats(LeapRawLinuxSocket* sock);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_RAW_LINUX_H */
