/*
 * leap_conformance_win_io.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONFORMANCE_WIN_IO_H
#define LEAP_CONFORMANCE_WIN_IO_H

#include "leap/conformance/leap_conformance.h"
#include "leap/conformance/leap_conformance_metrics.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapConformanceWinContext LeapConformanceWinContext;

LeapConformanceWinContext* leap_conformance_win_create(void);
void leap_conformance_win_destroy(LeapConformanceWinContext* ctx);

void leap_conformance_win_set_retries(
    LeapConformanceWinContext* ctx,
    unsigned                   bootstrap_retries,
    unsigned                   retry_delay_ms);

void leap_conformance_win_set_peer_mac(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    int                        has_peer_mac);

const LeapConformanceIo* leap_conformance_win_io(LeapConformanceWinContext* ctx);

const LeapControllerPeerTable* leap_conformance_win_peer_table(
    LeapConformanceWinContext* ctx);

/*
 * Unicast DISC IDENTIFY and parse IDENTIFY_REPLY (no stale-OP release).
 * For discovery enrichment after HELLO scan.
 */
int leap_conformance_win_query_identify(
    LeapConformanceWinContext* ctx,
    const uint8_t*             peer_mac,
    LeapIdentifyReply*         reply_out);

void leap_conformance_win_invalidate_diag_cache(LeapConformanceWinContext* ctx);

int leap_conformance_win_transport_is_open(LeapConformanceWinContext* ctx);

/*
 * Ensure transport is open, peers are discovered, and controller is in OP
 * so diagnostics/monitor snapshots return live device fields.
 */
int leap_conformance_win_prepare_diagnostics(LeapConformanceWinContext* ctx);

int leap_conformance_win_prepare_io_session(LeapConformanceWinContext* ctx);

int leap_conformance_win_io_session_prepared(const LeapConformanceWinContext* ctx);

void leap_conformance_win_set_progress(
    LeapConformanceWinContext*    ctx,
    LeapConformanceProgressFn     progress_fn,
    void*                         progress_ctx);

void leap_conformance_win_reset_latency_trend(LeapConformanceWinContext* ctx);

int leap_conformance_win_session_is_op(const LeapConformanceWinContext* ctx);

int leap_conformance_win_ensure_op(LeapConformanceWinContext* ctx, uint16_t outputs);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONFORMANCE_WIN_IO_H */
