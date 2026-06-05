/*
 * leap_commissioning.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/conformance/leap_commissioning.h"
#include "leap/conformance/leap_conformance_raw_io.h"

#include "leap/leap_disc_controller.h"
#include "leap/leap_frame.h"
#include "leap/leap_log.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct LeapCommissioningContext
{
    LeapRawWinpcapSocket      transport;
    LeapConformanceRawIo      raw_io;
    LeapControllerStack       stack;
    LeapControllerPeerTable   table;
    LeapCommissioningConfig   config;
    int                       transport_open;
    volatile int              cancel_flag;
};

static LeapCommissioningStatus leap_comm_open_transport(
    LeapCommissioningContext* ctx,
    const char*               adapter)
{
    LeapRawWinpcapOpenOptions open_options;

    if (ctx == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    if (ctx->transport_open)
    {
        leap_raw_winpcap_close(&ctx->transport);
        ctx->transport_open = 0;
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = ctx->config.promiscuous;
    open_options.filter_leap_ethertype = 1;

    if (leap_raw_winpcap_open(
            &ctx->transport,
            adapter,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        return LEAP_COMM_TRANSPORT_ERROR;
    }

    (void)leap_raw_winpcap_drain_rx(&ctx->transport);
    leap_conformance_raw_io_bind(&ctx->raw_io, &ctx->transport);
    ctx->transport_open = 1;
    return LEAP_COMM_OK;
}

LeapCommissioningStatus leap_commissioning_open(
    LeapCommissioningContext**      ctx_out,
    const LeapCommissioningConfig*  config)
{
    LeapCommissioningContext* ctx;

    if (ctx_out == NULL || config == NULL || config->adapter == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    ctx = (LeapCommissioningContext*)calloc(1, sizeof(*ctx));
    if (ctx == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    ctx->config = *config;
    if (ctx->config.recv_timeout_ms <= 0)
    {
        ctx->config.recv_timeout_ms = 1000;
    }

    if (leap_comm_open_transport(ctx, config->adapter) != LEAP_COMM_OK)
    {
        free(ctx);
        return LEAP_COMM_TRANSPORT_ERROR;
    }

    leap_controller_peer_table_init(&ctx->table);
    *ctx_out = ctx;
    return LEAP_COMM_OK;
}

void leap_commissioning_close(LeapCommissioningContext* ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->transport_open)
    {
        leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io);
        leap_raw_winpcap_close(&ctx->transport);
    }

    free(ctx);
}

LeapCommissioningStatus leap_commissioning_discover(
    LeapCommissioningContext*        ctx,
    int                              scan_ms,
    LeapCommissioningDiscoverResult* result_out)
{
    LeapControllerPeerDiscoverConfig disc_config;
    LeapControllerPeerStatus         status;
    unsigned                         i;

    if (ctx == NULL || result_out == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    memset(result_out, 0, sizeof(*result_out));
    leap_controller_peer_table_init(&result_out->table);

    memset(&disc_config, 0, sizeof(disc_config));
    disc_config.scan_duration_ms = scan_ms > 0 ? scan_ms : 2000;
    disc_config.min_peers        = 0u;

    status = leap_controller_peer_table_discover_ex(
        &result_out->table,
        &ctx->raw_io.stack_io,
        &disc_config);
    if (status != LEAP_CTRL_PEER_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    memcpy(&ctx->table, &result_out->table, sizeof(ctx->table));

    for (i = 0u; i < result_out->table.count; i++)
    {
        result_out->has_hello[i] = 0;
    }

    (void)i;
    return LEAP_COMM_OK;
}

static void leap_comm_reset_stack(LeapCommissioningContext* ctx)
{
    LeapControllerStackConfig stack_config;

    memset(&stack_config, 0, sizeof(stack_config));
    memcpy(stack_config.mgmt.controller_mac, ctx->transport.local_mac, 6);
    stack_config.bootstrap_lease_us     = 5000000u;
    stack_config.recv_timeout_ms        = ctx->config.recv_timeout_ms;
    stack_config.single_peer_auto_select = 0;
    leap_controller_stack_init(&ctx->stack, &stack_config);
}

LeapCommissioningStatus leap_commissioning_bootstrap(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    uint32_t                  lease_us)
{
    if (ctx == NULL || peer_mac == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    leap_comm_reset_stack(ctx);
    if (lease_us > 0u)
    {
        ctx->stack.config.bootstrap_lease_us = lease_us;
    }
    memcpy(ctx->stack.config.target_peer_mac, peer_mac, 6);

    if (leap_controller_stack_bootstrap(&ctx->stack, &ctx->raw_io.stack_io, NULL) !=
        LEAP_CTRL_STACK_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    return LEAP_COMM_OK;
}

LeapCommissioningStatus leap_commissioning_set_op(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac)
{
    return leap_commissioning_bootstrap(ctx, peer_mac, 0u);
}

LeapCommissioningStatus leap_commissioning_set_safe(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac)
{
    (void)peer_mac;
    if (ctx == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    if (leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io) !=
        LEAP_CTRL_STACK_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    return LEAP_COMM_OK;
}

LeapCommissioningStatus leap_commissioning_release(LeapCommissioningContext* ctx)
{
    if (ctx == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    if (leap_controller_stack_release(&ctx->stack, &ctx->raw_io.stack_io) !=
        LEAP_CTRL_STACK_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    return LEAP_COMM_OK;
}

static int leap_comm_wait_disc_reply(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    uint16_t                  expect_type,
    uint8_t*                  reply,
    size_t                    reply_cap,
    size_t*                   reply_len)
{
    uint64_t deadline;

    if (ctx == NULL || peer_mac == NULL || reply == NULL || reply_len == NULL)
    {
        return -1;
    }

    deadline = leap_raw_winpcap_monotonic_us() +
               (uint64_t)ctx->config.recv_timeout_ms * 1000u;

    for (;;)
    {
        uint8_t       src_mac[6];
        uint8_t       payload[1600];
        size_t        length = 0u;
        LeapFrameView view;
        int           timeout_ms;
        uint64_t      now_us;

        now_us = leap_raw_winpcap_monotonic_us();
        if (now_us >= deadline)
        {
            return -1;
        }

        timeout_ms = (int)((deadline - now_us + 999u) / 1000u);
        if (timeout_ms <= 0)
        {
            timeout_ms = 1;
        }

        if (leap_raw_winpcap_recv(
                &ctx->transport,
                src_mac,
                payload,
                sizeof(payload),
                &length,
                timeout_ms,
                NULL) != 0)
        {
            return -1;
        }

        if (memcmp(src_mac, peer_mac, 6) != 0)
        {
            continue;
        }

        if (leap_frame_parse(payload, length, &view) != LEAP_FRAME_OK)
        {
            continue;
        }

        if (view.header.service_id != (uint16_t)LEAP_SERVICE_DISC ||
            view.header.message_type != expect_type)
        {
            continue;
        }

        if (view.payload_length > reply_cap)
        {
            return -1;
        }

        if (view.payload_length > 0u && view.payload != NULL)
        {
            memcpy(reply, view.payload, view.payload_length);
        }

        *reply_len = view.payload_length;
        return 0;
    }
}

LeapCommissioningStatus leap_commissioning_identify(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    LeapIdentifyReply*        reply_out)
{
    uint8_t payload[64];
    size_t  payload_length;
    uint8_t reply_payload[128];
    size_t  reply_length = 0u;

    if (ctx == NULL || peer_mac == NULL || reply_out == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    (void)leap_commissioning_release(ctx);

    payload_length = leap_disc_controller_build_identify(
        NULL,
        0u,
        payload,
        sizeof(payload));
    if (payload_length == 0u)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    if (leap_conformance_raw_send_disc(
            &ctx->raw_io,
            peer_mac,
            LEAP_DISC_IDENTIFY,
            payload,
            payload_length) != 0)
    {
        return LEAP_COMM_TRANSPORT_ERROR;
    }

    if (leap_comm_wait_disc_reply(
            ctx,
            peer_mac,
            LEAP_DISC_IDENTIFY_REPLY,
            reply_payload,
            sizeof(reply_payload),
            &reply_length) != 0)
    {
        return LEAP_COMM_TIMEOUT;
    }

    if (leap_disc_controller_on_identify_reply(
            reply_payload,
            reply_length,
            reply_out) != LEAP_DISC_CTRL_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    return LEAP_COMM_OK;
}

LeapCommissioningStatus leap_commissioning_locate(
    LeapCommissioningContext* ctx,
    const uint8_t*            peer_mac,
    unsigned                  duration_ms,
    unsigned                  pattern,
    LeapLocateDeviceReply*    reply_out)
{
    uint8_t payload[64];
    size_t  payload_length;
    uint8_t reply_payload[64];
    size_t  reply_length = 0u;

    if (ctx == NULL || peer_mac == NULL || reply_out == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    (void)leap_commissioning_release(ctx);

    payload_length = leap_disc_controller_build_locate_device(
        (uint16_t)duration_ms,
        (uint8_t)pattern,
        LEAP_LOCATE_FLAG_LED,
        payload,
        sizeof(payload));
    if (payload_length == 0u)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    if (leap_conformance_raw_send_disc(
            &ctx->raw_io,
            peer_mac,
            LEAP_DISC_LOCATE_DEVICE,
            payload,
            payload_length) != 0)
    {
        return LEAP_COMM_TRANSPORT_ERROR;
    }

    if (leap_comm_wait_disc_reply(
            ctx,
            peer_mac,
            LEAP_DISC_LOCATE_DEVICE_REPLY,
            reply_payload,
            sizeof(reply_payload),
            &reply_length) != 0)
    {
        return LEAP_COMM_TIMEOUT;
    }

    if (leap_disc_controller_on_locate_device_reply(
            reply_payload,
            reply_length,
            reply_out) != LEAP_DISC_CTRL_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    return LEAP_COMM_OK;
}

LeapCommissioningStatus leap_commissioning_pd_write(
    LeapCommissioningContext* ctx,
    uint16_t                  outputs)
{
    if (ctx == NULL)
    {
        return LEAP_COMM_INVALID_ARG;
    }

    if (leap_controller_stack_get_phase(&ctx->stack) != LEAP_CTRL_STACK_OP)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    if (leap_controller_stack_pd_single_write(
            &ctx->stack, &ctx->raw_io.pd_io, outputs) !=
        LEAP_PD_CTRL_OK)
    {
        return LEAP_COMM_PROTOCOL_ERROR;
    }

    return LEAP_COMM_OK;
}
