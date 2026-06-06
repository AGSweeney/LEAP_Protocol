/*
 * leap_controller_stack.h
 *
 * Controller-side bootstrap FSM: DISC -> DIR -> MGMT -> OP.
 * Transport-agnostic — pair with LeapControllerStackIo callbacks.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONTROLLER_STACK_H
#define LEAP_CONTROLLER_STACK_H

#include <stddef.h>
#include <stdint.h>

#include "leap/leap_dir_controller.h"
#include "leap/leap_dir_controller_capabilities.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_controller_sequence.h"
#include "leap/leap_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_CTRL_STACK_DIAG_MAX_COUNTERS 16u

typedef enum LeapControllerStackDiagStatus
{
    LEAP_CTRL_STACK_DIAG_OK = 0,
    LEAP_CTRL_STACK_DIAG_INVALID_ARG,
    LEAP_CTRL_STACK_DIAG_NOT_OP,
    LEAP_CTRL_STACK_DIAG_IO_MISSING,
    LEAP_CTRL_STACK_DIAG_SEND_FAILED,
    LEAP_CTRL_STACK_DIAG_RECV_TIMEOUT,
    LEAP_CTRL_STACK_DIAG_UNEXPECTED_REPLY,
    LEAP_CTRL_STACK_DIAG_PARSE_ERROR
} LeapControllerStackDiagStatus;

typedef struct LeapControllerStackDiagResult
{
    int              has_counters;
    int              has_timing;
    uint16_t         counter_count;
    LeapCounterEntry counters[LEAP_CTRL_STACK_DIAG_MAX_COUNTERS];
    LeapTimingReply  timing;
} LeapControllerStackDiagResult;

#define LEAP_CTRL_STACK_BROADCAST_MAC \
    { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu }

typedef struct LeapControllerStackConfig
{
    LeapMgmtControllerConfig mgmt;
    LeapPdControllerConfig   pd;
    uint32_t                 default_profile_id;
    uint32_t                 bootstrap_lease_us;
    uint32_t                 bootstrap_watchdog_us;
    int                      recv_timeout_ms;
    /* OWNER_RELEASE safe profile; 0 = device default safe profile */
    uint32_t                 release_safe_profile_id;
    /*
     * When non-zero, accept the first HELLO_REPLY during bootstrap (loopback demo).
     * When zero, only accept HELLO_REPLY from target_peer_mac.
     */
    int                      single_peer_auto_select;
    uint8_t                  target_peer_mac[6];
    LeapControllerFrameSequenceConfig frame_sequence;
} LeapControllerStackConfig;

typedef enum LeapControllerStackPhase
{
    LEAP_CTRL_STACK_IDLE = 0,
    LEAP_CTRL_STACK_DISCOVERING,
    LEAP_CTRL_STACK_SELECT_PROFILE,
    LEAP_CTRL_STACK_OPEN_SESSION,
    LEAP_CTRL_STACK_SET_STATE,
    LEAP_CTRL_STACK_OP,
    LEAP_CTRL_STACK_FAULT
} LeapControllerStackPhase;

typedef enum LeapControllerStackStatus
{
    LEAP_CTRL_STACK_OK = 0,
    LEAP_CTRL_STACK_INVALID_ARG,
    LEAP_CTRL_STACK_IO_MISSING,
    LEAP_CTRL_STACK_SEND_FAILED,
    LEAP_CTRL_STACK_RECV_TIMEOUT,
    LEAP_CTRL_STACK_UNEXPECTED_REPLY,
    LEAP_CTRL_STACK_MGMT_ERROR,
    LEAP_CTRL_STACK_DIR_ERROR,
    LEAP_CTRL_STACK_DISC_ERROR,
    LEAP_CTRL_STACK_ABORTED,
    LEAP_CTRL_STACK_IGNORED
} LeapControllerStackStatus;

#define LEAP_CTRL_STACK_FLAG_PEER_DISCOVERED  (1u << 0)
#define LEAP_CTRL_STACK_FLAG_PROFILE_SELECTED (1u << 1)
#define LEAP_CTRL_STACK_FLAG_SESSION_OPENED   (1u << 2)
#define LEAP_CTRL_STACK_FLAG_OP_ENTERED       (1u << 3)
#define LEAP_CTRL_STACK_FLAG_FAULT            (1u << 4)
#define LEAP_CTRL_STACK_FLAG_DUPLICATE_FRAME  (1u << 5)
#define LEAP_CTRL_STACK_FLAG_MGMT_PROCESSED   (1u << 6)
#define LEAP_CTRL_STACK_FLAG_SEQUENCE_GAP     (1u << 7)
#define LEAP_CTRL_STACK_FLAG_SESSION_MISMATCH (1u << 8)

