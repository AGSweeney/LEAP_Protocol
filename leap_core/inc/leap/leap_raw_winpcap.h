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

#define LEAP_RAW_WINPCAP_MAC_LEN         6u
#define LEAP_RAW_WINPCAP_NAME_MAX          256u
#define LEAP_RAW_WINPCAP_CAPTURE_PATH_MAX  260u

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
    int         promiscuous;
    int         filter_leap_ethertype;
    const char* capture_path;
} LeapRawWinpcapOpenOptions;

typedef struct LeapRawWinpcapSocket
{
    void*                 pcap;
    void*                 pcap_tx;
    char                  device_name[LEAP_RAW_WINPCAP_NAME_MAX];
    uint16_t              ethertype;
    uint8_t               local_mac[LEAP_RAW_WINPCAP_MAC_LEN];
    int                   promiscuous;
    int                   filter_ethertype;
    int                   cached_link_up;
    int                   last_rx_valid_leap;
    uint16_t              last_rx_service_id;
    uint16_t              last_rx_message_type;
    size_t                last_rx_payload_len;
    uint32_t              last_rx_eth_caplen;
    uint8_t               last_rx_payload[512];
    int                   capture_time_synced;
    uint64_t              capture_base_pcap_us;
    uint64_t              capture_base_mono_us;
    void*                 pcap_dump;
    char                  capture_path[LEAP_RAW_WINPCAP_CAPTURE_PATH_MAX];
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

/*
 * Discard any packets already buffered by Npcap (stale LEAP frames from a
 * prior session). Returns the number of packets drained.
 */
unsigned leap_raw_winpcap_drain_rx(LeapRawWinpcapSocket* sock);

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
    int                   timeout_ms,
    uint64_t*             capture_mono_us_out);

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

#define LEAP_RAW_WINPCAP_ADAPTER_MAX       32u
#define LEAP_RAW_WINPCAP_ADAPTER_PATH_MAX    256u
#define LEAP_RAW_WINPCAP_ADAPTER_LABEL_MAX   128u
#define LEAP_RAW_WINPCAP_ADAPTER_MAC_MAX     24u

typedef struct LeapRawWinpcapAdapterInfo
{
    char path[LEAP_RAW_WINPCAP_ADAPTER_PATH_MAX];
    char label[LEAP_RAW_WINPCAP_ADAPTER_LABEL_MAX];
    char mac[LEAP_RAW_WINPCAP_ADAPTER_MAC_MAX];
} LeapRawWinpcapAdapterInfo;

/*
 * Fill out[] with Npcap adapter paths (loopback first when capacity allows).
 * Returns number of entries written (0 on error or non-Windows).
 */
size_t leap_raw_winpcap_enumerate_adapters(
    LeapRawWinpcapAdapterInfo* out,
    size_t                     capacity);

void leap_raw_winpcap_list_devices(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_RAW_WINPCAP_H */
