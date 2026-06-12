/*
 * leap_device_stack.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_device_stack.h"

#include "leap/leap_disc_device.h"
#include "leap/leap_mgmt_process.h"

#include <string.h>

void leap_device_stack_note_frame_rx(
    LeapDeviceStack* stack,
    uint64_t         now_us,
    uint16_t         service_id)
{
    if (stack == NULL)
    {
        return;
    }

    stack->last_frame_rx_us      = now_us;
    stack->last_frame_service_id = service_id;
}

static void leap_device_stack_on_mgmt_success(
    LeapDeviceStack*             stack,
    const LeapMgmtProcessResult* mgmt_result)
{
    if (stack == NULL || mgmt_result == NULL)
    {
        return;
    }

    if ((mgmt_result->flags & LEAP_MGMT_PROCESS_FLAG_OWNERSHIP_CHANGED) != 0u)
    {
        leap_pd_device_sync_profile_from_dir(&stack->pd, &stack->dir);
        leap_pd_device_reset_sequence(
            &stack->pd,
            stack->mgmt.owner_session_id);
    }

    if ((mgmt_result->flags & LEAP_MGMT_PROCESS_FLAG_STATE_CHANGED) != 0u &&
        stack->mgmt.device_state == LEAP_STATE_OP)
    {
        leap_pd_device_reset_sequence(
            &stack->pd,
            stack->mgmt.owner_session_id);
    }
}

void leap_device_stack_init(LeapDeviceStack* stack, const LeapMgmtDeviceConfig* config)
{
    LeapDeviceStackConfig full;

    memset(&full, 0, sizeof(full));
    if (config != NULL)
    {
        full.mgmt = *config;
    }

    leap_device_stack_init_full(stack, &full);
}

void leap_device_stack_init_full(LeapDeviceStack* stack, const LeapDeviceStackConfig* config)
{
    if (stack == NULL)
    {
        return;
    }

    memset(stack, 0, sizeof(*stack));

    if (config != NULL)
    {
        leap_mgmt_device_init(&stack->mgmt, &config->mgmt);
        leap_disc_device_init(&stack->disc, &config->disc);
        leap_dir_device_init(&stack->dir, &config->dir);
        leap_diag_device_init(&stack->diag, &config->diag);
    }
    else
    {
        leap_mgmt_device_init(&stack->mgmt, NULL);
        leap_disc_device_init(&stack->disc, NULL);
        leap_dir_device_init(&stack->dir, NULL);
        leap_diag_device_init(&stack->diag, NULL);
    }

    leap_dir_device_sync_disc(&stack->dir, &stack->disc);
    leap_pd_device_init(&stack->pd, NULL);
    /*
     * DIR may already advertise the active digital-I/O profile (platform
     * config) before SELECT_PROFILE runs. PD init defaults to 16x16; sync now
     * so EXCHANGE validation matches LEAP-DIR (e.g. 8x8 ClearCore).
     */
    leap_pd_device_sync_profile_from_dir(&stack->pd, &stack->dir);
    leap_diag_device_on_transport_ready(&stack->diag, 0u);
    stack->pd_io_bound = 0;
}

void leap_device_stack_bind_pd_io(
    LeapDeviceStack*             stack,
    const LeapPdDeviceIoBinding* io_binding)
{
    if (stack == NULL)
    {
        return;
    }

    if (io_binding != NULL)
    {
        stack->pd_io      = *io_binding;
        stack->pd_io_bound = 1;
    }
    else
    {
        memset(&stack->pd_io, 0, sizeof(stack->pd_io));
        stack->pd_io_bound = 0;
    }
}

