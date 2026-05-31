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

typedef struct LeapControllerPeerEntry
{
    uint8_t  mac[6];
    uint32_t active_profile_id;
    uint32_t default_profile_id;
    uint16_t device_state;
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
    LEAP_CTRL_PEER_TABLE_FULL
} LeapControllerPeerStatus;

void leap_controller_peer_table_init(LeapControllerPeerTable* table);

/*
 * Broadcast HELLO and collect HELLO_REPLY frames until scan_duration_ms elapses.
 * Duplicate MACs update the existing entry.
 */
LeapControllerPeerStatus leap_controller_peer_table_discover(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    int                          scan_duration_ms);

int leap_controller_peer_table_find(
    const LeapControllerPeerTable* table,
    const uint8_t*                 mac);

const LeapControllerPeerEntry* leap_controller_peer_table_get(
    const LeapControllerPeerTable* table,
    unsigned                       index);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONTROLLER_PEER_H */
