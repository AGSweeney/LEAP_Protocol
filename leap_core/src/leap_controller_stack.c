/*
 * leap_controller_stack.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_controller_stack.h"

#include "leap/leap_dir_controller_capabilities.h"
#include "leap/leap_disc_controller.h"
#include "leap/leap_log.h"

#include "leap/leap_controller_sequence.h"
#include "leap/leap_diag_controller.h"
#include "leap/leap_disc_controller.h"
#include "leap/leap_pd_common.h"

#include <stdio.h>
#include <string.h>

#define LEAP_CTRL_STACK_DEFAULT_LEASE_US     5000000u
#define LEAP_CTRL_STACK_DEFAULT_WATCHDOG_US  500000u
#define LEAP_CTRL_STACK_DEFAULT_RECV_MS      5000
#define LEAP_CTRL_STACK_RX_BUF               1600u
#define LEAP_CTRL_STACK_PAYLOAD_BUF          256u

static const uint8_t k_bcast[6] = LEAP_CTRL_STACK_BROADCAST_MAC;

static uint64_t leap_ctrl_stack_now_us(const LeapControllerStackIo* io);

static void leap_ctrl_stack_clear_event(LeapControllerStackEvent* event)
{
    if (event != NULL)
    {
        memset(event, 0, sizeof(*event));
        event->status = LEAP_CTRL_STACK_OK;
        event->phase  = LEAP_CTRL_STACK_IDLE;
    }
}

static uint32_t leap_ctrl_stack_profile_id(const LeapControllerStack* stack)
{
    if (stack->config.default_profile_id != 0u)
    {
        return stack->config.default_profile_id;
    }

    if (stack->mgmt.default_profile_id != 0u)
    {
        return stack->mgmt.default_profile_id;
    }

    return LEAP_PROFILE_DIGITAL_IO_16X16;
}

static LeapControllerStackStatus leap_ctrl_stack_send(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                dst_mac,
    uint8_t                       flags,
    uint16_t                      service_id,
    uint16_t                      message_type,
    uint32_t                      session_id,
    const uint8_t*                payload,
    size_t                        payload_length)
{
    uint32_t sequence;

    if (stack == NULL || io == NULL || dst_mac == NULL || io->send_frame == NULL)
    {
        return LEAP_CTRL_STACK_IO_MISSING;
    }

    sequence = leap_mgmt_controller_next_sequence(&stack->mgmt);

    if (io->send_frame(
            io->user_ctx,
            dst_mac,
            flags,
            service_id,
            message_type,
            session_id,
            sequence,
            stack->frame_seq.highest_peer_sequence,
            payload,
            payload_length) != 0)
    {
        return LEAP_CTRL_STACK_SEND_FAILED;
    }

    return LEAP_CTRL_STACK_OK;
}

static LeapControllerStackStatus leap_ctrl_stack_recv(
    const LeapControllerStackIo* io,
    int                            timeout_ms,
    uint8_t*                       src_mac,
    LeapFrameView*                 view,
    uint8_t*                       rx_buf,
    size_t                         rx_capacity,
    size_t*                        rx_length)
{
    if (io == NULL || io->recv_frame == NULL || view == NULL || rx_length == NULL)
    {
        return LEAP_CTRL_STACK_IO_MISSING;
    }

    *rx_length = 0u;

    if (io->recv_frame(
            io->user_ctx,
            src_mac,
            rx_buf,
            rx_capacity,
            rx_length,
            view,
            timeout_ms) != 0)
    {
        return LEAP_CTRL_STACK_RECV_TIMEOUT;
    }

    if (leap_frame_parse(rx_buf, *rx_length, view) != LEAP_FRAME_OK)
    {
        return LEAP_CTRL_STACK_UNEXPECTED_REPLY;
    }

    return LEAP_CTRL_STACK_OK;
}

static int leap_ctrl_stack_peer_matches(
    const LeapControllerStack* stack,
    const uint8_t*             src_mac)
{
    if (stack == NULL || src_mac == NULL)
    {
        return 0;
    }

    if (stack->peer_bound == 0)
    {
        return 1;
    }

    return (memcmp(stack->peer_mac, src_mac, 6) == 0) ? 1 : 0;
}

static LeapControllerStackStatus leap_ctrl_stack_recv_from_peer(
    LeapControllerStack*         stack,
    const LeapControllerStackIo* io,
    int                          timeout_ms,
    uint8_t*                     src_mac,
    LeapFrameView*               view,
    uint8_t*                     rx_buf,
    size_t                       rx_capacity,
    size_t*                      rx_length)
{
    uint64_t deadline_us = 0u;
    uint64_t start_us;

    if (stack == NULL || io == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    start_us = leap_ctrl_stack_now_us(io);
    if (start_us != 0u && timeout_ms > 0)
    {
        deadline_us = start_us + ((uint64_t)timeout_ms * 1000u);
    }

    for (;;)
    {
        int                       slice_ms = timeout_ms;
        LeapControllerStackStatus status;

        if (deadline_us != 0u)
        {
            uint64_t now_us = leap_ctrl_stack_now_us(io);

            if (now_us >= deadline_us)
            {
                return LEAP_CTRL_STACK_RECV_TIMEOUT;
            }

            slice_ms = (int)((deadline_us - now_us) / 1000u);
            if (slice_ms <= 0)
            {
                slice_ms = 1;
            }
            if (slice_ms > 100)
            {
                slice_ms = 100;
            }
        }

        status = leap_ctrl_stack_recv(
            io,
            slice_ms,
            src_mac,
            view,
            rx_buf,
            rx_capacity,
            rx_length);
        if (status != LEAP_CTRL_STACK_OK)
        {
            return status;
        }

        if (leap_ctrl_stack_peer_matches(stack, src_mac) != 0)
        {
            return LEAP_CTRL_STACK_OK;
        }

        /* Drop frames from other peers during multi-device bootstrap. */
    }
}

static LeapControllerStackStatus leap_ctrl_stack_recv_expected_from_peer(
    LeapControllerStack*         stack,
    const LeapControllerStackIo* io,
    int                          timeout_ms,
    uint16_t                     expect_service,
    uint16_t                     expect_message,
    uint8_t*                     src_mac,
    LeapFrameView*               view,
    uint8_t*                     rx_buf,
    size_t                       rx_capacity,
    size_t*                      rx_length)
{
    uint64_t deadline_us = 0u;
    uint64_t start_us    = leap_ctrl_stack_now_us(io);

    if (start_us != 0u && timeout_ms > 0)
    {
        deadline_us = start_us + ((uint64_t)timeout_ms * 1000u);
    }

    for (;;)
    {
        LeapControllerStackStatus status;
        int                       remaining_ms = timeout_ms;

        if (deadline_us != 0u)
        {
            uint64_t now_us = leap_ctrl_stack_now_us(io);

            if (now_us >= deadline_us)
            {
                return LEAP_CTRL_STACK_RECV_TIMEOUT;
            }

            remaining_ms = (int)((deadline_us - now_us) / 1000u);
            if (remaining_ms <= 0)
            {
                remaining_ms = 1;
            }
            if (remaining_ms > 100)
            {
                remaining_ms = 100;
            }
        }

        status = leap_ctrl_stack_recv_from_peer(
            stack,
            io,
            remaining_ms,
            src_mac,
            view,
            rx_buf,
            rx_capacity,
            rx_length);
        if (status != LEAP_CTRL_STACK_OK)
        {
            return status;
        }

        if (view->header.service_id == expect_service)
        {
            if (view->header.message_type == expect_message)
            {
                return LEAP_CTRL_STACK_OK;
            }

            if ((view->header.flags & LEAP_FLAG_ERROR) != 0u)
            {
                if (expect_service == (uint16_t)LEAP_SERVICE_DIR)
                {
                    return LEAP_CTRL_STACK_DIR_ERROR;
                }
                if (expect_service == (uint16_t)LEAP_SERVICE_MGMT)
                {
                    return LEAP_CTRL_STACK_MGMT_ERROR;
                }
                if (expect_service == (uint16_t)LEAP_SERVICE_DISC)
                {
                    return LEAP_CTRL_STACK_DISC_ERROR;
                }
                if (expect_service == (uint16_t)LEAP_SERVICE_DIAG)
                {
                    return LEAP_CTRL_STACK_DIAG_ERROR;
                }
            }
        }

        /* Stale PD/MGMT/async traffic from peer — keep waiting. */
    }
}

