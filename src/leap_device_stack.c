/*
 * leap_device_stack.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_device_stack.h"

#include <string.h>

void leap_device_stack_init(LeapDeviceStack* stack, const LeapMgmtDeviceConfig* config)
{
    if (stack == NULL)
    {
        return;
    }

    leap_mgmt_device_init(&stack->mgmt, config);
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
    LeapMgmtProcessStatus mgmt_status;
    LeapPdDeviceStatus    pd_status;
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
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        if (mgmt_status == LEAP_MGMT_PROCESS_HANDLER_ERROR)
        {
            result->status = LEAP_DEVICE_STACK_MGMT_ERROR;
            return LEAP_DEVICE_STACK_MGMT_ERROR;
        }

        result->status = LEAP_DEVICE_STACK_MGMT_ERROR;
        return LEAP_DEVICE_STACK_MGMT_ERROR;
    }

    if (service_id == (uint16_t)LEAP_SERVICE_PD)
    {
        pd_status = leap_pd_device_process_frame(
            &stack->mgmt,
            source_mac,
            now_us,
            data,
            length,
            &pd_result);

        result->frame        = pd_result.frame;
        result->flags       |= pd_result.flags;
        result->device_state = leap_mgmt_device_get_state(&stack->mgmt);
        result->error_code   = pd_result.error_code;

        if (pd_status == LEAP_PD_DEVICE_OK)
        {
            result->status = LEAP_DEVICE_STACK_OK;
            return LEAP_DEVICE_STACK_OK;
        }

        if (pd_status == LEAP_PD_DEVICE_REJECTED)
        {
            result->status = LEAP_DEVICE_STACK_PD_REJECTED;
            return LEAP_DEVICE_STACK_PD_REJECTED;
        }

        result->status = LEAP_DEVICE_STACK_PD_REJECTED;
        return LEAP_DEVICE_STACK_PD_REJECTED;
    }

    result->status = LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE;
    return LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE;
}

LeapDeviceStackStatus leap_device_stack_tick(
    LeapDeviceStack* stack,
    uint64_t         now_us,
    uint32_t*        flags_out)
{
    if (stack == NULL)
    {
        return LEAP_DEVICE_STACK_FRAME_ERROR;
    }

    (void)leap_mgmt_process_tick(&stack->mgmt, now_us, flags_out);
    return LEAP_DEVICE_STACK_OK;
}
