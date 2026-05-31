/*
 * leap_controller_sequence.h
 *
 * Per-peer Ethernet frame sequence tracking for controller-side replay
 * protection. Each peer slot in a session hub maintains independent state.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_CONTROLLER_SEQUENCE_H
#define LEAP_CONTROLLER_SEQUENCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_CTRL_FRAME_SEQ_WINDOW_DEFAULT 64u

typedef struct LeapControllerFrameSequenceConfig
{
    /*
     * Maximum forward jump (sequence - highest - 1) before rejecting as
     * out-of-window replay. Zero uses LEAP_CTRL_FRAME_SEQ_WINDOW_DEFAULT.
     */
    uint32_t window_size;
    /*
     * When non-zero, reject inbound frames whose sequence exceeds
     * highest + window_size (multi-peer wire isolation).
     */
    int reject_out_of_window;
    /*
     * When non-zero, reject frames whose LEAP session_id differs from the
     * bound owner session (prevents cross-peer frame confusion).
     */
    int enforce_session_match;
} LeapControllerFrameSequenceConfig;

typedef struct LeapControllerFrameSequenceState
{
    uint32_t highest_peer_sequence;
    uint32_t bound_session_id;
    uint32_t duplicate_frames;
    uint32_t sequence_gaps;
    uint32_t out_of_window_rejects;
    uint32_t session_mismatches;
    int      sequence_active;
} LeapControllerFrameSequenceState;

typedef enum LeapControllerFrameSequenceResult
{
    LEAP_CTRL_FRAME_SEQ_OK = 0,
    LEAP_CTRL_FRAME_SEQ_DUPLICATE,
    LEAP_CTRL_FRAME_SEQ_OUT_OF_WINDOW,
    LEAP_CTRL_FRAME_SEQ_SESSION_MISMATCH,
    LEAP_CTRL_FRAME_SEQ_INVALID_ARG
} LeapControllerFrameSequenceResult;

void leap_controller_frame_sequence_init(LeapControllerFrameSequenceState* state);

void leap_controller_frame_sequence_reset(LeapControllerFrameSequenceState* state);

void leap_controller_frame_sequence_bind_session(
    LeapControllerFrameSequenceState* state,
    uint32_t                          session_id);

/*
 * Validate an inbound peer frame sequence (§13.5). Updates counters on reject.
 * Forward gaps (sequence > highest + 1) are accepted but counted in
 * sequence_gaps — PD process_sequence is the authoritative cyclic guard.
 */
LeapControllerFrameSequenceResult leap_controller_frame_sequence_accept(
    LeapControllerFrameSequenceState*           state,
    const LeapControllerFrameSequenceConfig*    config,
    uint32_t                                    frame_session_id,
    uint32_t                                    sequence);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_CONTROLLER_SEQUENCE_H */
