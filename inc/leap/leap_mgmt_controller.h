/*
 * leap_mgmt_controller.h
 *
 * Controller-side LEAP-MGMT session state, request builders, and reply
 * processing. Transport-agnostic — pair with leap_frame_write() and your
 * link layer.
 *
 * Typical bring-up:
 *   HELLO (DISC) -> on_hello_reply()
 *   SELECT_PROFILE (DIR) — see leap_dir_controller.h
 *   build_open_session() -> on_open_session_reply()
 *   build_set_state(OP) -> on_state_reply()
 *   periodic build_heartbeat() while in OP
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_MGMT_CONTROLLER_H
#define LEAP_MGMT_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_MGMT_CONTROLLER_MAC_LEN 6u

typedef enum LeapMgmtControllerState
{
    LEAP_MGMT_CTRL_IDLE = 0,
    LEAP_MGMT_CTRL_DISCOVERED,
    LEAP_MGMT_CTRL_SESSION_OPEN,
    LEAP_MGMT_CTRL_OP,
    LEAP_MGMT_CTRL_STATE_FAULT
} LeapMgmtControllerState;

typedef struct LeapMgmtControllerConfig
{
    uint8_t  controller_mac[LEAP_MGMT_CONTROLLER_MAC_LEN];
    uint32_t default_lease_us;
    uint32_t default_watchdog_us;
    /*
     * Send HEARTBEAT when this fraction of granted_lease_us elapses since the
     * last successful lease refresh (heartbeat or accepted PD). Default 2 = 50%.
     */
    uint32_t heartbeat_lease_divisor;
} LeapMgmtControllerConfig;

typedef struct LeapMgmtControllerContext
{
    LeapMgmtControllerConfig  config;
    LeapMgmtControllerState   state;

    uint8_t  peer_mac[LEAP_MGMT_CONTROLLER_MAC_LEN];
    uint8_t  peer_known;

    uint32_t session_id;
    uint32_t sequence;
    uint32_t granted_lease_us;
    uint32_t granted_watchdog_us;
    uint16_t session_flags;

    LeapState_u16 peer_device_state;
    uint32_t      active_profile_id;
    uint32_t      default_profile_id;

    uint64_t last_lease_refresh_us;
    uint32_t latest_process_sequence;
} LeapMgmtControllerContext;

typedef enum LeapMgmtControllerStatus
{
    LEAP_MGMT_CTRL_OK = 0,
    LEAP_MGMT_CTRL_BAD_LENGTH,
    LEAP_MGMT_CTRL_BAD_STATE,
    LEAP_MGMT_CTRL_UNEXPECTED_REPLY,
    LEAP_MGMT_CTRL_ERROR_STATUS,
    LEAP_MGMT_CTRL_ERROR
} LeapMgmtControllerStatus;

#define LEAP_MGMT_CTRL_FLAG_PEER_DISCOVERED   (1u << 0)
#define LEAP_MGMT_CTRL_FLAG_SESSION_OPENED    (1u << 1)
#define LEAP_MGMT_CTRL_FLAG_STATE_CHANGED     (1u << 2)
#define LEAP_MGMT_CTRL_FLAG_OP_ENTERED        (1u << 3)
#define LEAP_MGMT_CTRL_FLAG_LEASE_REFRESHED   (1u << 4)

typedef struct LeapMgmtControllerEvent
{
    LeapMgmtControllerStatus status;
    uint32_t                 flags;
    LeapStatusCode_u16       error_code;
    LeapState_u16            peer_device_state;
} LeapMgmtControllerEvent;

void leap_mgmt_controller_init(
    LeapMgmtControllerContext*       ctx,
    const LeapMgmtControllerConfig* config);

void leap_mgmt_controller_reset(LeapMgmtControllerContext* ctx);

LeapMgmtControllerStatus leap_mgmt_controller_on_hello_reply(
    LeapMgmtControllerContext* ctx,
    const uint8_t*             source_mac,
    const uint8_t*               payload,
    size_t                       payload_length,
    LeapMgmtControllerEvent*     event);

size_t leap_mgmt_controller_build_open_session(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity,
    uint32_t                   lease_us,
    uint32_t                   watchdog_us,
    uint16_t                   extra_open_flags);

LeapMgmtControllerStatus leap_mgmt_controller_on_open_session_reply(
    LeapMgmtControllerContext* ctx,
    const uint8_t*             payload,
    size_t                     payload_length,
    LeapMgmtControllerEvent*   event);

size_t leap_mgmt_controller_build_set_state(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity,
    LeapState_u16              requested_state);

LeapMgmtControllerStatus leap_mgmt_controller_on_state_reply(
    LeapMgmtControllerContext* ctx,
    const uint8_t*             payload,
    size_t                     payload_length,
    LeapMgmtControllerEvent*   event);

size_t leap_mgmt_controller_build_heartbeat(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity);

size_t leap_mgmt_controller_build_owner_release(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity,
    uint32_t                   safe_profile_id);

LeapMgmtControllerStatus leap_mgmt_controller_on_mgmt_reply(
    LeapMgmtControllerContext*   ctx,
    const LeapFrameView*         view,
    LeapMgmtControllerEvent*     event);

int leap_mgmt_controller_should_send_heartbeat(
    const LeapMgmtControllerContext* ctx,
    uint64_t                         now_us);

void leap_mgmt_controller_on_heartbeat_sent(
    LeapMgmtControllerContext* ctx,
    uint64_t                   now_us);

void leap_mgmt_controller_on_pd_sent(
    LeapMgmtControllerContext* ctx,
    uint32_t                   process_sequence,
    uint64_t                   now_us);

uint32_t leap_mgmt_controller_next_sequence(LeapMgmtControllerContext* ctx);

LeapMgmtControllerState leap_mgmt_controller_get_state(
    const LeapMgmtControllerContext* ctx);

uint32_t leap_mgmt_controller_session_id(
    const LeapMgmtControllerContext* ctx);

const uint8_t* leap_mgmt_controller_peer_mac(
    const LeapMgmtControllerContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_MGMT_CONTROLLER_H */
