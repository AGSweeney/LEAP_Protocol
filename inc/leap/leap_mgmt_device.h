/*
 * leap_mgmt_device.h
 *
 * Device-side LEAP-MGMT session, ownership, and state machine logic.
 *
 * State machine (device-side):
 *
 *   BOOT --transport ready--> INIT --profile selected--> CONFIGURED
 *   CONFIGURED --owner OPEN_SESSION--> SAFE --SET_STATE--> OP
 *   OP --owner release / lease expiry / watchdog expiry / owner violation--> SAFE
 *   OP --heartbeat loss / comms loss (tick)--> SAFE
 *   any --fault--> FAULT --FAULT_RESET--> INIT
 *
 * Lease refresh: HEARTBEAT or accepted LEAP-PD traffic extends the owner lease.
 * Process watchdog: armed on SET_STATE->OP; refreshed by accepted LEAP-PD in OP.
 * While in OP, wrong owner MAC or session on command frames forces SAFE immediately.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_MGMT_DEVICE_H
#define LEAP_MGMT_DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_MGMT_DEVICE_MAC_LEN 6u

typedef struct LeapMgmtDeviceConfig
{
    uint8_t  primary_mac[LEAP_MGMT_DEVICE_MAC_LEN];
    uint32_t default_lease_us;
    uint32_t default_watchdog_us;
    uint32_t max_lease_us;
    uint32_t max_watchdog_us;
} LeapMgmtDeviceConfig;

typedef struct LeapMgmtDeviceContext
{
    LeapState_u16 device_state;

    uint32_t next_session_id;

    uint8_t  owner_active;
    uint32_t owner_session_id;
    uint8_t  owner_mac[LEAP_MGMT_DEVICE_MAC_LEN];
    uint32_t granted_lease_us;
    uint32_t granted_watchdog_us;
    uint64_t lease_deadline_us;
    uint64_t watchdog_deadline_us;

    uint8_t  observer_active;
    uint32_t observer_session_id;
    uint8_t  observer_mac[LEAP_MGMT_DEVICE_MAC_LEN];
    uint64_t observer_deadline_us;

    LeapMgmtDeviceConfig config;
} LeapMgmtDeviceContext;

typedef struct LeapMgmtDeviceRequest
{
    const uint8_t* source_mac;
    uint32_t       session_id;
    uint16_t       message_type;
    const uint8_t* payload;
    size_t         payload_length;
    uint64_t         now_us;
} LeapMgmtDeviceRequest;

typedef enum LeapMgmtDeviceHandleStatus
{
    LEAP_MGMT_DEVICE_HANDLE_OK = 0,
    LEAP_MGMT_DEVICE_HANDLE_NO_REPLY,
    LEAP_MGMT_DEVICE_HANDLE_ERROR
} LeapMgmtDeviceHandleStatus;

typedef struct LeapMgmtDeviceReply
{
    LeapMgmtDeviceHandleStatus status;
    LeapStatusCode_u16         error_code;
    uint16_t                   message_type;
    uint8_t                    payload[64];
    size_t                     payload_length;
} LeapMgmtDeviceReply;

void leap_mgmt_device_init(LeapMgmtDeviceContext* ctx, const LeapMgmtDeviceConfig* config);

void leap_mgmt_device_on_transport_ready(LeapMgmtDeviceContext* ctx);
void leap_mgmt_device_on_profile_selected(LeapMgmtDeviceContext* ctx);
void leap_mgmt_device_enter_fault(LeapMgmtDeviceContext* ctx);

void leap_mgmt_device_tick(LeapMgmtDeviceContext* ctx, uint64_t now_us);

LeapMgmtDeviceHandleStatus leap_mgmt_device_handle(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply);

int leap_mgmt_device_session_allows_owner_pd(
    const LeapMgmtDeviceContext* ctx,
    uint32_t                     session_id,
    const uint8_t*               source_mac);

void leap_mgmt_device_refresh_owner_lease(LeapMgmtDeviceContext* ctx, uint64_t now_us);
void leap_mgmt_device_refresh_process_watchdog(LeapMgmtDeviceContext* ctx, uint64_t now_us);

/*
 * Clear owner state and transition OP -> SAFE. Used on owner violations while
 * outputs may be active and on lease/watchdog expiry paths.
 */
void leap_mgmt_device_force_safe(LeapMgmtDeviceContext* ctx);

LeapState_u16 leap_mgmt_device_get_state(const LeapMgmtDeviceContext* ctx);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_MGMT_DEVICE_H */
