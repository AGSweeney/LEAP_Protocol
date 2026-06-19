/*
 * gateway_global.h — Shared LeapOS-Gateway runtime state.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_GLOBAL_H
#define LEAP_GATEWAY_GLOBAL_H

#include "gateway_config.h"
#include "gateway_leap_session.h"
#include "leap_transport.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_eip_bridge.h"
#include "leap/leap_gateway_config.h"

typedef struct LeapGatewayRuntime
{
    LeapGatewayConfig             config;
    LeapEipBridgeState            bridge;
    LeapRtemsTransport            transport;
    LeapControllerSessionHub      session_hub;
    LeapControllerStackIo         controller_io;
    LeapControllerPeerTable       peer_table;
    LeapGatewayLeapSessionState   leap_session;
    int                           discover_pending_ms;
    int                           discover_active;
    int                           config_dirty;
    char                          bound_ifname[16];
} LeapGatewayRuntime;

extern LeapGatewayRuntime g_gateway;

void leap_gateway_runtime_init(void);

int leap_gateway_runtime_apply_config(const LeapGatewayConfig* config);

int leap_gateway_runtime_persist_config(void);

void leap_gateway_runtime_lock(void);

void leap_gateway_runtime_unlock(void);

#endif /* LEAP_GATEWAY_GLOBAL_H */
