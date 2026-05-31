/*
 * leap_mgmt_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_mgmt_device.h"

#include <string.h>

#define LEAP_MGMT_DEFAULT_LEASE_US     5000000u
#define LEAP_MGMT_DEFAULT_WATCHDOG_US  100000u
#define LEAP_MGMT_MAX_LEASE_US           60000000u
#define LEAP_MGMT_MAX_WATCHDOG_US        10000000u

static int leap_mac_equal(const uint8_t* a, const uint8_t* b)
{
    return (memcmp(a, b, LEAP_MGMT_DEVICE_MAC_LEN) == 0);
}

static void leap_mac_copy(uint8_t* dst, const uint8_t* src)
{
    (void)memcpy(dst, src, LEAP_MGMT_DEVICE_MAC_LEN);
}

static uint32_t leap_mgmt_alloc_session_id(LeapMgmtDeviceContext* ctx)
{
    uint32_t id = ctx->next_session_id;

    if (id == 0u)
    {
        id = 1u;
    }

    ctx->next_session_id = id + 1u;
    if (ctx->next_session_id == 0u)
    {
        ctx->next_session_id = 1u;
    }

    return id;
}

static void leap_mgmt_clear_owner(LeapMgmtDeviceContext* ctx)
{
    ctx->owner_active        = 0u;
    ctx->owner_session_id    = 0u;
    memset(ctx->owner_mac, 0, sizeof(ctx->owner_mac));
    ctx->granted_lease_us    = 0u;
    ctx->granted_watchdog_us = 0u;
    ctx->lease_deadline_us   = 0u;
    ctx->watchdog_deadline_us = 0u;
}

static void leap_mgmt_clear_observer(LeapMgmtDeviceContext* ctx)
{
    ctx->observer_active     = 0u;
    ctx->observer_session_id = 0u;
    memset(ctx->observer_mac, 0, sizeof(ctx->observer_mac));
    ctx->observer_deadline_us = 0u;
}

static void leap_mgmt_transition_to_safe(LeapMgmtDeviceContext* ctx)
{
    if (ctx->device_state == LEAP_STATE_OP)
    {
        ctx->device_state = LEAP_STATE_SAFE;
    }
}

static uint32_t leap_mgmt_clamp_lease(const LeapMgmtDeviceContext* ctx, uint32_t requested_us)
{
    uint32_t granted = requested_us;

    if (granted == 0u)
    {
        granted = ctx->config.default_lease_us;
    }

    if (granted > ctx->config.max_lease_us)
    {
        granted = ctx->config.max_lease_us;
    }

    return granted;
}

static uint32_t leap_mgmt_clamp_watchdog(const LeapMgmtDeviceContext* ctx, uint32_t requested_us)
{
    uint32_t granted = requested_us;

    if (granted == 0u)
    {
        granted = ctx->config.default_watchdog_us;
    }

    if (granted > ctx->config.max_watchdog_us)
    {
        granted = ctx->config.max_watchdog_us;
    }

    return granted;
}

static void leap_mgmt_arm_owner_lease(LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    ctx->lease_deadline_us = now_us + (uint64_t)ctx->granted_lease_us;
    ctx->watchdog_deadline_us = now_us + (uint64_t)ctx->granted_watchdog_us;
}

static int leap_mgmt_owner_lease_expired(const LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    return (ctx->owner_active != 0u && now_us >= ctx->lease_deadline_us);
}

static int leap_mgmt_process_watchdog_expired(const LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    return (ctx->owner_active != 0u &&
            ctx->device_state == LEAP_STATE_OP &&
            now_us >= ctx->watchdog_deadline_us);
}

static int leap_mgmt_observer_expired(const LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    return (ctx->observer_active != 0u && now_us >= ctx->observer_deadline_us);
}

static int leap_mgmt_state_transition_allowed(LeapState_u16 current, LeapState_u16 requested)
{
    switch (current)
    {
    case LEAP_STATE_BOOT:
        return (requested == LEAP_STATE_INIT);
    case LEAP_STATE_INIT:
        return (requested == LEAP_STATE_CONFIGURED);
    case LEAP_STATE_CONFIGURED:
        return (requested == LEAP_STATE_SAFE);
    case LEAP_STATE_SAFE:
        return (requested == LEAP_STATE_OP);
    case LEAP_STATE_OP:
        return (requested == LEAP_STATE_SAFE);
    case LEAP_STATE_FAULT:
    default:
        return 0;
    }
}

static void leap_mgmt_fill_error(LeapMgmtDeviceReply* reply, LeapStatusCode_u16 code)
{
    reply->status      = LEAP_MGMT_DEVICE_HANDLE_ERROR;
    reply->error_code  = code;
    reply->message_type = 0u;
    reply->payload_length = 0u;
}

static void leap_mgmt_fill_open_session_reply(
    LeapMgmtDeviceContext* ctx,
    LeapMgmtDeviceReply*   reply,
    uint32_t               session_id,
    uint16_t               session_flags)
{
    LeapOpenSessionReply* body = (LeapOpenSessionReply*)reply->payload;

    memset(body, 0, sizeof(*body));
    body->assigned_session_id     = session_id;
    body->granted_lease_time_us   = ctx->granted_lease_us;
    body->granted_watchdog_time_us = ctx->granted_watchdog_us;
    body->session_flags           = session_flags;
    body->current_state           = (uint16_t)ctx->device_state;
    if ((session_flags & LEAP_SESSION_FLAG_OWNER) != 0u)
    {
        leap_mac_copy(body->owner_mac, ctx->owner_mac);
    }

    reply->status         = LEAP_MGMT_DEVICE_HANDLE_OK;
    reply->error_code     = LEAP_STATUS_OK;
    reply->message_type   = LEAP_MGMT_OPEN_SESSION_REPLY;
    reply->payload_length = sizeof(LeapOpenSessionReply);
}

static void leap_mgmt_fill_state_reply(
    LeapMgmtDeviceContext* ctx,
    LeapMgmtDeviceReply*   reply,
    LeapState_u16          accepted_state)
{
    LeapStateReply* body = (LeapStateReply*)reply->payload;

    memset(body, 0, sizeof(*body));
    body->accepted_state = (uint16_t)accepted_state;
    body->current_state  = (uint16_t)ctx->device_state;

    reply->status         = LEAP_MGMT_DEVICE_HANDLE_OK;
    reply->error_code     = LEAP_STATUS_OK;
    reply->message_type   = LEAP_MGMT_STATE_REPLY;
    reply->payload_length = sizeof(LeapStateReply);
}

static LeapMgmtDeviceHandleStatus leap_mgmt_handle_open_session(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    const LeapOpenSessionRequest* open_req;
    uint16_t                      session_flags = 0u;
    uint32_t                      session_id;
    const uint8_t*                source_mac;
    int                           reboot_recovery_accepted = 0;

    if (request->payload == NULL || request->payload_length < sizeof(LeapOpenSessionRequest))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (request->source_mac == NULL)
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    open_req   = (const LeapOpenSessionRequest*)request->payload;
    source_mac = request->source_mac;

    if ((open_req->open_flags & LEAP_OPEN_FLAG_REQUEST_OWNER) != 0u)
    {
        if (ctx->owner_active != 0u)
        {
            if (leap_mac_equal(source_mac, ctx->owner_mac))
            {
                if ((open_req->open_flags & LEAP_OPEN_FLAG_REBOOT_RECOVERY) != 0u)
                {
                    if (ctx->device_state == LEAP_STATE_OP)
                    {
                        ctx->device_state = LEAP_STATE_CONFIGURED;
                    }
                    leap_mgmt_clear_owner(ctx);
                    reboot_recovery_accepted = 1;
                }
                else if (!leap_mgmt_owner_lease_expired(ctx, request->now_us))
                {
                    leap_mgmt_fill_error(reply, LEAP_STATUS_BUSY);
                    return LEAP_MGMT_DEVICE_HANDLE_ERROR;
                }
                else if ((open_req->open_flags & LEAP_OPEN_FLAG_STEAL_EXPIRED) == 0u)
                {
                    leap_mgmt_fill_error(reply, LEAP_STATUS_BUSY);
                    return LEAP_MGMT_DEVICE_HANDLE_ERROR;
                }
                else
                {
                    leap_mgmt_clear_owner(ctx);
                    leap_mgmt_transition_to_safe(ctx);
                }
            }
            else if (!leap_mgmt_owner_lease_expired(ctx, request->now_us))
            {
                leap_mgmt_fill_error(reply, LEAP_STATUS_NOT_OWNER);
                return LEAP_MGMT_DEVICE_HANDLE_ERROR;
            }
            else if ((open_req->open_flags & LEAP_OPEN_FLAG_STEAL_EXPIRED) != 0u)
            {
                leap_mgmt_clear_owner(ctx);
                leap_mgmt_transition_to_safe(ctx);
            }
            else
            {
                leap_mgmt_fill_error(reply, LEAP_STATUS_BUSY);
                return LEAP_MGMT_DEVICE_HANDLE_ERROR;
            }
        }

        ctx->owner_active = 1u;
        leap_mac_copy(ctx->owner_mac, source_mac);
        ctx->granted_lease_us = leap_mgmt_clamp_lease(ctx, open_req->requested_lease_time_us);
        ctx->granted_watchdog_us = leap_mgmt_clamp_watchdog(ctx, open_req->requested_watchdog_time_us);
        leap_mgmt_arm_owner_lease(ctx, request->now_us);

        session_flags = (uint16_t)(LEAP_SESSION_FLAG_OWNER | LEAP_SESSION_FLAG_LEASE_ACTIVE);
    }
    else if ((open_req->open_flags & LEAP_OPEN_FLAG_OBSERVER_ONLY) != 0u)
    {
        ctx->observer_active = 1u;
        leap_mac_copy(ctx->observer_mac, source_mac);
        ctx->observer_deadline_us = request->now_us + (uint64_t)ctx->config.default_lease_us;
        session_flags = LEAP_SESSION_FLAG_OBSERVER;
    }
    else
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    session_id = leap_mgmt_alloc_session_id(ctx);
    if ((session_flags & LEAP_SESSION_FLAG_OWNER) != 0u)
    {
        ctx->owner_session_id = session_id;
    }
    else
    {
        ctx->observer_session_id = session_id;
    }

    if ((session_flags & LEAP_SESSION_FLAG_OWNER) != 0u &&
        ctx->device_state == LEAP_STATE_CONFIGURED &&
        reboot_recovery_accepted == 0)
    {
        ctx->device_state = LEAP_STATE_SAFE;
    }

    leap_mgmt_fill_open_session_reply(ctx, reply, session_id, session_flags);
    return LEAP_MGMT_DEVICE_HANDLE_OK;
}

static LeapMgmtDeviceHandleStatus leap_mgmt_handle_close_session(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    (void)reply;

    if (request->payload == NULL || request->payload_length < sizeof(LeapCloseSessionRequest))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (ctx->owner_active != 0u && request->session_id == ctx->owner_session_id)
    {
        leap_mgmt_clear_owner(ctx);
        leap_mgmt_transition_to_safe(ctx);
        return LEAP_MGMT_DEVICE_HANDLE_NO_REPLY;
    }

    if (ctx->observer_active != 0u && request->session_id == ctx->observer_session_id)
    {
        leap_mgmt_clear_observer(ctx);
        return LEAP_MGMT_DEVICE_HANDLE_NO_REPLY;
    }

    leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
    return LEAP_MGMT_DEVICE_HANDLE_ERROR;
}

static LeapMgmtDeviceHandleStatus leap_mgmt_handle_heartbeat(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    (void)reply;

    if (request->payload == NULL || request->payload_length < sizeof(LeapHeartbeatPayload))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (ctx->owner_active == 0u || request->session_id != ctx->owner_session_id)
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (request->source_mac == NULL || !leap_mac_equal(request->source_mac, ctx->owner_mac))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_NOT_OWNER);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    leap_mgmt_device_refresh_owner_lease(ctx, request->now_us);
    return LEAP_MGMT_DEVICE_HANDLE_NO_REPLY;
}

static LeapMgmtDeviceHandleStatus leap_mgmt_handle_set_state(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    const LeapSetStateRequest* set_req;
    LeapState_u16              requested;

    if (request->payload == NULL || request->payload_length < sizeof(LeapSetStateRequest))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (ctx->owner_active == 0u || request->session_id != ctx->owner_session_id)
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_NOT_OWNER);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (request->source_mac == NULL || !leap_mac_equal(request->source_mac, ctx->owner_mac))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_NOT_OWNER);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    set_req   = (const LeapSetStateRequest*)request->payload;
    requested = (LeapState_u16)set_req->requested_state;

    if (!leap_mgmt_state_transition_allowed(ctx->device_state, requested))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (requested == LEAP_STATE_OP)
    {
        if (ctx->granted_watchdog_us == 0u)
        {
            leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
            return LEAP_MGMT_DEVICE_HANDLE_ERROR;
        }
        leap_mgmt_device_refresh_process_watchdog(ctx, request->now_us);
    }

    ctx->device_state = requested;
    leap_mgmt_fill_state_reply(ctx, reply, requested);
    return LEAP_MGMT_DEVICE_HANDLE_OK;
}

static LeapMgmtDeviceHandleStatus leap_mgmt_handle_fault_reset(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    (void)request;

    if (ctx->device_state != LEAP_STATE_FAULT)
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    ctx->device_state = LEAP_STATE_INIT;
    leap_mgmt_clear_owner(ctx);
    leap_mgmt_clear_observer(ctx);
    return LEAP_MGMT_DEVICE_HANDLE_NO_REPLY;
}

static LeapMgmtDeviceHandleStatus leap_mgmt_handle_owner_release(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    (void)request;
    (void)reply;

    if (ctx->owner_active == 0u || request->session_id != ctx->owner_session_id)
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_NOT_OWNER);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (request->source_mac == NULL || !leap_mac_equal(request->source_mac, ctx->owner_mac))
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_NOT_OWNER);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    leap_mgmt_clear_owner(ctx);
    ctx->device_state = LEAP_STATE_SAFE;
    return LEAP_MGMT_DEVICE_HANDLE_NO_REPLY;
}

static int leap_mgmt_validate_session(
    const LeapMgmtDeviceContext* ctx,
    uint32_t                     session_id,
    const uint8_t*               source_mac)
{
    if (ctx->owner_active != 0u && session_id == ctx->owner_session_id)
    {
        if (source_mac != NULL && !leap_mac_equal(source_mac, ctx->owner_mac))
        {
            return 0;
        }
        return 1;
    }

    if (ctx->observer_active != 0u && session_id == ctx->observer_session_id)
    {
        if (source_mac != NULL && !leap_mac_equal(source_mac, ctx->observer_mac))
        {
            return 0;
        }
        return 1;
    }

    return 0;
}

void leap_mgmt_device_init(LeapMgmtDeviceContext* ctx, const LeapMgmtDeviceConfig* config)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->device_state = LEAP_STATE_BOOT;
    ctx->next_session_id = 1u;

    if (config != NULL)
    {
        ctx->config = *config;
    }

    if (ctx->config.default_lease_us == 0u)
    {
        ctx->config.default_lease_us = LEAP_MGMT_DEFAULT_LEASE_US;
    }
    if (ctx->config.default_watchdog_us == 0u)
    {
        ctx->config.default_watchdog_us = LEAP_MGMT_DEFAULT_WATCHDOG_US;
    }
    if (ctx->config.max_lease_us == 0u)
    {
        ctx->config.max_lease_us = LEAP_MGMT_MAX_LEASE_US;
    }
    if (ctx->config.max_watchdog_us == 0u)
    {
        ctx->config.max_watchdog_us = LEAP_MGMT_MAX_WATCHDOG_US;
    }
}

void leap_mgmt_device_on_transport_ready(LeapMgmtDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->device_state == LEAP_STATE_BOOT)
    {
        ctx->device_state = LEAP_STATE_INIT;
    }
}

void leap_mgmt_device_on_profile_selected(LeapMgmtDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->device_state == LEAP_STATE_INIT)
    {
        ctx->device_state = LEAP_STATE_CONFIGURED;
    }
}

void leap_mgmt_device_enter_fault(LeapMgmtDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->device_state = LEAP_STATE_FAULT;
    leap_mgmt_clear_owner(ctx);
    leap_mgmt_clear_observer(ctx);
}

void leap_mgmt_device_tick(LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    if (ctx == NULL)
    {
        return;
    }

    if (leap_mgmt_observer_expired(ctx, now_us))
    {
        leap_mgmt_clear_observer(ctx);
    }

    if (leap_mgmt_process_watchdog_expired(ctx, now_us))
    {
        leap_mgmt_clear_owner(ctx);
        ctx->device_state = LEAP_STATE_SAFE;
        return;
    }

    if (leap_mgmt_owner_lease_expired(ctx, now_us))
    {
        leap_mgmt_clear_owner(ctx);
        leap_mgmt_transition_to_safe(ctx);
    }
}

LeapMgmtDeviceHandleStatus leap_mgmt_device_handle(
    LeapMgmtDeviceContext*       ctx,
    const LeapMgmtDeviceRequest* request,
    LeapMgmtDeviceReply*         reply)
{
    if (ctx == NULL || request == NULL || reply == NULL)
    {
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    memset(reply, 0, sizeof(*reply));

    if (ctx->device_state == LEAP_STATE_FAULT &&
        request->message_type != LEAP_MGMT_FAULT_RESET)
    {
        leap_mgmt_fill_error(reply, LEAP_STATUS_FAULTED);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    if (request->message_type != LEAP_MGMT_OPEN_SESSION &&
        request->session_id != 0u &&
        !leap_mgmt_validate_session(ctx, request->session_id, request->source_mac))
    {
        if (ctx->owner_active != 0u && leap_mgmt_owner_lease_expired(ctx, request->now_us))
        {
            leap_mgmt_fill_error(reply, LEAP_STATUS_LEASE_EXPIRED);
        }
        else
        {
            leap_mgmt_fill_error(reply, LEAP_STATUS_INVALID_STATE);
        }
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }

    switch (request->message_type)
    {
    case LEAP_MGMT_OPEN_SESSION:
        return leap_mgmt_handle_open_session(ctx, request, reply);
    case LEAP_MGMT_CLOSE_SESSION:
        return leap_mgmt_handle_close_session(ctx, request, reply);
    case LEAP_MGMT_HEARTBEAT:
        return leap_mgmt_handle_heartbeat(ctx, request, reply);
    case LEAP_MGMT_SET_STATE:
        return leap_mgmt_handle_set_state(ctx, request, reply);
    case LEAP_MGMT_FAULT_RESET:
        return leap_mgmt_handle_fault_reset(ctx, request, reply);
    case LEAP_MGMT_OWNER_RELEASE:
        return leap_mgmt_handle_owner_release(ctx, request, reply);
    default:
        leap_mgmt_fill_error(reply, LEAP_STATUS_UNSUPPORTED_MESSAGE);
        return LEAP_MGMT_DEVICE_HANDLE_ERROR;
    }
}

int leap_mgmt_device_session_allows_owner_pd(
    const LeapMgmtDeviceContext* ctx,
    uint32_t                     session_id,
    const uint8_t*               source_mac)
{
    if (ctx == NULL)
    {
        return 0;
    }

    if (ctx->device_state != LEAP_STATE_OP)
    {
        return 0;
    }

    if (ctx->owner_active == 0u || session_id != ctx->owner_session_id)
    {
        return 0;
    }

    if (source_mac == NULL || !leap_mac_equal(source_mac, ctx->owner_mac))
    {
        return 0;
    }

    return 1;
}

void leap_mgmt_device_refresh_owner_lease(LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    if (ctx == NULL || ctx->owner_active == 0u)
    {
        return;
    }

    ctx->lease_deadline_us = now_us + (uint64_t)ctx->granted_lease_us;
}

void leap_mgmt_device_refresh_process_watchdog(LeapMgmtDeviceContext* ctx, uint64_t now_us)
{
    if (ctx == NULL || ctx->owner_active == 0u)
    {
        return;
    }

    ctx->watchdog_deadline_us = now_us + (uint64_t)ctx->granted_watchdog_us;
}

LeapState_u16 leap_mgmt_device_get_state(const LeapMgmtDeviceContext* ctx)
{
    if (ctx == NULL)
    {
        return LEAP_STATE_BOOT;
    }

    return ctx->device_state;
}
