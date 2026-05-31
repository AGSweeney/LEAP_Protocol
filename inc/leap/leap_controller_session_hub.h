/*
 * leap_controller_session_hub.h
 *
 * Per-peer concurrent controller sessions: independent MGMT/PD state per device
 * on a shared transport.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONTROLLER_SESSION_HUB_H
#define LEAP_CONTROLLER_SESSION_HUB_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_pd_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapControllerSessionHubConfig
{
    LeapControllerStackConfig default_peer;
    /*
     * When non-zero, bootstrap_table skips peers whose HELLO active_owner_mac
     * is set and differs from default_peer.mgmt.controller_mac.
     */
    int skip_foreign_owned_peers;
} LeapControllerSessionHubConfig;

typedef struct LeapControllerPeerSlot
{
    uint8_t             peer_mac[6];
    int                 in_use;
    LeapControllerStack stack;
} LeapControllerPeerSlot;

typedef struct LeapControllerSessionHub
{
    LeapControllerSessionHubConfig config;
    LeapControllerPeerSlot         slots[LEAP_CTRL_MAX_PEERS];
    unsigned                       active_count;
} LeapControllerSessionHub;

typedef enum LeapControllerSessionHubStatus
{
    LEAP_CTRL_HUB_OK = 0,
    LEAP_CTRL_HUB_INVALID_ARG,
    LEAP_CTRL_HUB_FULL,
    LEAP_CTRL_HUB_NOT_FOUND,
    LEAP_CTRL_HUB_ALREADY_BOUND,
    LEAP_CTRL_HUB_NO_ACTIVE_PEERS
} LeapControllerSessionHubStatus;

void leap_controller_session_hub_init(
    LeapControllerSessionHub*             hub,
    const LeapControllerSessionHubConfig* config);

void leap_controller_session_hub_reset(LeapControllerSessionHub* hub);

unsigned leap_controller_session_hub_active_count(
    const LeapControllerSessionHub* hub);

/*
 * Find slot index by peer MAC, or -1 when not bound.
 */
int leap_controller_session_hub_find(
    const LeapControllerSessionHub* hub,
    const uint8_t*                  peer_mac);

LeapControllerStack* leap_controller_session_hub_stack(
    LeapControllerSessionHub* hub,
    int                       slot);

const uint8_t* leap_controller_session_hub_peer_mac(
    const LeapControllerSessionHub* hub,
    int                             slot);

int leap_controller_session_hub_is_op(
    const LeapControllerSessionHub* hub,
    int                             slot);

/*
 * Allocate a slot, bootstrap the peer to OP, and return slot index.
 * Returns LEAP_CTRL_HUB_ALREADY_BOUND when the MAC is already active.
 */
LeapControllerStackStatus leap_controller_session_hub_bootstrap_peer(
    LeapControllerSessionHub*     hub,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    const LeapHelloReply*         hello_reply,
    int*                          slot_out);

LeapControllerStackStatus leap_controller_session_hub_bootstrap_peer_at_slot(
    LeapControllerSessionHub*     hub,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    const LeapHelloReply*         hello_reply,
    int                           slot);

/*
 * Bootstrap every entry in a discovery table (best-effort; continues on failure).
 * bootstrapped_count receives the number of peers now in OP.
 */
LeapControllerSessionHubStatus leap_controller_session_hub_bootstrap_table(
    LeapControllerSessionHub*          hub,
    const LeapControllerStackIo*       io,
    const LeapControllerPeerTable*     table,
    unsigned*                          bootstrapped_count);

/*
 * Route an inbound frame to the matching peer session.
 * Returns LEAP_CTRL_STACK_IGNORED when no slot matches src_mac.
 */
LeapControllerStackStatus leap_controller_session_hub_on_frame(
    LeapControllerSessionHub*    hub,
    const uint8_t*               src_mac,
    const LeapFrameView*         view,
    LeapControllerStackEvent*    event,
    int*                         slot_out);

LeapControllerStackStatus leap_controller_session_hub_release(
    LeapControllerSessionHub*    hub,
    int                          slot,
    const LeapControllerStackIo* io);

void leap_controller_session_hub_release_all(
    LeapControllerSessionHub*    hub,
    const LeapControllerStackIo* io);

LeapPdControllerStatus leap_controller_session_hub_run_one_cycle(
    LeapControllerSessionHub* hub,
    int                       slot,
    const LeapPdControllerIo* io,
    volatile int*             stop_flag);

/*
 * Run one PD cycle for each OP peer (round-robin). Repeats until stop_flag is set.
 */
LeapPdControllerStatus leap_controller_session_hub_run_round_robin(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* io,
    volatile int*             stop_flag);

/*
 * One parallel lap: send to all OP peers, finish each, optional lap pacing sleep.
 */
LeapPdControllerStatus leap_controller_session_hub_run_parallel_lap(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* io,
    volatile int*             stop_flag,
    int                       sleep_for_period);

/*
 * Parallel laps until stop_flag is set (no transport/link polling).
 */
LeapPdControllerStatus leap_controller_session_hub_run_parallel(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* io,
    volatile int*             stop_flag,
    int                       sleep_for_period);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONTROLLER_SESSION_HUB_H */
