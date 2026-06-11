/*
 * leap_controller_peer.h
 *
 * Multi-device discovery: broadcast HELLO scan and peer table.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONTROLLER_PEER_H
#define LEAP_CONTROLLER_PEER_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_controller_stack.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_CTRL_MAX_PEERS 16u

#define LEAP_CTRL_PEER_DISCOVER_DEFAULT_SCAN_MS 1000
#define LEAP_CTRL_PEER_DISCOVER_MIN_SCAN_MS      250
#define LEAP_CTRL_PEER_PROBE_TIMEOUT_MS          500
#define LEAP_CTRL_HUB_BOOTSTRAP_RECV_MS          1000

typedef struct LeapControllerPeerDiscoverConfig
{
    /*
     * Broadcast HELLO listen window in ms. Use LEAP_CTRL_PEER_DISCOVER_MIN_SCAN_MS
     * when zero. Negative values skip the broadcast phase (probe-only bring-up).
     */
    int      scan_duration_ms;
    unsigned min_peers;
} LeapControllerPeerDiscoverConfig;

typedef struct LeapControllerPeerEntry
{
    uint8_t  mac[6];
    uint32_t active_profile_id;
    uint32_t default_profile_id;
    uint16_t device_state;
    uint8_t  active_owner_mac[6];
    int      reachable;
} LeapControllerPeerEntry;

typedef struct LeapControllerPeerTable
{
    LeapControllerPeerEntry peers[LEAP_CTRL_MAX_PEERS];
    unsigned                count;
} LeapControllerPeerTable;

typedef enum LeapControllerPeerStatus
{
    LEAP_CTRL_PEER_OK = 0,
    LEAP_CTRL_PEER_INVALID_ARG,
    LEAP_CTRL_PEER_IO_MISSING,
    LEAP_CTRL_PEER_SEND_FAILED,
    LEAP_CTRL_PEER_TABLE_FULL,
    LEAP_CTRL_PEER_NOT_FOUND
} LeapControllerPeerStatus;

void leap_controller_peer_table_init(LeapControllerPeerTable* table);

/*
 * Parse "aa:bb:cc:dd:ee:ff" or "aa-bb-cc-dd-ee-ff". Returns 1 on success.
 */
int leap_controller_peer_parse_mac(const char* text, uint8_t mac_out[6]);

/*
 * Broadcast HELLO and collect HELLO_REPLY frames until scan_duration_ms elapses.
 * Duplicate MACs update the existing entry. Equivalent to discover_ex with
 * min_peers = 0.
 */
LeapControllerPeerStatus leap_controller_peer_table_discover(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    int                          scan_duration_ms);

/*
 * Broadcast discovery with optional early exit once min_peers are in the table.
 */
LeapControllerPeerStatus leap_controller_peer_table_discover_ex(
    LeapControllerPeerTable*                  table,
    const LeapControllerStackIo*              io,
    const LeapControllerPeerDiscoverConfig*   config);

/*
 * Unicast HELLO to one peer and wait up to timeout_ms for HELLO_REPLY.
 */
LeapControllerPeerStatus leap_controller_peer_table_probe_peer(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    const uint8_t*               peer_mac,
    int                          timeout_ms);

int leap_controller_peer_table_find(
    const LeapControllerPeerTable* table,
    const uint8_t*                 mac);

const LeapControllerPeerEntry* leap_controller_peer_table_get(
    const LeapControllerPeerTable* table,
    unsigned                       index);

/*
 * Non-zero when HELLO reported an active owner MAC other than controller_mac.
 * Does not block bootstrap — use before OPEN_SESSION to avoid owner races.
 */
int leap_controller_peer_owned_by_other(
    const LeapControllerPeerEntry* entry,
    const uint8_t*                 controller_mac);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONTROLLER_PEER_H */
