/*
 * leap_raw_winpcap.h
 *
 * Windows Npcap/WinPcap raw Ethernet transport for LEAP examples and smoke tests.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RAW_WINPCAP_H
#define LEAP_RAW_WINPCAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_RAW_WINPCAP_MAC_LEN    6u
#define LEAP_RAW_WINPCAP_NAME_MAX   256u

typedef struct LeapRawWinpcapLinkState
{
    int interface_up;
    int link_up;
} LeapRawWinpcapLinkState;

typedef struct LeapRawWinpcapStats
{
    uint64_t tx_frames_ok;
    uint64_t tx_bytes;
    uint64_t tx_errors;
    uint64_t rx_frames_ok;
    uint64_t rx_bytes;
    uint64_t rx_filtered;
    uint64_t rx_timeouts;
    uint64_t rx_errors;
    uint64_t rx_short_frames;
    uint64_t link_transitions;
} LeapRawWinpcapStats;

typedef struct LeapRawWinpcapOpenOptions
{
    int promiscuous;
    int filter_leap_ethertype;
} LeapRawWinpcapOpenOptions;

typedef struct LeapRawWinpcapSocket
{
    void*                 pcap;
    char                  device_name[LEAP_RAW_WINPCAP_NAME_MAX];
    uint16_t              ethertype;
    uint8_t               local_mac[LEAP_RAW_WINPCAP_MAC_LEN];
    int                   promiscuous;
    int                   filter_ethertype;
    int                   cached_link_up;
    LeapRawWinpcapStats   stats;
} LeapRawWinpcapSocket;

/*
 * Pick the Npcap loopback adapter when name_out is NULL; otherwise open name_out.
 */
int leap_raw_winpcap_open(
    LeapRawWinpcapSocket*             sock,
    const char*                       device_name,
    uint16_t                          ethertype,
    const LeapRawWinpcapOpenOptions* options);

void leap_raw_winpcap_close(LeapRawWinpcapSocket* sock);

int leap_raw_winpcap_send(
    LeapRawWinpcapSocket* sock,
    const uint8_t*        dst_mac,
    const uint8_t*        payload,
    size_t                payload_length);

int leap_raw_winpcap_recv(
    LeapRawWinpcapSocket* sock,
    uint8_t*              src_mac,
    uint8_t*              payload,
    size_t                payload_capacity,
    size_t*               payload_length,
    int                   timeout_ms);

uint64_t leap_raw_winpcap_monotonic_us(void);

int leap_raw_winpcap_last_errno(void);

const char* leap_raw_winpcap_last_error(void);

void leap_raw_winpcap_get_stats(
    const LeapRawWinpcapSocket* sock,
    LeapRawWinpcapStats*        out);

void leap_raw_winpcap_reset_stats(LeapRawWinpcapSocket* sock);

int leap_raw_winpcap_query_link(
    const LeapRawWinpcapSocket* sock,
    LeapRawWinpcapLinkState*    state_out);

int leap_raw_winpcap_poll_link(
    LeapRawWinpcapSocket*       sock,
    int*                        changed_out,
    LeapRawWinpcapLinkState*    state_out);

void leap_raw_winpcap_list_devices(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_RAW_WINPCAP_H */