typedef struct LeapControllerStackIo
{
    void* user_ctx;

    int (*send_frame)(
        void*          user_ctx,
        const uint8_t* dst_mac,
        uint8_t        flags,
        uint16_t       service_id,
        uint16_t       message_type,
        uint32_t       session_id,
        uint32_t       sequence,
        uint32_t       ack_sequence,
        const uint8_t* payload,
        size_t         payload_length);

    int (*recv_frame)(
        void*          user_ctx,
        uint8_t*       src_mac,
        uint8_t*       payload_buf,
        size_t         payload_capacity,
        size_t*        payload_length,
        LeapFrameView* parsed,
        int            timeout_ms);

    uint64_t (*monotonic_us)(void* user_ctx);
} LeapControllerStackIo;

typedef struct LeapControllerStack
{
    LeapControllerStackConfig config;
    LeapControllerStackPhase  phase;
    LeapControllerStackStatus last_status;

    LeapMgmtControllerContext mgmt;
    LeapPdControllerContext   pd;

    uint8_t peer_mac[6];
    int     peer_bound;

    LeapControllerFrameSequenceState frame_seq;
} LeapControllerStack;

typedef struct LeapControllerStackEvent
{
    LeapControllerStackStatus    status;
    LeapControllerStackPhase     phase;
    uint32_t                     flags;
    LeapDirControllerProfileInfo profile_info;
    uint16_t                     error_code;
} LeapControllerStackEvent;

void leap_controller_stack_init(
    LeapControllerStack*             stack,
    const LeapControllerStackConfig* config);

void leap_controller_stack_reset(LeapControllerStack* stack);

LeapControllerStackPhase leap_controller_stack_get_phase(
    const LeapControllerStack* stack);

LeapControllerStackStatus leap_controller_stack_step(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    LeapControllerStackEvent*     event);

LeapControllerStackStatus leap_controller_stack_bootstrap(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    uint8_t*                      peer_mac_out);

/*
 * Bootstrap a known peer without HELLO discovery (DIR -> MGMT -> OP).
 * hello_reply may be NULL to use default profile hints for MGMT state.
 */
LeapControllerStackStatus leap_controller_stack_bootstrap_peer(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    const LeapHelloReply*         hello_reply);

/*
 * Dispatch an inbound frame after bootstrap (or ERROR during bring-up).
 * PD exchange replies remain the responsibility of leap_pd_controller.
 * Returns LEAP_CTRL_STACK_IGNORED for non-peer or duplicate frames.
 */
LeapControllerStackStatus leap_controller_stack_on_frame(
    LeapControllerStack*         stack,
    const uint8_t*               src_mac,
    const LeapFrameView*         view,
    LeapControllerStackEvent*    event);

/*
 * Send OWNER_RELEASE to the peer (best-effort) and reset the stack to IDLE.
 */
LeapControllerStackStatus leap_controller_stack_release(
    LeapControllerStack*         stack,
    const LeapControllerStackIo* io);

LeapPdControllerStatus leap_controller_stack_run_cyclic_pd(
    LeapControllerStack*        stack,
    const LeapPdControllerIo*   pd_io,
    volatile int*               stop_flag);

LeapPdControllerStatus leap_controller_stack_pd_single_write(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    uint16_t                  digital_outputs);

/*
 * Post-OP diagnostics read (READ_COUNTERS + READ_TIMING). Requires owner session.
 */
LeapControllerStackDiagStatus leap_controller_stack_read_diag(
    LeapControllerStack*             stack,
    const LeapControllerStackIo*     io,
    LeapControllerStackDiagResult*   result_out);

/*
 * Extended DIAG counter read (switch-safe 0x0010-0x0016 range).
 */
LeapControllerStackDiagStatus leap_controller_stack_read_diag_extended(
    LeapControllerStack*             stack,
    const LeapControllerStackIo*     io,
    LeapControllerStackDiagResult*   result_out);

void leap_controller_stack_log_diag(
    const LeapControllerStackDiagResult* result);

/*
 * Query LEAP-DIR profile endpoints without full bootstrap (SELECT_PROFILE only).
 * Device returns PROFILE_REPLY with LeapEndpointDescriptor tail per spec.
 */
LeapControllerStackStatus leap_controller_stack_fetch_profile_reply(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    uint32_t                      profile_id,
    uint8_t*                      reply_payload,
    size_t                        reply_capacity,
    size_t*                       reply_length_out);

LeapControllerStackStatus leap_controller_stack_fetch_read_directory(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    uint8_t*                      reply_payload,
    size_t                        reply_capacity,
    size_t*                       reply_length_out);

/*
 * HELLO peer + LEAP-DIR READ_DIRECTORY (and SELECT_PROFILE when allowed).
 * Fills caps_out with endpoint descriptors per spec.
 */
LeapControllerStackStatus leap_controller_stack_probe_directory(
    LeapControllerStack*               stack,
    const LeapControllerStackIo*       io,
    const uint8_t*                     peer_mac,
    LeapDirControllerCapabilities* caps_out);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONTROLLER_STACK_H */
