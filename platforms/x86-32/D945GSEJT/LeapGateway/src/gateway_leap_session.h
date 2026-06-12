/*
 * gateway_leap_session.h — LEAP owner session for mapped peers.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_LEAP_SESSION_H
#define LEAP_GATEWAY_LEAP_SESSION_H

#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_stack.h"

typedef struct LeapGatewayRuntime LeapGatewayRuntime;

typedef struct LeapGatewayLeapSessionState
{
    int                       connect_pending;
    unsigned                  op_peer_count;
    LeapControllerStackStatus last_status;
    unsigned                  retry_ticks;
    unsigned                  pd_miss_count[LEAP_CTRL_MAX_PEERS];
    unsigned                  reconnect_ticks[LEAP_CTRL_MAX_PEERS];
} LeapGatewayLeapSessionState;

int leap_gateway_leap_session_active(const LeapGatewayRuntime* gw);

int leap_gateway_leap_session_start_task(void);

void leap_gateway_leap_session_request_connect(LeapGatewayRuntime* gw);

void leap_gateway_leap_session_request_auto_connect(LeapGatewayRuntime* gw);

int leap_gateway_leap_session_connect(LeapGatewayRuntime* gw);

void leap_gateway_leap_session_disconnect(LeapGatewayRuntime* gw);

#endif /* LEAP_GATEWAY_LEAP_SESSION_H */