static void leap_ctrl_stack_on_op_entered(LeapControllerStack* stack)
{
    if (stack == NULL)
    {
        return;
    }

    /*
     * Multi-peer: bind frame-sequence state to this owner session so replies
     * from a stale or foreign session_id are dropped in on_frame().
     */
    leap_controller_frame_sequence_bind_session(
        &stack->frame_seq,
        leap_mgmt_controller_session_id(&stack->mgmt));
}

static LeapControllerStackStatus leap_ctrl_stack_track_sequence(
    LeapControllerStack*      stack,
    uint32_t                  frame_session_id,
    uint32_t                  sequence,
    LeapControllerStackEvent* event)
{
    LeapControllerFrameSequenceResult seq_result;
    uint32_t                          gaps_before;

    if (stack == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    gaps_before = stack->frame_seq.sequence_gaps;

    seq_result = leap_controller_frame_sequence_accept(
        &stack->frame_seq,
        &stack->config.frame_sequence,
        frame_session_id,
        sequence);

    if (seq_result == LEAP_CTRL_FRAME_SEQ_DUPLICATE)
    {
        if (event != NULL)
        {
            event->flags |= LEAP_CTRL_STACK_FLAG_DUPLICATE_FRAME;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if (seq_result == LEAP_CTRL_FRAME_SEQ_SESSION_MISMATCH)
    {
        if (event != NULL)
        {
            event->flags |= LEAP_CTRL_STACK_FLAG_SESSION_MISMATCH;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if (seq_result == LEAP_CTRL_FRAME_SEQ_OUT_OF_WINDOW)
    {
        if (event != NULL)
        {
            event->flags |= LEAP_CTRL_STACK_FLAG_FAULT;
        }
        return LEAP_CTRL_STACK_UNEXPECTED_REPLY;
    }

    if (seq_result == LEAP_CTRL_FRAME_SEQ_GAP)
    {
        if (event != NULL)
        {
            event->flags |= LEAP_CTRL_STACK_FLAG_FAULT;
        }
        return LEAP_CTRL_STACK_UNEXPECTED_REPLY;
    }

    if (seq_result == LEAP_CTRL_FRAME_SEQ_OK &&
        stack->frame_seq.sequence_gaps > gaps_before && event != NULL)
    {
        event->flags |= LEAP_CTRL_STACK_FLAG_SEQUENCE_GAP;
    }

    return LEAP_CTRL_STACK_OK;
}

static void leap_ctrl_stack_set_fault(
    LeapControllerStack*      stack,
    LeapControllerStackStatus status,
    uint16_t                  error_code,
    LeapControllerStackEvent* event)
{
    if (stack == NULL)
    {
        return;
    }

    stack->phase       = LEAP_CTRL_STACK_FAULT;
    stack->last_status = status;

    if (event != NULL)
    {
        event->status     = status;
        event->phase      = stack->phase;
        event->error_code = error_code;
        event->flags     |= LEAP_CTRL_STACK_FLAG_FAULT;
    }
}

static LeapControllerStackStatus leap_ctrl_stack_handle_error_frame(
    LeapControllerStack*          stack,
    const LeapFrameView*          view,
    LeapControllerStackEvent*     event)
{
    const LeapErrorPayload* err;

    if (view->payload_length < sizeof(LeapErrorPayload))
    {
        leap_ctrl_stack_set_fault(
            stack,
            LEAP_CTRL_STACK_UNEXPECTED_REPLY,
            (uint16_t)LEAP_STATUS_BAD_LENGTH,
            event);
        return LEAP_CTRL_STACK_UNEXPECTED_REPLY;
    }

    err = (const LeapErrorPayload*)view->payload;
    leap_ctrl_stack_set_fault(
        stack,
        LEAP_CTRL_STACK_MGMT_ERROR,
        err->status_code,
        event);
    return LEAP_CTRL_STACK_MGMT_ERROR;
}

static uint64_t leap_ctrl_stack_now_us(const LeapControllerStackIo* io)
{
    if (io != NULL && io->monotonic_us != NULL)
    {
        return io->monotonic_us(io->user_ctx);
    }

    return 0u;
}

static int leap_ctrl_stack_target_peer_set(const LeapControllerStack* stack)
{
    static const uint8_t k_zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };

    if (stack == NULL)
    {
        return 0;
    }

    return (memcmp(stack->config.target_peer_mac, k_zero, 6) != 0) ? 1 : 0;
}

static int leap_ctrl_stack_peer_accept(
    const LeapControllerStack* stack,
    const uint8_t*             src_mac)
{
    if (stack == NULL || src_mac == NULL)
    {
        return 0;
    }

    if (stack->config.single_peer_auto_select != 0)
    {
        return 1;
    }

    if (leap_ctrl_stack_target_peer_set(stack) == 0)
    {
        return 1;
    }

    return (memcmp(stack->config.target_peer_mac, src_mac, 6) == 0) ? 1 : 0;
}

static int leap_ctrl_stack_mac_is_zero(const uint8_t* mac)
{
    static const uint8_t k_zero[6] = { 0, 0, 0, 0, 0, 0 };

    if (mac == NULL)
    {
        return 1;
    }

    return (memcmp(mac, k_zero, 6) == 0) ? 1 : 0;
}

static LeapControllerStackStatus leap_ctrl_stack_send_open_session(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    LeapControllerStackEvent*     event,
    uint16_t                      extra_open_flags)
{
    uint8_t                   payload[LEAP_CTRL_STACK_PAYLOAD_BUF];
    size_t                    payload_length;
    LeapControllerStackStatus send_status;

    payload_length = leap_mgmt_controller_build_open_session(
        &stack->mgmt,
        payload,
        sizeof(payload),
        stack->config.bootstrap_lease_us,
        stack->config.bootstrap_watchdog_us,
        extra_open_flags);
    if (payload_length == 0u)
    {
        stack->phase       = LEAP_CTRL_STACK_FAULT;
        stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_MGMT_ERROR;
            event->phase  = stack->phase;
        }
        return LEAP_CTRL_STACK_MGMT_ERROR;
    }

    send_status = leap_ctrl_stack_send(
        stack,
        io,
        stack->peer_mac,
        LEAP_FLAG_ACK_REQUESTED,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_OPEN_SESSION,
        0u,
        payload,
        payload_length);
    if (send_status != LEAP_CTRL_STACK_OK)
    {
        stack->phase       = LEAP_CTRL_STACK_FAULT;
        stack->last_status = send_status;
        if (event != NULL)
        {
            event->status = send_status;
            event->phase  = stack->phase;
        }
        return send_status;
    }

    stack->phase = LEAP_CTRL_STACK_OPEN_SESSION;
    if (event != NULL)
    {
        event->status = LEAP_CTRL_STACK_OK;
        event->phase  = stack->phase;
        if (extra_open_flags == 0u)
        {
            event->flags |= LEAP_CTRL_STACK_FLAG_PROFILE_SELECTED;
        }
        else
        {
            event->flags |= LEAP_CTRL_STACK_FLAG_PEER_DISCOVERED;
        }
    }

    return LEAP_CTRL_STACK_OK;
}

static int leap_ctrl_stack_should_skip_select_profile(
    const LeapHelloReply* hello,
    const uint8_t*        controller_mac)
{
    if (hello == NULL)
    {
        return 0;
    }

    if (hello->current_state == (uint16_t)LEAP_STATE_SAFE)
    {
        return 1;
    }

    if (hello->current_state == (uint16_t)LEAP_STATE_CONFIGURED &&
        controller_mac != NULL &&
        !leap_ctrl_stack_mac_is_zero(hello->active_owner_mac) &&
        memcmp(hello->active_owner_mac, controller_mac, 6) == 0)
    {
        /*
         * A controller reboot/drop can leave a device in CONFIGURED while it
         * still advertises our owner MAC. Skip SELECT_PROFILE and reopen with
         * reboot recovery so the stale owner/session is replaced cleanly.
         */
        return 1;
    }

    if (hello->current_state == (uint16_t)LEAP_STATE_OP)
    {
        if (leap_ctrl_stack_mac_is_zero(hello->active_owner_mac))
        {
            return 1;
        }

        if (controller_mac != NULL &&
            memcmp(hello->active_owner_mac, controller_mac, 6) == 0)
        {
            return 1;
        }

        return 0;
    }

    return 0;
}

static uint16_t leap_ctrl_stack_reconnect_open_flags(
    const LeapHelloReply* hello,
    const uint8_t*        controller_mac)
{
    if (hello == NULL)
    {
        return 0u;
    }

    if (hello->current_state != (uint16_t)LEAP_STATE_OP &&
        hello->current_state != (uint16_t)LEAP_STATE_CONFIGURED)
    {
        return 0u;
    }

    if (leap_ctrl_stack_mac_is_zero(hello->active_owner_mac))
    {
        return (uint16_t)LEAP_OPEN_FLAG_REBOOT_RECOVERY;
    }

    if (controller_mac != NULL &&
        memcmp(hello->active_owner_mac, controller_mac, 6) == 0)
    {
        return (uint16_t)LEAP_OPEN_FLAG_REBOOT_RECOVERY;
    }

    return 0u;
}

static uint16_t leap_ctrl_stack_bootstrap_open_flags(
    const LeapHelloReply* hello,
    const uint8_t*        controller_mac)
{
    uint16_t flags;

    flags = leap_ctrl_stack_reconnect_open_flags(hello, controller_mac);
    if (hello != NULL && hello->current_state == (uint16_t)LEAP_STATE_SAFE)
    {
        flags |= (uint16_t)LEAP_OPEN_FLAG_STEAL_EXPIRED;
    }

    return flags;
}

static LeapControllerStackStatus leap_ctrl_stack_send_select_profile(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    LeapControllerStackEvent*     event)
{
    uint8_t                   payload[LEAP_CTRL_STACK_PAYLOAD_BUF];
    size_t                    payload_length;
    LeapControllerStackStatus send_status;
    uint32_t                  profile_id;

    profile_id     = leap_ctrl_stack_profile_id(stack);
    payload_length = leap_dir_controller_build_select_profile(
        payload,
        sizeof(payload),
        profile_id,
        0u);
    if (payload_length == 0u)
    {
        stack->phase       = LEAP_CTRL_STACK_FAULT;
        stack->last_status = LEAP_CTRL_STACK_DIR_ERROR;
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_DIR_ERROR;
            event->phase  = stack->phase;
        }
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    send_status = leap_ctrl_stack_send(
        stack,
        io,
        stack->peer_mac,
        LEAP_FLAG_ACK_REQUESTED,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_SELECT_PROFILE,
        0u,
        payload,
        payload_length);
    if (send_status != LEAP_CTRL_STACK_OK)
    {
        stack->phase       = LEAP_CTRL_STACK_FAULT;
        stack->last_status = send_status;
        if (event != NULL)
        {
            event->status = send_status;
            event->phase  = stack->phase;
        }
        return send_status;
    }

    stack->phase = LEAP_CTRL_STACK_SELECT_PROFILE;
    if (event != NULL)
    {
        event->status = LEAP_CTRL_STACK_OK;
        event->phase  = stack->phase;
        event->flags |= LEAP_CTRL_STACK_FLAG_PEER_DISCOVERED;
    }

    return LEAP_CTRL_STACK_OK;
}

static LeapControllerStackStatus leap_ctrl_stack_on_hello_reply(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                src_mac,
    const LeapFrameView*          view,
    LeapControllerStackEvent*     event)
{
    LeapMgmtControllerEvent mgmt_event;

    if (leap_mgmt_controller_on_hello_reply(
            &stack->mgmt,
            src_mac,
            view->payload,
            view->payload_length,
            &mgmt_event) != LEAP_MGMT_CTRL_OK)
    {
        stack->phase       = LEAP_CTRL_STACK_FAULT;
        stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_MGMT_ERROR;
            event->phase  = stack->phase;
        }
        return LEAP_CTRL_STACK_MGMT_ERROR;
    }

    memcpy(stack->peer_mac, src_mac, 6);
    stack->peer_bound = 1;

    if (view->payload_length >= sizeof(LeapHelloReply))
    {
        const LeapHelloReply* hello = (const LeapHelloReply*)view->payload;

        if (hello->active_profile_id != 0u)
        {
            (void)leap_pd_profile_map_from_profile_id(
                hello->active_profile_id,
                &stack->pd.config.profile);
        }
    }

    if (view->payload_length < sizeof(LeapHelloReply))
    {
        return leap_ctrl_stack_send_select_profile(stack, io, event);
    }

    {
        const LeapHelloReply* hello = (const LeapHelloReply*)view->payload;

        if (leap_ctrl_stack_should_skip_select_profile(
                hello,
                stack->config.mgmt.controller_mac) != 0)
        {
            return leap_ctrl_stack_send_open_session(
                stack,
                io,
                event,
                leap_ctrl_stack_bootstrap_open_flags(
                    hello,
                    stack->config.mgmt.controller_mac));
        }

        if (hello->current_state == (uint16_t)LEAP_STATE_OP)
        {
            return leap_ctrl_stack_send_open_session(
                stack,
                io,
                event,
                (uint16_t)(LEAP_OPEN_FLAG_REQUEST_OWNER |
                             LEAP_OPEN_FLAG_STEAL_EXPIRED));
        }
    }

    return leap_ctrl_stack_send_select_profile(stack, io, event);
}

static const char* leap_ctrl_stack_phase_label(LeapControllerStackPhase phase)
{
    switch (phase)
    {
    case LEAP_CTRL_STACK_IDLE:
        return "IDLE";
    case LEAP_CTRL_STACK_DISCOVERING:
        return "DISCOVERING";
    case LEAP_CTRL_STACK_SELECT_PROFILE:
        return "SELECT_PROFILE";
    case LEAP_CTRL_STACK_OPEN_SESSION:
        return "OPEN_SESSION";
    case LEAP_CTRL_STACK_SET_STATE:
        return "SET_STATE";
    case LEAP_CTRL_STACK_OP:
        return "OP";
    case LEAP_CTRL_STACK_FAULT:
        return "FAULT";
    default:
        return "?";
    }
}

static LeapControllerStackStatus leap_ctrl_stack_run_until_op(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    uint8_t*                      peer_mac_out)
{
    LeapControllerStackEvent  event;
    LeapControllerStackStatus status;
    unsigned                  guard = 0u;
    uint64_t                  start_us = leap_ctrl_stack_now_us(io);
    LeapControllerStackPhase  last_logged_phase = LEAP_CTRL_STACK_FAULT;

    while (stack->phase != LEAP_CTRL_STACK_OP &&
           stack->phase != LEAP_CTRL_STACK_FAULT)
    {
        LeapControllerStackPhase phase_before = stack->phase;

        status = leap_controller_stack_step(stack, io, &event);
        if (status != LEAP_CTRL_STACK_OK)
        {
            return status;
        }

        if (stack->phase != phase_before &&
            stack->phase != last_logged_phase)
        {
            unsigned elapsed_ms = 0u;

            if (start_us != 0u)
            {
                uint64_t now_us = leap_ctrl_stack_now_us(io);

                if (now_us >= start_us)
                {
                    elapsed_ms = (unsigned)((now_us - start_us) / 1000u);
                }
            }

            leap_log_printf(
                "Bootstrap: phase %s (+ %u ms)\n",
                leap_ctrl_stack_phase_label(stack->phase),
                elapsed_ms);
            last_logged_phase = stack->phase;
        }

        guard++;
        if (guard > 16u)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_ABORTED;
            return LEAP_CTRL_STACK_ABORTED;
        }
    }

    if (stack->phase == LEAP_CTRL_STACK_FAULT)
    {
        return LEAP_CTRL_STACK_ABORTED;
    }

    if (start_us != 0u)
    {
        uint64_t now_us = leap_ctrl_stack_now_us(io);

        if (now_us >= start_us)
        {
            leap_log_printf(
                "Bootstrap: OP reached in %u ms\n",
                (unsigned)((now_us - start_us) / 1000u));
        }
    }

    if (peer_mac_out != NULL && stack->peer_bound != 0)
    {
        memcpy(peer_mac_out, stack->peer_mac, 6);
    }

    return LEAP_CTRL_STACK_OK;
}

void leap_controller_stack_init(
    LeapControllerStack*             stack,
    const LeapControllerStackConfig* config)
{
    if (stack == NULL)
    {
        return;
    }

    memset(stack, 0, sizeof(*stack));
    stack->phase       = LEAP_CTRL_STACK_IDLE;
    stack->last_status = LEAP_CTRL_STACK_OK;

    if (config != NULL)
    {
        stack->config = *config;
    }

    if (stack->config.bootstrap_lease_us == 0u)
    {
        stack->config.bootstrap_lease_us = LEAP_CTRL_STACK_DEFAULT_LEASE_US;
    }

    if (stack->config.bootstrap_watchdog_us == 0u)
    {
        stack->config.bootstrap_watchdog_us = LEAP_CTRL_STACK_DEFAULT_WATCHDOG_US;
    }

    if (stack->config.recv_timeout_ms == 0)
    {
        stack->config.recv_timeout_ms = LEAP_CTRL_STACK_DEFAULT_RECV_MS;
    }

    if (stack->config.single_peer_auto_select == 0 &&
        leap_ctrl_stack_target_peer_set(stack) == 0)
    {
        stack->config.single_peer_auto_select = 1;
    }

    leap_mgmt_controller_init(&stack->mgmt, &stack->config.mgmt);
    leap_pd_controller_init(&stack->pd, &stack->config.pd);
    leap_controller_frame_sequence_init(&stack->frame_seq);

    if (config == NULL)
    {
        stack->config.frame_sequence.enforce_session_match = 1;
    }
}

void leap_controller_stack_reset(LeapControllerStack* stack)
{
    LeapControllerStackConfig saved;

    if (stack == NULL)
    {
        return;
    }

    saved = stack->config;
    leap_controller_stack_init(stack, &saved);
}

LeapControllerStackPhase leap_controller_stack_get_phase(
    const LeapControllerStack* stack)
{
    if (stack == NULL)
    {
        return LEAP_CTRL_STACK_FAULT;
    }

    return stack->phase;
}

LeapControllerStackStatus leap_controller_stack_step(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    LeapControllerStackEvent*     event)
{
    uint8_t                       payload[LEAP_CTRL_STACK_PAYLOAD_BUF];
    uint8_t                       rx[LEAP_CTRL_STACK_RX_BUF];
    uint8_t                       src_mac[6];
    size_t                        payload_length;
    size_t                        rx_length;
    LeapFrameView                 view;
    LeapMgmtControllerEvent       mgmt_event;
    LeapControllerStackStatus     status;
    LeapControllerStackStatus     send_status;

    leap_ctrl_stack_clear_event(event);

    if (stack == NULL || io == NULL)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_INVALID_ARG;
        }
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    if (event != NULL)
    {
        event->phase = stack->phase;
    }

    if (stack->phase == LEAP_CTRL_STACK_FAULT)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_ABORTED;
        }
        return LEAP_CTRL_STACK_ABORTED;
    }

    if (stack->phase == LEAP_CTRL_STACK_OP)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_OK;
            event->phase  = LEAP_CTRL_STACK_OP;
        }
        return LEAP_CTRL_STACK_OK;
    }

    switch (stack->phase)
    {
    case LEAP_CTRL_STACK_IDLE:
        payload_length = leap_disc_controller_build_hello(payload, sizeof(payload));
        if (payload_length == 0u)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_DISC_ERROR;
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_DISC_ERROR;
                event->phase  = stack->phase;
            }
            return LEAP_CTRL_STACK_DISC_ERROR;
        }

        send_status = leap_ctrl_stack_send(
            stack,
            io,
            k_bcast,
            LEAP_FLAG_BROADCAST,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            payload,
            payload_length);
        if (send_status != LEAP_CTRL_STACK_OK)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = send_status;
            if (event != NULL)
            {
                event->status = send_status;
                event->phase  = stack->phase;
            }
            return send_status;
        }

        stack->phase = LEAP_CTRL_STACK_DISCOVERING;
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_OK;
            event->phase  = stack->phase;
        }
        return LEAP_CTRL_STACK_OK;

    case LEAP_CTRL_STACK_DISCOVERING:
    {
        uint64_t deadline_us = 0u;
        uint64_t start_us    = leap_ctrl_stack_now_us(io);
        uint64_t last_hello_us = start_us;

        if (start_us != 0u)
        {
            deadline_us = start_us +
                          ((uint64_t)stack->config.recv_timeout_ms * 1000u);
        }

        for (;;)
        {
            int recv_timeout_ms = stack->config.recv_timeout_ms;

            if (deadline_us != 0u)
            {
                uint64_t now_us = leap_ctrl_stack_now_us(io);

                if (now_us >= deadline_us)
                {
                    stack->last_status = LEAP_CTRL_STACK_RECV_TIMEOUT;
                    if (event != NULL)
                    {
                        event->status = LEAP_CTRL_STACK_RECV_TIMEOUT;
                        event->phase  = stack->phase;
                    }
                    return LEAP_CTRL_STACK_RECV_TIMEOUT;
                }

                if (now_us != 0u && (now_us - last_hello_us) >= 250000u)
                {
                    payload_length = leap_disc_controller_build_hello(
                        payload,
                        sizeof(payload));
                    if (payload_length != 0u)
                    {
                        (void)leap_ctrl_stack_send(
                            stack,
                            io,
                            k_bcast,
                            LEAP_FLAG_BROADCAST,
                            (uint16_t)LEAP_SERVICE_DISC,
                            LEAP_DISC_HELLO,
                            0u,
                            payload,
                            payload_length);
                    }
                    last_hello_us = now_us;
                }

                recv_timeout_ms = (int)((deadline_us - now_us) / 1000u);
                if (recv_timeout_ms <= 0)
                {
                    recv_timeout_ms = 1;
                }
                if (recv_timeout_ms > 100)
                {
                    recv_timeout_ms = 100;
                }
            }

            status = leap_ctrl_stack_recv(
                io,
                recv_timeout_ms,
                src_mac,
                &view,
                rx,
                sizeof(rx),
                &rx_length);
            if (status != LEAP_CTRL_STACK_OK)
            {
                if (deadline_us != 0u)
                {
                    continue;
                }

                stack->last_status = status;
                if (event != NULL)
                {
                    event->status = status;
                    event->phase  = stack->phase;
                }
                return status;
            }

            if (view.header.service_id != (uint16_t)LEAP_SERVICE_DISC ||
                view.header.message_type != LEAP_DISC_HELLO_REPLY)
            {
                continue;
            }

            if (leap_ctrl_stack_peer_accept(stack, src_mac) == 0)
            {
                continue;
            }

            return leap_ctrl_stack_on_hello_reply(
                stack,
                io,
                src_mac,
                &view,
                event);
        }
    }
    break;

    case LEAP_CTRL_STACK_SELECT_PROFILE:
        status = leap_ctrl_stack_recv_expected_from_peer(
            stack,
            io,
            stack->config.recv_timeout_ms,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_PROFILE_REPLY,
            src_mac,
            &view,
            rx,
            sizeof(rx),
            &rx_length);
        if (status != LEAP_CTRL_STACK_OK)
        {
            stack->last_status = status;
            if (event != NULL)
            {
                event->status = status;
                event->phase  = stack->phase;
            }
            return status;
        }

        if (leap_dir_controller_on_profile_reply(
                view.payload,
                view.payload_length,
                event != NULL ? &event->profile_info : NULL) != LEAP_DIR_CTRL_OK)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_DIR_ERROR;
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_DIR_ERROR;
                event->phase  = stack->phase;
            }
            return LEAP_CTRL_STACK_DIR_ERROR;
        }

        return leap_ctrl_stack_send_open_session(stack, io, event, 0u);

    case LEAP_CTRL_STACK_OPEN_SESSION:
        status = leap_ctrl_stack_recv_expected_from_peer(
            stack,
            io,
            stack->config.recv_timeout_ms,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_OPEN_SESSION_REPLY,
            src_mac,
            &view,
            rx,
            sizeof(rx),
            &rx_length);
        if (status != LEAP_CTRL_STACK_OK)
        {
            stack->last_status = status;
            if (event != NULL)
            {
                event->status = status;
                event->phase  = stack->phase;
            }
            return status;
        }

        if (leap_mgmt_controller_on_mgmt_reply(
                &stack->mgmt, &view, &mgmt_event) != LEAP_MGMT_CTRL_OK)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_MGMT_ERROR;
                event->phase  = stack->phase;
            }
            return LEAP_CTRL_STACK_MGMT_ERROR;
        }

        if (stack->mgmt.peer_device_state == (uint16_t)LEAP_STATE_OP)
        {
            stack->mgmt.state  = LEAP_MGMT_CTRL_OP;
            stack->phase       = LEAP_CTRL_STACK_OP;
            stack->last_status = LEAP_CTRL_STACK_OK;
            leap_ctrl_stack_on_op_entered(stack);
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_OK;
                event->phase  = stack->phase;
                event->flags |= LEAP_CTRL_STACK_FLAG_OP_ENTERED;
            }
            return LEAP_CTRL_STACK_OK;
        }

        payload_length = leap_mgmt_controller_build_set_state(
            &stack->mgmt,
            payload,
            sizeof(payload),
            LEAP_STATE_OP);
        if (payload_length == 0u)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_MGMT_ERROR;
                event->phase  = stack->phase;
            }
            return LEAP_CTRL_STACK_MGMT_ERROR;
        }

        send_status = leap_ctrl_stack_send(
            stack,
            io,
            stack->peer_mac,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_SET_STATE,
            leap_mgmt_controller_session_id(&stack->mgmt),
            payload,
            payload_length);
        if (send_status != LEAP_CTRL_STACK_OK)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = send_status;
            if (event != NULL)
            {
                event->status = send_status;
                event->phase  = stack->phase;
            }
            return send_status;
        }

        stack->phase = LEAP_CTRL_STACK_SET_STATE;
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_OK;
            event->phase  = stack->phase;
            event->flags |= LEAP_CTRL_STACK_FLAG_SESSION_OPENED;
        }
        return LEAP_CTRL_STACK_OK;

    case LEAP_CTRL_STACK_SET_STATE:
        status = leap_ctrl_stack_recv_expected_from_peer(
            stack,
            io,
            stack->config.recv_timeout_ms,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            src_mac,
            &view,
            rx,
            sizeof(rx),
            &rx_length);
        if (status != LEAP_CTRL_STACK_OK)
        {
            stack->last_status = status;
            if (event != NULL)
            {
                event->status = status;
                event->phase  = stack->phase;
            }
            return status;
        }

        if (leap_mgmt_controller_on_mgmt_reply(
                &stack->mgmt, &view, &mgmt_event) != LEAP_MGMT_CTRL_OK)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_MGMT_ERROR;
                event->phase  = stack->phase;
            }
            return LEAP_CTRL_STACK_MGMT_ERROR;
        }

        if (leap_mgmt_controller_get_state(&stack->mgmt) != LEAP_MGMT_CTRL_OP)
        {
            stack->phase       = LEAP_CTRL_STACK_FAULT;
            stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
            if (event != NULL)
            {
                event->status = LEAP_CTRL_STACK_MGMT_ERROR;
                event->phase  = stack->phase;
            }
            return LEAP_CTRL_STACK_MGMT_ERROR;
        }

        stack->phase       = LEAP_CTRL_STACK_OP;
        stack->last_status = LEAP_CTRL_STACK_OK;
        leap_ctrl_stack_on_op_entered(stack);
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_OK;
            event->phase  = stack->phase;
            event->flags |= LEAP_CTRL_STACK_FLAG_OP_ENTERED;
        }
        return LEAP_CTRL_STACK_OK;

    default:
        stack->phase       = LEAP_CTRL_STACK_FAULT;
        stack->last_status = LEAP_CTRL_STACK_ABORTED;
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_ABORTED;
            event->phase  = stack->phase;
        }
        return LEAP_CTRL_STACK_ABORTED;
    }
}

