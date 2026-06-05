/*
 * leap_commissioning.h
 *
 * One-shot commissioning actions shared by CLI and Qt Studio.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_COMMISSIONING_H
#define LEAP_COMMISSIONING_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_controller_peer.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapCommissioningContext LeapCommissioningContext;

typedef struct LeapCommissioningConfig
{
    const char* adapter;
    int         promiscuous;
    int         recv_timeout_ms;
} LeapCommissioningConfig;

typedef enum LeapCommissioningStatus
{
    LEAP_COMM_OK = 0,
    LEAP_COMM_INVALID_ARG,
    LEAP_COMM_TRANSPORT_ERROR,
    LEAP_COMM_TIMEOUT,
    LEAP_COMM_PROTOCOL_ERROR
} LeapCommissioningStatus;

typedef struct LeapCommissioningDiscoverResult
{
    LeapControllerPeerTable table;
    LeapHelloReply          hello_by_peer[LEAP_CTRL_MAX_PEERS];
    int                     has_hello[LEAP_CTRL_MAX_PEERS];
} LeapCommissioningDiscoverResult;

LeapCommissioningStatus leap_commissioning_open(
    LeapCommissioningContext**      ctx_out,
    const LeapCommissioningConfig*  config);

void leap_commissioning_close(LeapCommissioningContext* ctx);

LeapCommissioningStatus leap_commissioning_discover(
    LeapCommissioningContext*        ctx,
    int                              scan_ms,
    LeapCommissioningDiscoverResult* result_out);

LeapCommissioningStatus leap_commissioning_bootstrap(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    uint32_t                  lease_us);

LeapCommissioningStatus leap_commissioning_set_op(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac);

LeapCommissioningStatus leap_commissioning_set_safe(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac);

LeapCommissioningStatus leap_commissioning_release(
    LeapCommissioningContext* ctx);

LeapCommissioningStatus leap_commissioning_identify(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    LeapIdentifyReply*        reply_out);

LeapCommissioningStatus leap_commissioning_locate(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    unsigned                  duration_ms,
    unsigned                  pattern,
    LeapLocateDeviceReply*    reply_out);

LeapCommissioningStatus leap_commissioning_pd_write(
    LeapCommissioningContext* ctx,
    uint16_t                  outputs);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_COMMISSIONING_H */