LeapDeviceStackStatus leap_device_stack_process_frame(
    LeapDeviceStack*       stack,
    const uint8_t*         source_mac,
    uint64_t               now_us,
    const uint8_t*         data,
    size_t                 length,
    LeapDeviceStackResult* result)
{
    LeapFrameView         view;
    LeapFrameParseResult  parse_result;
    LeapMgmtProcessResult mgmt_result;
    LeapPdDeviceResult    pd_result;
    LeapDiscDeviceResult  disc_result;
    LeapDirDeviceResult   dir_result;
    LeapDiagDeviceResult  diag_result;
    LeapMgmtProcessStatus mgmt_status;
    LeapPdDeviceStatus    pd_status;
    LeapDiscDeviceStatus  disc_status;
    LeapDirDeviceStatus   dir_status;
    LeapDiagDeviceStatus  diag_status;
    uint16_t              service_id;

    if (result == NULL)
    {
        return LEAP_DEVICE_STACK_FRAME_ERROR;
    }

    memset(result, 0, sizeof(*result));

    if (stack == NULL || source_mac == NULL || data == NULL)
    {
        result->status = LEAP_DEVICE_STACK_FRAME_ERROR;
        return LEAP_DEVICE_STACK_FRAME_ERROR;
    }

    parse_result = leap_frame_parse(data, length, &view);
    if (parse_result != LEAP_FRAME_OK)
    {
        leap_diag_device_on_frame_parse_error(&stack->diag, parse_result);
        result->status      = LEAP_DEVICE_STACK_FRAME_ERROR;
        result->frame_error = parse_result;
        return LEAP_DEVICE_STACK_FRAME_ERROR;
    }

    result->frame       = view;
    result->device_state = leap_mgmt_device_get_state(&stack->mgmt);
    service_id          = view.header.service_id;
    result->service_id  = (LeapServiceId_u16)service_id;

    if (service_id == (uint16_t)LEAP_SERVICE_MGMT)
    {
        mgmt_status = leap_mgmt_process_frame(
            &stack->mgmt,
            source_mac,
            now_us,
            data,
            length,
            &mgmt_result);

        result->frame        = mgmt_result.frame;
        result->flags       |= mgmt_result.flags;
        result->device_state = mgmt_result.device_state;
        result->error_code   = mgmt_result.error_code;
        result->mgmt_reply   = mgmt_result.reply;

        if (mgmt_status == LEAP_MGMT_PROCESS_OK)
        {
            leap_diag_device_on_mgmt_flags(
                &stack->diag,
                mgmt_result.flags,
                mgmt_result.reply.message_type,
                mgmt_result.device_state,
                now_us);
            leap_diag_device_on_frame_accepted(&stack->diag);
            leap_device_stack_note_frame_rx(stack, now_us, service_id);
            leap_device_stack_on_mgmt_success(stack, &mgmt_result);
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        leap_diag_device_on_frame_rejected(&stack->diag);
        if (mgmt_status == LEAP_MGMT_PROCESS_HANDLER_ERROR)
        {
            stack->diag.state_transition_rejects++;
        }

        if (mgmt_status == LEAP_MGMT_PROCESS_HANDLER_ERROR)
        {
            result->status = LEAP_DEVICE_STACK_MGMT_ERROR;
            return LEAP_DEVICE_STACK_MGMT_ERROR;
        }

        result->status = LEAP_DEVICE_STACK_MGMT_ERROR;
        return LEAP_DEVICE_STACK_MGMT_ERROR;
    }

    if (service_id == (uint16_t)LEAP_SERVICE_DISC)
    {
        disc_status = leap_disc_device_process_frame(
            &stack->disc,
            &stack->mgmt,
            source_mac,
            data,
            length,
            &disc_result);

        result->frame        = disc_result.frame;
        result->device_state = leap_mgmt_device_get_state(&stack->mgmt);
        result->error_code   = disc_result.error_code;
        result->disc_message_type   = disc_result.message_type;
        result->disc_payload_length = disc_result.payload_length;
        (void)memcpy(
            result->disc_payload,
            disc_result.payload,
            disc_result.payload_length);

        if (disc_status == LEAP_DISC_DEVICE_OK)
        {
            leap_diag_device_on_frame_accepted(&stack->diag);
            leap_device_stack_note_frame_rx(stack, now_us, service_id);
            result->flags |= LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY;
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        leap_diag_device_on_frame_rejected(&stack->diag);
        result->status = LEAP_DEVICE_STACK_DISC_ERROR;
        return LEAP_DEVICE_STACK_DISC_ERROR;
    }

    if (service_id == (uint16_t)LEAP_SERVICE_DIR)
    {
        dir_status = leap_dir_device_process_frame(
            &stack->dir,
            &stack->disc,
            &stack->mgmt,
            source_mac,
            data,
            length,
            &dir_result);

        result->frame        = dir_result.frame;
        result->device_state = leap_mgmt_device_get_state(&stack->mgmt);
        result->error_code   = dir_result.error_code;
        result->dir_message_type   = dir_result.message_type;
        result->dir_payload_length = dir_result.payload_length;
        result->flags             |= dir_result.flags;
        (void)memcpy(
            result->dir_payload,
            dir_result.payload,
            dir_result.payload_length);

        if (dir_status == LEAP_DIR_DEVICE_OK)
        {
            leap_diag_device_on_frame_accepted(&stack->diag);
            leap_device_stack_note_frame_rx(stack, now_us, service_id);
            result->flags |= LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY;
            if ((dir_result.flags & LEAP_DIR_DEVICE_FLAG_PROFILE_SELECTED) != 0u)
            {
                leap_pd_device_sync_profile_from_dir(&stack->pd, &stack->dir);
                leap_pd_device_reset_sequence(
                    &stack->pd,
                    stack->mgmt.owner_session_id);
            }
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        leap_diag_device_on_frame_rejected(&stack->diag);
        result->status = LEAP_DEVICE_STACK_DIR_ERROR;
        return LEAP_DEVICE_STACK_DIR_ERROR;
    }

    if (service_id == (uint16_t)LEAP_SERVICE_PD)
    {
        pd_status = leap_pd_device_process_parsed_frame(
            &stack->mgmt,
            &stack->pd,
            stack->pd_io_bound ? &stack->pd_io : NULL,
            source_mac,
            now_us,
            &view,
            &pd_result);

        result->frame        = pd_result.frame;
        result->flags       |= pd_result.flags;
        result->device_state = leap_mgmt_device_get_state(&stack->mgmt);
        result->error_code   = pd_result.error_code;
        result->pd_outputs_applied  = pd_result.digital_outputs_applied;
        result->pd_inputs_snapshot  = pd_result.digital_inputs_snapshot;
        result->pd_reply_message_type   = pd_result.reply_message_type;
        result->pd_reply_payload_length = pd_result.reply_payload_length;
        if (pd_result.reply_payload_length > 0u)
        {
            (void)memcpy(
                result->pd_reply_payload,
                pd_result.reply_payload,
                pd_result.reply_payload_length);
        }

        leap_diag_device_on_pd_result(&stack->diag, &pd_result, now_us);

        if (pd_status == LEAP_PD_DEVICE_OK)
        {
            leap_diag_device_on_frame_accepted(&stack->diag);
            leap_device_stack_note_frame_rx(stack, now_us, service_id);
            if (pd_result.reply_payload_length > 0u)
            {
                result->flags |= LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY;
            }
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        leap_diag_device_on_frame_rejected(&stack->diag);

        if (pd_status == LEAP_PD_DEVICE_REJECTED)
        {
            result->status = LEAP_DEVICE_STACK_PD_REJECTED;
            return LEAP_DEVICE_STACK_PD_REJECTED;
        }

        if (pd_status == LEAP_PD_DEVICE_IGNORED_RESPONSE)
        {
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        result->status = LEAP_DEVICE_STACK_PD_REJECTED;
        return LEAP_DEVICE_STACK_PD_REJECTED;
    }

    if (service_id == (uint16_t)LEAP_SERVICE_DIAG)
    {
        diag_status = leap_diag_device_process_frame(
            &stack->diag,
            &stack->mgmt,
            source_mac,
            now_us,
            data,
            length,
            &diag_result);

        result->frame        = diag_result.frame;
        result->device_state = leap_mgmt_device_get_state(&stack->mgmt);
        result->error_code   = diag_result.error_code;
        result->diag_message_type   = diag_result.message_type;
        result->diag_payload_length = diag_result.payload_length;
        if (diag_result.payload_length > 0u)
        {
            (void)memcpy(
                result->diag_payload,
                diag_result.payload,
                diag_result.payload_length);
        }

        if (diag_status == LEAP_DIAG_DEVICE_OK)
        {
            leap_diag_device_on_frame_accepted(&stack->diag);
            leap_device_stack_note_frame_rx(stack, now_us, service_id);
            result->flags |= LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY;
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        if (diag_status == LEAP_DIAG_DEVICE_NO_REPLY)
        {
            leap_diag_device_on_frame_accepted(&stack->diag);
            leap_device_stack_note_frame_rx(stack, now_us, service_id);
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        leap_diag_device_on_frame_rejected(&stack->diag);
        result->status = LEAP_DEVICE_STACK_DIAG_ERROR;
        return LEAP_DEVICE_STACK_DIAG_ERROR;
    }

    leap_diag_device_on_unsupported_service(&stack->diag);
    result->status = LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE;
    return LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE;
}

LeapDeviceStackStatus leap_device_stack_tick(
    LeapDeviceStack* stack,
    uint64_t         now_us,
    uint32_t*        flags_out)
{
    uint32_t flags = 0u;

    if (stack == NULL)
    {
        return LEAP_DEVICE_STACK_FRAME_ERROR;
    }

    (void)leap_mgmt_process_tick(&stack->mgmt, now_us, &flags);
    leap_diag_device_on_tick_flags(&stack->diag, flags, now_us);

    if (flags_out != NULL)
    {
        *flags_out = flags;
    }

    return LEAP_DEVICE_STACK_OK;
}

void leap_device_stack_notify_tx_ok(
    LeapDeviceStack* stack,
    uint64_t         now_us)
{
    uint64_t reply_latency_us = 0u;
    uint32_t cycle_time_us    = 0u;

    if (stack == NULL)
    {
        return;
    }

    if (stack->last_frame_rx_us > 0u && now_us >= stack->last_frame_rx_us)
    {
        reply_latency_us = now_us - stack->last_frame_rx_us;
    }

    leap_diag_device_on_frame_transmitted(&stack->diag, reply_latency_us);

    if (stack->last_frame_service_id == (uint16_t)LEAP_SERVICE_PD &&
        reply_latency_us > 0u)
    {
        cycle_time_us = (reply_latency_us > (uint64_t)UINT32_MAX)
                            ? UINT32_MAX
                            : (uint32_t)reply_latency_us;
        leap_diag_device_on_pd_cycle_time(&stack->diag, cycle_time_us);
    }
}

void leap_device_stack_notify_tx_drop(LeapDeviceStack* stack)
{
    if (stack == NULL)
    {
        return;
    }

    leap_diag_device_on_frame_tx_dropped(&stack->diag);
}

void leap_device_stack_apply_safe_on_flags(
    uint32_t flags,
    void (*enter_safe)(void* ctx),
    void* ctx)
{
    if ((flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u &&
        enter_safe != NULL)
    {
        enter_safe(ctx);
    }
}