LeapControllerStackStatus leap_controller_stack_on_frame(
    LeapControllerStack*      stack,
    const uint8_t*            src_mac,
    const LeapFrameView*      view,
    LeapControllerStackEvent* event)
{
    LeapMgmtControllerEvent mgmt_event;
    LeapControllerStackStatus seq_status;

    leap_ctrl_stack_clear_event(event);

    if (stack == NULL || src_mac == NULL || view == NULL)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_INVALID_ARG;
        }
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    if (event != NULL)
    {
        event->phase = stack->phase;
    }

    if (stack->phase == LEAP_CTRL_STACK_FAULT)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_ABORTED;
        }
        return LEAP_CTRL_STACK_ABORTED;
    }

    if (stack->phase == LEAP_CTRL_STACK_IDLE)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if (leap_ctrl_stack_peer_matches(stack, src_mac) == 0)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    seq_status = leap_ctrl_stack_track_sequence(
        stack,
        view->header.session_id,
        view->header.sequence,
        event);
    if (seq_status == LEAP_CTRL_STACK_IGNORED)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if ((view->header.flags & LEAP_FLAG_ERROR) != 0u)
    {
        return leap_ctrl_stack_handle_error_frame(stack, view, event);
    }

    if (stack->phase != LEAP_CTRL_STACK_OP)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if (view->header.service_id != (uint16_t)LEAP_SERVICE_MGMT)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if ((view->header.flags & LEAP_FLAG_RESPONSE) == 0u)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    if (leap_mgmt_controller_on_mgmt_reply(&stack->mgmt, view, &mgmt_event) !=
        LEAP_MGMT_CTRL_OK)
    {
        stack->last_status = LEAP_CTRL_STACK_MGMT_ERROR;
        if (event != NULL)
        {
            event->status     = LEAP_CTRL_STACK_MGMT_ERROR;
            event->error_code = (uint16_t)mgmt_event.error_code;
            event->flags     |= LEAP_CTRL_STACK_FLAG_MGMT_PROCESSED;
        }
        return LEAP_CTRL_STACK_MGMT_ERROR;
    }

    if (event != NULL)
    {
        event->status = LEAP_CTRL_STACK_OK;
        event->phase  = stack->phase;
        event->flags |= LEAP_CTRL_STACK_FLAG_MGMT_PROCESSED;
    }

    return LEAP_CTRL_STACK_OK;
}

