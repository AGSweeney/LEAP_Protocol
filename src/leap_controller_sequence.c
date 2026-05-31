/*
 * leap_controller_sequence.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_controller_sequence.h"

#include "leap/leap_log.h"

#include <stddef.h>

void leap_controller_frame_sequence_init(LeapControllerFrameSequenceState* state)
{
    if (state == NULL)
    {
        return;
    }

    leap_controller_frame_sequence_reset(state);
}

void leap_controller_frame_sequence_reset(LeapControllerFrameSequenceState* state)
{
    if (state == NULL)
    {
        return;
    }

    state->highest_peer_sequence = 0u;
    state->bound_session_id      = 0u;
    state->duplicate_frames      = 0u;
    state->sequence_gaps         = 0u;
    state->gap_rejects           = 0u;
    state->out_of_window_rejects   = 0u;
    state->session_mismatches      = 0u;
    state->sequence_active         = 0;
}

void leap_controller_frame_sequence_bind_session(
    LeapControllerFrameSequenceState* state,
    uint32_t                          session_id)
{
    if (state == NULL)
    {
        return;
    }

    state->bound_session_id = session_id;
}

LeapControllerFrameSequenceResult leap_controller_frame_sequence_accept(
    LeapControllerFrameSequenceState*        state,
    const LeapControllerFrameSequenceConfig* config,
    uint32_t                                 frame_session_id,
    uint32_t                                 sequence)
{
    uint32_t window;
    uint32_t gap;

    if (state == NULL)
    {
        return LEAP_CTRL_FRAME_SEQ_INVALID_ARG;
    }

    /*
     * Multi-peer safety: once OP is established, ignore MGMT/PD from another
     * session_id on the same MAC (stale owner or mis-routed reply).
     */
    if (config != NULL && config->enforce_session_match != 0 &&
        state->bound_session_id != 0u && frame_session_id != 0u &&
        frame_session_id != state->bound_session_id)
    {
        state->session_mismatches++;
        leap_log_security(
            LEAP_LOG_SEC_FRAME_SEQ_SESSION_MISMATCH,
            "session_id=%u bound=%u",
            frame_session_id,
            state->bound_session_id);
        return LEAP_CTRL_FRAME_SEQ_SESSION_MISMATCH;
    }

    if (sequence == 0u)
    {
        return LEAP_CTRL_FRAME_SEQ_OK;
    }

    window = LEAP_CTRL_FRAME_SEQ_WINDOW_DEFAULT;
    if (config != NULL && config->window_size != 0u)
    {
        window = config->window_size;
    }

    if (state->sequence_active == 0)
    {
        state->highest_peer_sequence = sequence;
        state->sequence_active       = 1;
        return LEAP_CTRL_FRAME_SEQ_OK;
    }

    if (sequence <= state->highest_peer_sequence)
    {
        state->duplicate_frames++;
        leap_log_security(
            LEAP_LOG_SEC_FRAME_SEQ_DUPLICATE,
            "sequence=%u highest=%u",
            sequence,
            state->highest_peer_sequence);
        return LEAP_CTRL_FRAME_SEQ_DUPLICATE;
    }

    gap = sequence - state->highest_peer_sequence - 1u;
    if (gap > 0u)
    {
        state->sequence_gaps += gap;

        if (config != NULL && config->reject_sequence_gaps != 0)
        {
            state->gap_rejects++;
            leap_log_security(
                LEAP_LOG_SEC_FRAME_SEQ_GAP,
                "sequence=%u highest=%u gap=%u",
                sequence,
                state->highest_peer_sequence,
                gap);
            return LEAP_CTRL_FRAME_SEQ_GAP;
        }
    }

    if (config != NULL && config->reject_out_of_window != 0 &&
        sequence > state->highest_peer_sequence + window)
    {
        state->out_of_window_rejects++;
        leap_log_security(
            LEAP_LOG_SEC_FRAME_SEQ_OUT_OF_WINDOW,
            "sequence=%u highest=%u window=%u",
            sequence,
            state->highest_peer_sequence,
            window);
        return LEAP_CTRL_FRAME_SEQ_OUT_OF_WINDOW;
    }

    state->highest_peer_sequence = sequence;
    return LEAP_CTRL_FRAME_SEQ_OK;
}
