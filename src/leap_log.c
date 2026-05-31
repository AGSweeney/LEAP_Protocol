/*
 * leap_log.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_log.h"

const char* leap_log_security_event_name(LeapLogSecurityEvent event)
{
    switch (event)
    {
    case LEAP_LOG_SEC_FRAME_SEQ_DUPLICATE:
        return "frame_seq_duplicate";
    case LEAP_LOG_SEC_FRAME_SEQ_GAP:
        return "frame_seq_gap";
    case LEAP_LOG_SEC_FRAME_SEQ_OUT_OF_WINDOW:
        return "frame_seq_out_of_window";
    case LEAP_LOG_SEC_FRAME_SEQ_SESSION_MISMATCH:
        return "frame_seq_session_mismatch";
    case LEAP_LOG_SEC_PD_NOT_OWNER:
        return "pd_not_owner";
    case LEAP_LOG_SEC_PD_STALE_FRAME:
        return "pd_stale_frame";
    default:
        return "unknown";
    }
}