LeapControllerStackStatus leap_controller_stack_release(
    LeapControllerStack*         stack,
    const LeapControllerStackIo* io)
{
    uint8_t                   payload[LEAP_CTRL_STACK_PAYLOAD_BUF];
    size_t                    payload_length;
    LeapControllerStackStatus send_status;
    uint32_t                  safe_profile_id;

    if (stack == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    if (stack->mgmt.session_id == 0u || stack->peer_bound == 0)
    {
        leap_controller_stack_reset(stack);
        return LEAP_CTRL_STACK_OK;
    }

    if (io == NULL || io->send_frame == NULL)
    {
        leap_controller_stack_reset(stack);
        return LEAP_CTRL_STACK_IO_MISSING;
    }

    safe_profile_id = stack->config.release_safe_profile_id;

    payload_length = leap_mgmt_controller_build_owner_release(
        &stack->mgmt,
        payload,
        sizeof(payload),
        safe_profile_id);
    if (payload_length == 0u)
    {
        leap_controller_stack_reset(stack);
        return LEAP_CTRL_STACK_MGMT_ERROR;
    }

    send_status = leap_ctrl_stack_send(
        stack,
        io,
        stack->peer_mac,
        LEAP_FLAG_ACK_REQUESTED,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_OWNER_RELEASE,
        leap_mgmt_controller_session_id(&stack->mgmt),
        payload,
        payload_length);

    leap_controller_stack_reset(stack);

    return send_status;
}

LeapControllerStackStatus leap_controller_stack_bootstrap(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    uint8_t*                      peer_mac_out)
{
    if (stack == NULL || io == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    return leap_ctrl_stack_run_until_op(stack, io, peer_mac_out);
}

LeapControllerStackStatus leap_controller_stack_bootstrap_peer(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    const LeapHelloReply*         hello_reply)
{
    LeapHelloReply            default_hello;
    LeapControllerStackEvent  event;
    LeapControllerStackStatus status;
    uint8_t                   hello_bytes[sizeof(LeapHelloReply)];

    if (stack == NULL || io == NULL || peer_mac == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    leap_controller_stack_reset(stack);

    if (hello_reply != NULL)
    {
        memcpy(hello_bytes, hello_reply, sizeof(hello_bytes));
    }
    else
    {
        memset(&default_hello, 0, sizeof(default_hello));
        default_hello.current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
        default_hello.active_profile_id  = leap_ctrl_stack_profile_id(stack);
        default_hello.default_profile_id = default_hello.active_profile_id;
        memcpy(hello_bytes, &default_hello, sizeof(hello_bytes));
    }

    {
        LeapFrameView synthetic;

        memset(&synthetic, 0, sizeof(synthetic));
        synthetic.payload         = hello_bytes;
        synthetic.payload_length  = sizeof(hello_bytes);
        synthetic.header.service_id = (uint16_t)LEAP_SERVICE_DISC;

        status = leap_ctrl_stack_on_hello_reply(
            stack,
            io,
            peer_mac,
            &synthetic,
            &event);
        if (status != LEAP_CTRL_STACK_OK)
        {
            return status;
        }
    }

    return leap_ctrl_stack_run_until_op(stack, io, NULL);
}

static int leap_ctrl_stack_peer_ready(const LeapControllerStack* stack)
{
    if (stack == NULL || stack->peer_bound == 0)
    {
        return 0;
    }

    return (stack->phase == LEAP_CTRL_STACK_OP) ? 1 : 0;
}

LeapPdControllerStatus leap_controller_stack_run_cyclic_pd(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    volatile int*             stop_flag)
{
    if (stack == NULL || pd_io == NULL || stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (leap_ctrl_stack_peer_ready(stack) == 0)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    return leap_pd_controller_run_cyclic(
        &stack->pd,
        &stack->mgmt,
        pd_io,
        stack->peer_mac,
        stop_flag);
}

LeapPdControllerStatus leap_controller_stack_pd_single_write(
    LeapControllerStack*      stack,
    const LeapPdControllerIo* pd_io,
    uint16_t                  digital_outputs)
{
    if (stack == NULL || pd_io == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (leap_ctrl_stack_peer_ready(stack) == 0)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    return leap_pd_controller_send_single_write(
        &stack->pd,
        &stack->mgmt,
        pd_io,
        stack->peer_mac,
        digital_outputs);
}

static LeapControllerStackDiagStatus leap_ctrl_stack_diag_map_status(
    LeapControllerStackStatus status)
{
    switch (status)
    {
    case LEAP_CTRL_STACK_OK:
        return LEAP_CTRL_STACK_DIAG_OK;
    case LEAP_CTRL_STACK_INVALID_ARG:
        return LEAP_CTRL_STACK_DIAG_INVALID_ARG;
    case LEAP_CTRL_STACK_IO_MISSING:
        return LEAP_CTRL_STACK_DIAG_IO_MISSING;
    case LEAP_CTRL_STACK_SEND_FAILED:
        return LEAP_CTRL_STACK_DIAG_SEND_FAILED;
    case LEAP_CTRL_STACK_RECV_TIMEOUT:
        return LEAP_CTRL_STACK_DIAG_RECV_TIMEOUT;
    default:
        return LEAP_CTRL_STACK_DIAG_UNEXPECTED_REPLY;
    }
}

static LeapControllerStackDiagStatus leap_ctrl_stack_diag_exchange(
    LeapControllerStack*           stack,
    const LeapControllerStackIo*   io,
    uint16_t                       request_type,
    const uint8_t*                 request_payload,
    size_t                         request_length,
    uint16_t                       expected_reply_type,
    uint8_t*                       reply_payload,
    size_t                         reply_capacity,
    size_t*                        reply_length_out)
{
    uint8_t         rx_buf[LEAP_CTRL_STACK_RX_BUF];
    uint8_t         src_mac[6];
    LeapFrameView   view;
    size_t          rx_length;
    LeapControllerStackStatus status;
    unsigned        attempt;
    uint32_t        session_id;
    uint32_t        sequence;
    int             timeout_ms;

    if (stack == NULL || io == NULL || request_payload == NULL ||
        reply_payload == NULL || reply_length_out == NULL)
    {
        return LEAP_CTRL_STACK_DIAG_INVALID_ARG;
    }

    if (io->send_frame == NULL || io->recv_frame == NULL)
    {
        return LEAP_CTRL_STACK_DIAG_IO_MISSING;
    }

    session_id = leap_mgmt_controller_session_id(&stack->mgmt);
    sequence   = leap_mgmt_controller_next_sequence(&stack->mgmt);

    if (io->send_frame(
            io->user_ctx,
            stack->peer_mac,
            0u,
            (uint16_t)LEAP_SERVICE_DIAG,
            request_type,
            session_id,
            sequence,
            0u,
            request_payload,
            request_length) != 0)
    {
        return LEAP_CTRL_STACK_DIAG_SEND_FAILED;
    }

    timeout_ms = stack->config.recv_timeout_ms;
    if (timeout_ms <= 0)
    {
        timeout_ms = LEAP_CTRL_STACK_DEFAULT_RECV_MS;
    }

    {
        uint64_t deadline_us = 0u;
        int      slice_ms    = timeout_ms;

        if (io->monotonic_us != NULL)
        {
            deadline_us =
                io->monotonic_us(io->user_ctx) +
                ((uint64_t)timeout_ms * 1000u);
        }

        for (attempt = 0u;; attempt++)
        {
            if (deadline_us != 0u && io->monotonic_us != NULL)
            {
                uint64_t now_us = io->monotonic_us(io->user_ctx);

                if (now_us >= deadline_us)
                {
                    return LEAP_CTRL_STACK_DIAG_RECV_TIMEOUT;
                }

                slice_ms = (int)((deadline_us - now_us) / 1000u);
                if (slice_ms <= 0)
                {
                    slice_ms = 1;
                }
                if (slice_ms > 100)
                {
                    slice_ms = 100;
                }
            }
            else if (attempt >= 8u)
            {
                return LEAP_CTRL_STACK_DIAG_RECV_TIMEOUT;
            }

            status = leap_ctrl_stack_recv(
                io,
                slice_ms,
                src_mac,
                &view,
                rx_buf,
                sizeof(rx_buf),
                &rx_length);
            if (status == LEAP_CTRL_STACK_RECV_TIMEOUT)
            {
                if (deadline_us != 0u && io->monotonic_us != NULL)
                {
                    continue;
                }

                return LEAP_CTRL_STACK_DIAG_RECV_TIMEOUT;
            }
            if (status != LEAP_CTRL_STACK_OK)
            {
                return leap_ctrl_stack_diag_map_status(status);
            }

            if (memcmp(src_mac, stack->peer_mac, 6) != 0)
            {
                continue;
            }

            if (view.header.service_id != (uint16_t)LEAP_SERVICE_DIAG)
            {
                continue;
            }

            if ((view.header.flags & LEAP_FLAG_ERROR) != 0u)
            {
                return LEAP_CTRL_STACK_DIAG_UNEXPECTED_REPLY;
            }

            if (view.header.message_type != expected_reply_type)
            {
                continue;
            }

            if (view.payload_length > reply_capacity)
            {
                return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
            }

            memcpy(reply_payload, view.payload, view.payload_length);
            *reply_length_out = view.payload_length;
            return LEAP_CTRL_STACK_DIAG_OK;
        }
    }
}

LeapControllerStackDiagStatus leap_controller_stack_read_diag(
    LeapControllerStack*           stack,
    const LeapControllerStackIo*   io,
    LeapControllerStackDiagResult* result_out)
{
    uint8_t                      req[64];
    uint8_t                      reply[LEAP_CTRL_STACK_RX_BUF];
    size_t                       req_length;
    size_t                       reply_length;
    LeapCountersReply            counters_hdr;
    LeapControllerStackDiagStatus status;
    LeapDiagControllerStatus     parse_status;

    if (result_out != NULL)
    {
        memset(result_out, 0, sizeof(*result_out));
    }

    if (stack == NULL || io == NULL || result_out == NULL)
    {
        return LEAP_CTRL_STACK_DIAG_INVALID_ARG;
    }

    if (leap_ctrl_stack_peer_ready(stack) == 0)
    {
        return LEAP_CTRL_STACK_DIAG_NOT_OP;
    }

    req_length = leap_diag_controller_build_read_counters(
        req,
        sizeof(req),
        (uint16_t)LEAP_COUNTER_RX_FRAMES_ACCEPTED,
        12u,
        0u);
    if (req_length == 0u)
    {
        return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
    }

    status = leap_ctrl_stack_diag_exchange(
        stack,
        io,
        LEAP_DIAG_READ_COUNTERS,
        req,
        req_length,
        LEAP_DIAG_COUNTERS_REPLY,
        reply,
        sizeof(reply),
        &reply_length);
    if (status != LEAP_CTRL_STACK_DIAG_OK)
    {
        return status;
    }

    parse_status = leap_diag_controller_on_counters_reply(
        reply,
        reply_length,
        &counters_hdr,
        result_out->counters,
        LEAP_CTRL_STACK_DIAG_MAX_COUNTERS,
        NULL);
    if (parse_status != LEAP_DIAG_CTRL_OK)
    {
        return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
    }

    result_out->has_counters   = 1;
    result_out->counter_count  = counters_hdr.counter_count;

    req_length = leap_diag_controller_build_read_timing(req, sizeof(req), 0u);
    if (req_length == 0u)
    {
        return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
    }

    status = leap_ctrl_stack_diag_exchange(
        stack,
        io,
        LEAP_DIAG_READ_TIMING,
        req,
        req_length,
        LEAP_DIAG_TIMING_REPLY,
        reply,
        sizeof(reply),
        &reply_length);
    if (status != LEAP_CTRL_STACK_DIAG_OK)
    {
        return status;
    }

    parse_status = leap_diag_controller_on_timing_reply(
        reply,
        reply_length,
        &result_out->timing);
    if (parse_status != LEAP_DIAG_CTRL_OK)
    {
        return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
    }

    result_out->has_timing = 1;
    return LEAP_CTRL_STACK_DIAG_OK;
}

LeapControllerStackDiagStatus leap_controller_stack_read_diag_extended(
    LeapControllerStack*           stack,
    const LeapControllerStackIo*   io,
    LeapControllerStackDiagResult* result_out)
{
    uint8_t                      req[64];
    uint8_t                      reply[LEAP_CTRL_STACK_RX_BUF];
    size_t                       req_length;
    size_t                       reply_length;
    LeapCountersReply            counters_hdr;
    LeapControllerStackDiagStatus status;
    LeapDiagControllerStatus     parse_status;

    if (result_out != NULL)
    {
        memset(result_out, 0, sizeof(*result_out));
    }

    if (stack == NULL || io == NULL || result_out == NULL)
    {
        return LEAP_CTRL_STACK_DIAG_INVALID_ARG;
    }

    if (leap_ctrl_stack_peer_ready(stack) == 0)
    {
        return LEAP_CTRL_STACK_DIAG_NOT_OP;
    }

    req_length = leap_diag_controller_build_read_counters(
        req,
        sizeof(req),
        0x0010u,
        7u,
        0u);
    if (req_length == 0u)
    {
        return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
    }

    status = leap_ctrl_stack_diag_exchange(
        stack,
        io,
        LEAP_DIAG_READ_COUNTERS,
        req,
        req_length,
        LEAP_DIAG_COUNTERS_REPLY,
        reply,
        sizeof(reply),
        &reply_length);
    if (status != LEAP_CTRL_STACK_DIAG_OK)
    {
        return status;
    }

    parse_status = leap_diag_controller_on_counters_reply(
        reply,
        reply_length,
        &counters_hdr,
        result_out->counters,
        LEAP_CTRL_STACK_DIAG_MAX_COUNTERS,
        NULL);
    if (parse_status != LEAP_DIAG_CTRL_OK)
    {
        return LEAP_CTRL_STACK_DIAG_PARSE_ERROR;
    }

    result_out->has_counters  = 1;
    result_out->counter_count = counters_hdr.counter_count;
    return LEAP_CTRL_STACK_DIAG_OK;
}

#define LEAP_CTRL_STACK_PROFILE_OBJECT_ID \
    LEAP_OBJECT_ID(LEAP_OBJ_NS_ENDPOINT_PROFILE, 0x0001u)

static LeapControllerStackStatus leap_ctrl_stack_fetch_profile_object(
    LeapControllerStack*            stack,
    const LeapControllerStackIo*    io,
    LeapDirControllerCapabilities*  caps_out)
{
    uint8_t                   payload[LEAP_CTRL_STACK_PAYLOAD_BUF];
    size_t                    payload_length;
    uint8_t                   src_mac[6];
    uint8_t                   rx[LEAP_CTRL_STACK_RX_BUF];
    size_t                    rx_length = 0u;
    LeapFrameView             view;
    LeapControllerStackStatus status;
    const uint8_t*            object_bytes;
    size_t                    object_length;

    payload_length = leap_dir_controller_build_read_object(
        payload,
        sizeof(payload),
        LEAP_CTRL_STACK_PROFILE_OBJECT_ID,
        0u,
        0u);
    if (payload_length == 0u)
    {
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    status = leap_ctrl_stack_send(
        stack,
        io,
        stack->peer_mac,
        LEAP_FLAG_ACK_REQUESTED,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_READ_OBJECT,
        0u,
        payload,
        payload_length);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    status = leap_ctrl_stack_recv_expected_from_peer(
        stack,
        io,
        stack->config.recv_timeout_ms > 0 ? stack->config.recv_timeout_ms : 2000,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_READ_OBJECT_REPLY,
        src_mac,
        &view,
        rx,
        sizeof(rx),
        &rx_length);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    if (view.payload == NULL ||
        view.payload_length < sizeof(LeapReadObjectReply))
    {
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    object_bytes  = view.payload + sizeof(LeapReadObjectReply);
    object_length = view.payload_length - sizeof(LeapReadObjectReply);
    if (leap_dir_controller_parse_profile_object(
            object_bytes,
            object_length,
            caps_out) != LEAP_DIR_CTRL_OK ||
        caps_out->endpoint_count == 0u)
    {
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    return LEAP_CTRL_STACK_OK;
}

static LeapControllerStackStatus leap_ctrl_stack_send_read_directory(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io)
{
    uint8_t                   payload[LEAP_CTRL_STACK_PAYLOAD_BUF];
    size_t                    payload_length;
    LeapControllerStackStatus send_status;

    payload_length = leap_dir_controller_build_read_directory(
        payload,
        sizeof(payload),
        0u,
        480u);
    if (payload_length == 0u)
    {
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    send_status = leap_ctrl_stack_send(
        stack,
        io,
        stack->peer_mac,
        LEAP_FLAG_ACK_REQUESTED,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_READ_DIRECTORY,
        0u,
        payload,
        payload_length);
    return send_status;
}

LeapControllerStackStatus leap_controller_stack_fetch_read_directory(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    uint8_t*                      reply_payload,
    size_t                        reply_capacity,
    size_t*                       reply_length_out)
{
    uint8_t                   src_mac[6];
    uint8_t                   rx[LEAP_CTRL_STACK_RX_BUF];
    size_t                    rx_length = 0u;
    LeapFrameView             view;
    LeapControllerStackStatus status;

    if (stack == NULL || io == NULL || peer_mac == NULL ||
        reply_payload == NULL || reply_length_out == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    leap_controller_stack_reset(stack);
    memcpy(stack->peer_mac, peer_mac, 6);
    stack->peer_bound = 1;

    status = leap_ctrl_stack_send_read_directory(stack, io);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    status = leap_ctrl_stack_recv_expected_from_peer(
        stack,
        io,
        stack->config.recv_timeout_ms > 0 ? stack->config.recv_timeout_ms : 2000,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_READ_DIRECTORY_REPLY,
        src_mac,
        &view,
        rx,
        sizeof(rx),
        &rx_length);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    if (view.payload == NULL || view.payload_length == 0u ||
        view.payload_length > reply_capacity)
    {
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    memcpy(reply_payload, view.payload, view.payload_length);
    *reply_length_out = view.payload_length;
    return LEAP_CTRL_STACK_OK;
}

LeapControllerStackStatus leap_controller_stack_probe_directory(
    LeapControllerStack*            stack,
    const LeapControllerStackIo*    io,
    const uint8_t*                  peer_mac,
    LeapDirControllerCapabilities*  caps_out)
{
    uint8_t                   hello_payload[64];
    size_t                    hello_len;
    uint8_t                   rx[LEAP_CTRL_STACK_RX_BUF];
    size_t                    rx_len = 0u;
    LeapFrameView             view;
    uint8_t                   src_mac[6];
    LeapHelloReply            hello;
    uint8_t                   dir_reply[LEAP_CTRL_STACK_RX_BUF];
    size_t                    dir_reply_len = 0u;
    uint32_t                  profile_id;
    int                       recv_ms;
    LeapControllerStackStatus status;

    if (stack == NULL || io == NULL || peer_mac == NULL || caps_out == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    leap_dir_controller_capabilities_init(caps_out);
    recv_ms = stack->config.recv_timeout_ms > 0 ? stack->config.recv_timeout_ms : 3000;

    leap_controller_stack_reset(stack);
    memcpy(stack->peer_mac, peer_mac, 6);
    stack->peer_bound = 1;

    hello_len = leap_disc_controller_build_hello(
        hello_payload,
        sizeof(hello_payload));
    if (hello_len == 0u)
    {
        return LEAP_CTRL_STACK_DISC_ERROR;
    }

    status = leap_ctrl_stack_send(
        stack,
        io,
        peer_mac,
        LEAP_FLAG_ACK_REQUESTED,
        (uint16_t)LEAP_SERVICE_DISC,
        LEAP_DISC_HELLO,
        0u,
        hello_payload,
        hello_len);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    status = leap_ctrl_stack_recv_expected_from_peer(
        stack,
        io,
        recv_ms,
        (uint16_t)LEAP_SERVICE_DISC,
        LEAP_DISC_HELLO_REPLY,
        src_mac,
        &view,
        rx,
        sizeof(rx),
        &rx_len);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    if (leap_disc_controller_on_hello_reply(
            view.payload,
            view.payload_length,
            &hello) != LEAP_DISC_CTRL_OK)
    {
        return LEAP_CTRL_STACK_DISC_ERROR;
    }

    caps_out->identity                = hello.identity;
    caps_out->default_profile_id      = hello.default_profile_id;
    caps_out->active_profile_id       = hello.active_profile_id;
    caps_out->locate_capability_flags = hello.locate_capability_flags;

    status = leap_ctrl_stack_send_read_directory(stack, io);
    if (status == LEAP_CTRL_STACK_OK)
    {
        dir_reply_len = 0u;
        status = leap_ctrl_stack_recv_expected_from_peer(
            stack,
            io,
            recv_ms,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_READ_DIRECTORY_REPLY,
            src_mac,
            &view,
            rx,
            sizeof(rx),
            &rx_len);
        if (status == LEAP_CTRL_STACK_OK &&
            view.payload != NULL &&
            view.payload_length >= sizeof(LeapReadDirectoryReply))
        {
            if (leap_dir_controller_parse_directory_tlvs(
                    view.payload + sizeof(LeapReadDirectoryReply),
                    view.payload_length - sizeof(LeapReadDirectoryReply),
                    caps_out) == LEAP_DIR_CTRL_OK &&
                caps_out->endpoint_count > 0u)
            {
                leap_dir_controller_capabilities_finalize(caps_out);
                return LEAP_CTRL_STACK_OK;
            }
        }
    }

    profile_id = hello.active_profile_id;
    if (profile_id == 0u)
    {
        profile_id = hello.default_profile_id;
    }

    if (profile_id != 0u &&
        leap_ctrl_stack_fetch_profile_object(stack, io, caps_out) ==
            LEAP_CTRL_STACK_OK &&
        caps_out->endpoint_count > 0u)
    {
        return LEAP_CTRL_STACK_OK;
    }

    if (hello.current_state == (uint16_t)LEAP_STATE_INIT ||
        hello.current_state == (uint16_t)LEAP_STATE_CONFIGURED)
    {
        if (profile_id != 0u &&
            leap_controller_stack_fetch_profile_reply(
                stack,
                io,
                peer_mac,
                profile_id,
                dir_reply,
                sizeof(dir_reply),
                &dir_reply_len) == LEAP_CTRL_STACK_OK &&
            leap_dir_controller_on_profile_reply_full(
                dir_reply,
                dir_reply_len,
                caps_out) == LEAP_DIR_CTRL_OK &&
            caps_out->endpoint_count > 0u)
        {
            leap_dir_controller_capabilities_finalize(caps_out);
            return LEAP_CTRL_STACK_OK;
        }
    }

    return LEAP_CTRL_STACK_DIR_ERROR;
}

LeapControllerStackStatus leap_controller_stack_fetch_profile_reply(
    LeapControllerStack*          stack,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    uint32_t                      profile_id,
    uint8_t*                      reply_payload,
    size_t                        reply_capacity,
    size_t*                       reply_length_out)
{
    uint8_t                   src_mac[6];
    uint8_t                   rx[LEAP_CTRL_STACK_RX_BUF];
    size_t                    rx_length = 0u;
    LeapFrameView             view;
    LeapControllerStackStatus status;

    if (stack == NULL || io == NULL || peer_mac == NULL ||
        reply_payload == NULL || reply_length_out == NULL || profile_id == 0u)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    leap_controller_stack_reset(stack);
    memcpy(stack->peer_mac, peer_mac, 6);
    stack->peer_bound = 1;
    stack->config.default_profile_id = profile_id;

    status = leap_ctrl_stack_send_select_profile(stack, io, NULL);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    status = leap_ctrl_stack_recv_expected_from_peer(
        stack,
        io,
        stack->config.recv_timeout_ms > 0 ? stack->config.recv_timeout_ms : 2000,
        (uint16_t)LEAP_SERVICE_DIR,
        LEAP_DIR_PROFILE_REPLY,
        src_mac,
        &view,
        rx,
        sizeof(rx),
        &rx_length);
    if (status != LEAP_CTRL_STACK_OK)
    {
        return status;
    }

    if (view.payload == NULL || view.payload_length == 0u ||
        view.payload_length > reply_capacity)
    {
        return LEAP_CTRL_STACK_DIR_ERROR;
    }

    memcpy(reply_payload, view.payload, view.payload_length);
    *reply_length_out = view.payload_length;
    return LEAP_CTRL_STACK_OK;
}

void leap_controller_stack_log_diag(
    const LeapControllerStackDiagResult* result)
{
    unsigned i;

    if (result == NULL)
    {
        return;
    }

    printf("DIAG readback:\n");

    if (result->has_counters != 0)
    {
        printf("  counters (%u):\n", (unsigned)result->counter_count);
        for (i = 0u; i < result->counter_count &&
                    i < LEAP_CTRL_STACK_DIAG_MAX_COUNTERS;
             i++)
        {
            printf(
                "    id=0x%04X value=%llu\n",
                result->counters[i].counter_id,
                (unsigned long long)result->counters[i].value);
        }
    }

    if (result->has_timing != 0)
    {
        printf(
            "  timing: last_cycle=%u us max_cycle=%u min_cycle=%u "
            "last_reply_lat=%u us max_reply_lat=%u us "
            "watchdog_remain=%u us lease_remain=%u us\n",
            result->timing.last_cycle_time_us,
            result->timing.max_cycle_time_us,
            result->timing.min_cycle_time_us,
            result->timing.last_reply_latency_us,
            result->timing.max_reply_latency_us,
            result->timing.process_watchdog_remaining_us,
            result->timing.owner_lease_remaining_us);
    }
}
