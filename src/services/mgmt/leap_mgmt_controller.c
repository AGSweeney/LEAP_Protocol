/*
 * leap_mgmt_controller.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_mgmt_controller.h"

#include <string.h>

#define LEAP_MGMT_CTRL_DEFAULT_LEASE_US     5000000u
#define LEAP_MGMT_CTRL_DEFAULT_WATCHDOG_US  500000u
#define LEAP_MGMT_CTRL_DEFAULT_LEASE_DIV    2u

static void leap_mgmt_ctrl_clear_event(LeapMgmtControllerEvent* event)
{
    if (event != NULL)
    {
        memset(event, 0, sizeof(*event));
        event->status = LEAP_MGMT_CTRL_OK;
    }
}

static void leap_mgmt_ctrl_set_error(
    LeapMgmtControllerEvent*     event,
    LeapMgmtControllerStatus     status,
    LeapStatusCode_u16           code)
{
    if (event != NULL)
    {
        event->status     = status;
        event->error_code = code;
    }
}

static void leap_mgmt_ctrl_refresh_lease(
    LeapMgmtControllerContext* ctx,
    uint64_t                   now_us,
    LeapMgmtControllerEvent*   event)
{
    ctx->last_lease_refresh_us = now_us;
    if (event != NULL)
    {
        event->flags |= LEAP_MGMT_CTRL_FLAG_LEASE_REFRESHED;
    }
}

void leap_mgmt_controller_init(
    LeapMgmtControllerContext*       ctx,
    const LeapMgmtControllerConfig* config)
{
    if (ctx == NULL)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    if (config != NULL)
    {
        ctx->config = *config;
    }

    if (ctx->config.default_lease_us == 0u)
    {
        ctx->config.default_lease_us = LEAP_MGMT_CTRL_DEFAULT_LEASE_US;
    }

    if (ctx->config.default_watchdog_us == 0u)
    {
        ctx->config.default_watchdog_us = LEAP_MGMT_CTRL_DEFAULT_WATCHDOG_US;
    }

    if (ctx->config.heartbeat_lease_divisor == 0u)
    {
        ctx->config.heartbeat_lease_divisor = LEAP_MGMT_CTRL_DEFAULT_LEASE_DIV;
    }

    ctx->sequence = 1u;
    ctx->state    = LEAP_MGMT_CTRL_IDLE;
}

void leap_mgmt_controller_reset(LeapMgmtControllerContext* ctx)
{
    LeapMgmtControllerConfig saved;

    if (ctx == NULL)
    {
        return;
    }

    saved = ctx->config;
    leap_mgmt_controller_init(ctx, &saved);
}

LeapMgmtControllerStatus leap_mgmt_controller_on_hello_reply(
    LeapMgmtControllerContext* ctx,
    const uint8_t*             source_mac,
    const uint8_t*               payload,
    size_t                       payload_length,
    LeapMgmtControllerEvent*     event)
{
    const LeapHelloReply* hello;

    leap_mgmt_ctrl_clear_event(event);

    if (ctx == NULL || source_mac == NULL || payload == NULL)
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_ERROR, LEAP_STATUS_OK);
        return LEAP_MGMT_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapHelloReply))
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_BAD_LENGTH, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_CTRL_BAD_LENGTH;
    }

    hello = (const LeapHelloReply*)payload;
    memcpy(ctx->peer_mac, source_mac, LEAP_MGMT_CONTROLLER_MAC_LEN);
    ctx->peer_known           = 1u;
    ctx->peer_device_state    = (LeapState_u16)hello->current_state;
    ctx->active_profile_id    = hello->active_profile_id;
    ctx->default_profile_id   = hello->default_profile_id;
    ctx->state                = LEAP_MGMT_CTRL_DISCOVERED;

    if (event != NULL)
    {
        event->flags              |= LEAP_MGMT_CTRL_FLAG_PEER_DISCOVERED;
        event->peer_device_state   = ctx->peer_device_state;
    }

    return LEAP_MGMT_CTRL_OK;
}

size_t leap_mgmt_controller_build_open_session(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity,
    uint32_t                   lease_us,
    uint32_t                   watchdog_us)
{
    LeapOpenSessionRequest* req;

    if (ctx == NULL || payload == NULL ||
        payload_capacity < sizeof(LeapOpenSessionRequest))
    {
        return 0u;
    }

    if (ctx->state != LEAP_MGMT_CTRL_DISCOVERED &&
        ctx->state != LEAP_MGMT_CTRL_SESSION_OPEN &&
        ctx->state != LEAP_MGMT_CTRL_OP)
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapOpenSessionRequest));
    req = (LeapOpenSessionRequest*)payload;
    memcpy(req->controller_mac, ctx->config.controller_mac, LEAP_MGMT_CONTROLLER_MAC_LEN);
    req->open_flags                 = LEAP_OPEN_FLAG_REQUEST_OWNER;
    req->requested_lease_time_us    = (lease_us != 0u) ? lease_us : ctx->config.default_lease_us;
    req->requested_watchdog_time_us =
        (watchdog_us != 0u) ? watchdog_us : ctx->config.default_watchdog_us;

    return sizeof(LeapOpenSessionRequest);
}

LeapMgmtControllerStatus leap_mgmt_controller_on_open_session_reply(
    LeapMgmtControllerContext* ctx,
    const uint8_t*             payload,
    size_t                     payload_length,
    LeapMgmtControllerEvent*   event)
{
    const LeapOpenSessionReply* reply;

    leap_mgmt_ctrl_clear_event(event);

    if (ctx == NULL || payload == NULL)
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_ERROR, LEAP_STATUS_OK);
        return LEAP_MGMT_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapOpenSessionReply))
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_BAD_LENGTH, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_CTRL_BAD_LENGTH;
    }

    reply = (const LeapOpenSessionReply*)payload;
    ctx->session_id          = reply->assigned_session_id;
    ctx->granted_lease_us    = reply->granted_lease_time_us;
    ctx->granted_watchdog_us = reply->granted_watchdog_time_us;
    ctx->session_flags       = reply->session_flags;
    ctx->peer_device_state   = (LeapState_u16)reply->current_state;
    ctx->state               = LEAP_MGMT_CTRL_SESSION_OPEN;

    if (event != NULL)
    {
        event->flags              |= LEAP_MGMT_CTRL_FLAG_SESSION_OPENED;
        event->peer_device_state   = ctx->peer_device_state;
    }

    return LEAP_MGMT_CTRL_OK;
}

size_t leap_mgmt_controller_build_set_state(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity,
    LeapState_u16              requested_state)
{
    LeapSetStateRequest* req;

    if (ctx == NULL || payload == NULL ||
        payload_capacity < sizeof(LeapSetStateRequest))
    {
        return 0u;
    }

    if (ctx->state != LEAP_MGMT_CTRL_SESSION_OPEN && ctx->state != LEAP_MGMT_CTRL_OP)
    {
        return 0u;
    }

    if (ctx->session_id == 0u)
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapSetStateRequest));
    req = (LeapSetStateRequest*)payload;
    req->requested_state = (uint16_t)requested_state;

    return sizeof(LeapSetStateRequest);
}

LeapMgmtControllerStatus leap_mgmt_controller_on_state_reply(
    LeapMgmtControllerContext* ctx,
    const uint8_t*             payload,
    size_t                     payload_length,
    LeapMgmtControllerEvent*   event)
{
    const LeapStateReply* reply;

    leap_mgmt_ctrl_clear_event(event);

    if (ctx == NULL || payload == NULL)
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_ERROR, LEAP_STATUS_OK);
        return LEAP_MGMT_CTRL_ERROR;
    }

    if (payload_length < sizeof(LeapStateReply))
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_BAD_LENGTH, LEAP_STATUS_BAD_LENGTH);
        return LEAP_MGMT_CTRL_BAD_LENGTH;
    }

    reply = (const LeapStateReply*)payload;
    ctx->peer_device_state = (LeapState_u16)reply->current_state;

    if (event != NULL)
    {
        event->flags              |= LEAP_MGMT_CTRL_FLAG_STATE_CHANGED;
        event->peer_device_state   = ctx->peer_device_state;
    }

    if (reply->current_state == (uint16_t)LEAP_STATE_OP)
    {
        ctx->state = LEAP_MGMT_CTRL_OP;
        if (event != NULL)
        {
            event->flags |= LEAP_MGMT_CTRL_FLAG_OP_ENTERED;
        }
    }

    return LEAP_MGMT_CTRL_OK;
}

size_t leap_mgmt_controller_build_heartbeat(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity)
{
    LeapHeartbeatPayload* hb;

    if (ctx == NULL || payload == NULL ||
        payload_capacity < sizeof(LeapHeartbeatPayload))
    {
        return 0u;
    }

    if (ctx->session_id == 0u)
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapHeartbeatPayload));
    hb = (LeapHeartbeatPayload*)payload;
    hb->latest_process_sequence   = ctx->latest_process_sequence;
    hb->current_controller_state  = (uint16_t)LEAP_STATE_OP;

    return sizeof(LeapHeartbeatPayload);
}

size_t leap_mgmt_controller_build_owner_release(
    LeapMgmtControllerContext* ctx,
    uint8_t*                   payload,
    size_t                     payload_capacity,
    uint32_t                   safe_profile_id)
{
    LeapOwnerReleaseRequest* rel;

    if (ctx == NULL || payload == NULL ||
        payload_capacity < sizeof(LeapOwnerReleaseRequest))
    {
        return 0u;
    }

    if (ctx->session_id == 0u)
    {
        return 0u;
    }

    memset(payload, 0, sizeof(LeapOwnerReleaseRequest));
    rel = (LeapOwnerReleaseRequest*)payload;
    rel->requested_safe_profile_id = safe_profile_id;

    return sizeof(LeapOwnerReleaseRequest);
}

LeapMgmtControllerStatus leap_mgmt_controller_on_mgmt_reply(
    LeapMgmtControllerContext* ctx,
    const LeapFrameView*         view,
    LeapMgmtControllerEvent*   event)
{
    const LeapErrorPayload* err;

    leap_mgmt_ctrl_clear_event(event);

    if (ctx == NULL || view == NULL)
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_ERROR, LEAP_STATUS_OK);
        return LEAP_MGMT_CTRL_ERROR;
    }

    if (view->header.service_id != (uint16_t)LEAP_SERVICE_MGMT)
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_UNEXPECTED_REPLY, LEAP_STATUS_UNSUPPORTED_SERVICE);
        return LEAP_MGMT_CTRL_UNEXPECTED_REPLY;
    }

    if ((view->header.flags & LEAP_FLAG_RESPONSE) == 0u)
    {
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_UNEXPECTED_REPLY, LEAP_STATUS_UNSUPPORTED_MESSAGE);
        return LEAP_MGMT_CTRL_UNEXPECTED_REPLY;
    }

    if ((view->header.flags & LEAP_FLAG_ERROR) != 0u)
    {
        if (view->payload_length < sizeof(LeapErrorPayload))
        {
            leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_BAD_LENGTH, LEAP_STATUS_BAD_LENGTH);
            return LEAP_MGMT_CTRL_BAD_LENGTH;
        }

        err = (const LeapErrorPayload*)view->payload;
        ctx->state = LEAP_MGMT_CTRL_STATE_FAULT;
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_ERROR_STATUS, err->status_code);
        return LEAP_MGMT_CTRL_ERROR_STATUS;
    }

    switch (view->header.message_type)
    {
    case LEAP_MGMT_OPEN_SESSION_REPLY:
        return leap_mgmt_controller_on_open_session_reply(
            ctx,
            view->payload,
            view->payload_length,
            event);

    case LEAP_MGMT_STATE_REPLY:
        return leap_mgmt_controller_on_state_reply(
            ctx,
            view->payload,
            view->payload_length,
            event);

    default:
        leap_mgmt_ctrl_set_error(event, LEAP_MGMT_CTRL_UNEXPECTED_REPLY, LEAP_STATUS_UNSUPPORTED_MESSAGE);
        return LEAP_MGMT_CTRL_UNEXPECTED_REPLY;
    }
}

int leap_mgmt_controller_should_send_heartbeat(
    const LeapMgmtControllerContext* ctx,
    uint64_t                         now_us)
{
    uint64_t interval_us;
    uint64_t elapsed_us;

    if (ctx == NULL || ctx->session_id == 0u)
    {
        return 0;
    }

    if (ctx->state != LEAP_MGMT_CTRL_SESSION_OPEN && ctx->state != LEAP_MGMT_CTRL_OP)
    {
        return 0;
    }

    if (ctx->granted_lease_us == 0u)
    {
        return 0;
    }

    interval_us = (uint64_t)ctx->granted_lease_us /
                  (uint64_t)ctx->config.heartbeat_lease_divisor;
    if (interval_us == 0u)
    {
        interval_us = 1u;
    }

    if (ctx->last_lease_refresh_us == 0u)
    {
        return 1;
    }

    elapsed_us = now_us - ctx->last_lease_refresh_us;
    return (elapsed_us >= interval_us) ? 1 : 0;
}

void leap_mgmt_controller_on_heartbeat_sent(
    LeapMgmtControllerContext* ctx,
    uint64_t                   now_us)
{
    if (ctx == NULL)
    {
        return;
    }

    leap_mgmt_ctrl_refresh_lease(ctx, now_us, NULL);
}

void leap_mgmt_controller_on_pd_sent(
    LeapMgmtControllerContext* ctx,
    uint32_t                   process_sequence,
    uint64_t                   now_us)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->latest_process_sequence = process_sequence;
    leap_mgmt_ctrl_refresh_lease(ctx, now_us, NULL);
}

uint32_t leap_mgmt_controller_next_sequence(LeapMgmtControllerContext* ctx)
{
    uint32_t seq;

    if (ctx == NULL)
    {
        return 0u;
    }

    seq = ctx->sequence;
    ctx->sequence++;
    if (ctx->sequence == 0u)
    {
        ctx->sequence = 1u;
    }

    return seq;
}

LeapMgmtControllerState leap_mgmt_controller_get_state(
    const LeapMgmtControllerContext* ctx)
{
    if (ctx == NULL)
    {
        return LEAP_MGMT_CTRL_IDLE;
    }

    return ctx->state;
}

uint32_t leap_mgmt_controller_session_id(
    const LeapMgmtControllerContext* ctx)
{
    if (ctx == NULL)
    {
        return 0u;
    }

    return ctx->session_id;
}

const uint8_t* leap_mgmt_controller_peer_mac(
    const LeapMgmtControllerContext* ctx)
{
    if (ctx == NULL || ctx->peer_known == 0u)
    {
        return NULL;
    }

    return ctx->peer_mac;
}
