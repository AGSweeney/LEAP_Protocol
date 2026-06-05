/*
 * examples/win_smoke/wire_smoke_main.c
 *
 * Windows Npcap loopback wire smoke: bootstrap validation, cyclic PD,
 * lease expiry, and transport statistics.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_win_io.h"
#include "leap_win_smoke.h"

#include "leap/leap_controller_stack.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define LEAP_WIN_RX_BUF 1600u

typedef struct WinSmokeCoopCtx
{
    LeapRawWinpcapSocket* transport;
    LeapDeviceStack*      device;
    unsigned long*        device_frames;
    int*                  hello_reply_gate;
} WinSmokeCoopCtx;

static void win_smoke_device_send_reply(
    WinSmokeCoopCtx*             ctx,
    const uint8_t*               dst_mac,
    const LeapDeviceStackResult* result,
    uint16_t                     service_id,
    uint16_t                     message_type,
    const uint8_t*               payload,
    size_t                       payload_length)
{
    (void)leap_win_send_leap(
        ctx->transport,
        dst_mac,
        (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
        service_id,
        message_type,
        result->frame.header.session_id,
        result->frame.header.sequence,
        result->frame.header.ack_sequence,
        payload,
        payload_length);
}

static void win_smoke_device_feed(
    WinSmokeCoopCtx* ctx,
    const uint8_t*   src_mac,
    const uint8_t*   payload,
    size_t           payload_length)
{
    LeapDeviceStackResult result;
    uint64_t              now_us;

    if (ctx == NULL || ctx->device == NULL || src_mac == NULL ||
        payload == NULL)
    {
        return;
    }

    now_us = leap_raw_winpcap_monotonic_us();

    if (leap_device_stack_process_frame(
            ctx->device,
            src_mac,
            now_us,
            payload,
            payload_length,
            &result) != LEAP_DEVICE_STACK_OK)
    {
        return;
    }

    if (ctx->device_frames != NULL)
    {
        (*ctx->device_frames)++;
    }

    if ((result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u)
    {
        win_smoke_device_send_reply(
            ctx,
            src_mac,
            &result,
            (uint16_t)LEAP_SERVICE_DISC,
            result.disc_message_type,
            result.disc_payload,
            result.disc_payload_length);
    }
    else if ((result.flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u)
    {
        win_smoke_device_send_reply(
            ctx,
            src_mac,
            &result,
            (uint16_t)LEAP_SERVICE_MGMT,
            result.mgmt_reply.message_type,
            result.mgmt_reply.payload,
            result.mgmt_reply.payload_length);
    }
    else if ((result.flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u)
    {
        win_smoke_device_send_reply(
            ctx,
            src_mac,
            &result,
            (uint16_t)LEAP_SERVICE_DIR,
            result.dir_message_type,
            result.dir_payload,
            result.dir_payload_length);
    }
    else if ((result.flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u)
    {
        win_smoke_device_send_reply(
            ctx,
            src_mac,
            &result,
            (uint16_t)LEAP_SERVICE_PD,
            result.pd_reply_message_type,
            result.pd_reply_payload,
            result.pd_reply_payload_length);
    }
}

static int win_smoke_coop_recv(
    WinSmokeCoopCtx* ctx,
    uint8_t*         src_mac,
    uint8_t*         payload,
    size_t           payload_capacity,
    size_t*          payload_length,
    int              timeout_ms)
{
    uint64_t deadline_ms;
    uint64_t now_ms;

    if (ctx == NULL || payload_length == NULL)
    {
        return -1;
    }

    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }

    now_ms      = (uint64_t)GetTickCount64();
    deadline_ms = now_ms + (uint64_t)timeout_ms;

    for (;;)
    {
        int chunk_ms = timeout_ms;

        if (timeout_ms > 0)
        {
            now_ms = (uint64_t)GetTickCount64();
            if (now_ms >= deadline_ms)
            {
                return -1;
            }

            chunk_ms = (int)(deadline_ms - now_ms);
            if (chunk_ms <= 0)
            {
                return -1;
            }
            if (chunk_ms > 100)
            {
                chunk_ms = 100;
            }
        }

        if (leap_win_recv_leap(
                ctx->transport,
                src_mac,
                payload,
                payload_capacity,
                payload_length,
                chunk_ms,
                NULL) != 0)
        {
            if (timeout_ms == 0)
            {
                return -1;
            }

            continue;
        }

        win_smoke_device_feed(ctx, src_mac, payload, *payload_length);

        {
            LeapFrameView view;

            if (leap_frame_parse(payload, *payload_length, &view) != LEAP_FRAME_OK)
            {
                continue;
            }

            if ((view.header.flags & LEAP_FLAG_RESPONSE) == 0u)
            {
                continue;
            }

            if (view.header.service_id == (uint16_t)LEAP_SERVICE_DISC &&
                view.header.message_type == LEAP_DISC_HELLO_REPLY &&
                ctx->hello_reply_gate != NULL)
            {
                if (*ctx->hello_reply_gate != 0)
                {
                    continue;
                }

                *ctx->hello_reply_gate = 1;
            }
        }

        return 0;
    }
}

#if defined(_WIN32)

static int win_smoke_ctrl_io_send(
    void*          user_ctx,
    const uint8_t* dst_mac,
    uint8_t        flags,
    uint16_t       service_id,
    uint16_t       message_type,
    uint32_t       session_id,
    uint32_t       sequence,
    uint32_t       ack_sequence,
    const uint8_t* payload,
    size_t         payload_length)
{
    WinSmokeCoopCtx* ctx = (WinSmokeCoopCtx*)user_ctx;

    if (ctx == NULL)
    {
        return -1;
    }

    return leap_win_send_leap(
        ctx->transport,
        dst_mac,
        flags,
        service_id,
        message_type,
        session_id,
        sequence,
        ack_sequence,
        payload,
        payload_length);
}

static int win_smoke_ctrl_io_recv(
    void*          user_ctx,
    uint8_t*       src_mac,
    uint8_t*       payload_buf,
    size_t         payload_capacity,
    size_t*        payload_length,
    LeapFrameView* parsed,
    int            timeout_ms)
{
    WinSmokeCoopCtx* ctx = (WinSmokeCoopCtx*)user_ctx;

    (void)parsed;

    return win_smoke_coop_recv(
        ctx,
        src_mac,
        payload_buf,
        payload_capacity,
        payload_length,
        timeout_ms);
}

static uint64_t win_smoke_ctrl_io_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

static void win_smoke_pump_pending(WinSmokeCoopCtx* ctx, unsigned max_frames)
{
    uint8_t  src_mac[6];
    uint8_t  payload[LEAP_WIN_RX_BUF];
    size_t   payload_length;
    unsigned i;

    if (ctx == NULL)
    {
        return;
    }

    for (i = 0u; i < max_frames; i++)
    {
        if (leap_win_recv_leap(
                ctx->transport,
                src_mac,
                payload,
                sizeof(payload),
                &payload_length,
                10,
                NULL) != 0)
        {
            break;
        }

        win_smoke_device_feed(ctx, src_mac, payload, payload_length);
    }
}

static int win_smoke_pd_send(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    WinSmokeCoopCtx* ctx = (WinSmokeCoopCtx*)user_ctx;
    int              result;

    result = leap_win_send_leap(
        ctx->transport,
        peer_mac,
        0u,
        (uint16_t)LEAP_SERVICE_PD,
        message_type,
        session_id,
        sequence,
        0u,
        payload,
        payload_length);
    if (result != 0)
    {
        return result;
    }

    if (message_type == LEAP_PD_WRITE_ENDPOINT)
    {
        win_smoke_pump_pending(ctx, 8u);
    }

    return 0;
}

static int win_smoke_pd_send_heartbeat(
    void*          user_ctx,
    const uint8_t* peer_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    WinSmokeCoopCtx* ctx = (WinSmokeCoopCtx*)user_ctx;

    return leap_win_send_leap(
        ctx->transport,
        peer_mac,
        0u,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_HEARTBEAT,
        session_id,
        sequence,
        0u,
        payload,
        payload_length);
}

static int win_smoke_pd_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms,
    uint64_t*      reply_recv_us_out)
{
    WinSmokeCoopCtx* ctx = (WinSmokeCoopCtx*)user_ctx;
    LeapFrameView    view;
    uint8_t          src_mac[6];
    uint8_t          frame_buf[512];
    size_t           frame_length;

    if (ctx == NULL || reply_length == NULL || peer_mac == NULL)
    {
        return -1;
    }

    if (reply_recv_us_out != NULL)
    {
        *reply_recv_us_out = 0u;
    }

    for (;;)
    {
        if (win_smoke_coop_recv(
                ctx,
                src_mac,
                frame_buf,
                sizeof(frame_buf),
                &frame_length,
                timeout_ms) != 0)
        {
            return -1;
        }

        if (leap_frame_parse(frame_buf, frame_length, &view) != LEAP_FRAME_OK)
        {
            continue;
        }

        if (view.header.service_id != (uint16_t)LEAP_SERVICE_PD ||
            view.header.message_type != LEAP_PD_EXCHANGE_REPLY)
        {
            continue;
        }

        (void)peer_mac;
        (void)src_mac;

        if (view.payload_length > reply_capacity)
        {
            return -1;
        }

        *reply_length = view.payload_length;
        memcpy(reply_payload, view.payload, view.payload_length);
        if (reply_recv_us_out != NULL)
        {
            *reply_recv_us_out = leap_raw_winpcap_monotonic_us();
        }
        return 0;
    }
}

static uint64_t win_smoke_pd_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

static void win_smoke_pump_cb(void* user_ctx, unsigned max_frames)
{
    win_smoke_pump_pending((WinSmokeCoopCtx*)user_ctx, max_frames);
}

static void win_smoke_fail_bootstrap(
    const LeapControllerStack* stack,
    unsigned long              device_frames,
    const LeapRawWinpcapSocket* transport)
{
    LeapRawWinpcapStats stats;

    leap_raw_winpcap_get_stats(transport, &stats);
    fprintf(
        stderr,
        "bootstrap failed (phase=%u status=%d device_frames=%lu tx=%llu rx=%llu)\n",
        (unsigned)leap_controller_stack_get_phase(stack),
        (int)stack->last_status,
        device_frames,
        (unsigned long long)stats.tx_frames_ok,
        (unsigned long long)stats.rx_frames_ok);
}

int main(int argc, char** argv)
{
    LeapWinSmokeOptions        options;
    LeapWinSmokeReport         report;
    LeapRawWinpcapSocket       transport;
    LeapDeviceStack            device_stack;
    LeapDeviceStackConfig      device_config;
    LeapPdDeviceIoBinding      pd_binding;
    LeapControllerStack        ctrl_stack;
    LeapControllerStackConfig  ctrl_config;
    LeapControllerStackIo      stack_io;
    LeapPdControllerIo         pd_io;
    WinSmokeCoopCtx            coop;
    uint16_t                   digital_outputs = 0u;
    uint16_t                   digital_inputs  = 0x0004u;
    uint16_t                   io_status       = 0u;
    unsigned long              device_frames   = 0u;
    int                        hello_reply_gate;
    uint8_t                    peer_mac[6];
    uint32_t                   tick_flags;
    int                        parse_result;

    parse_result = leap_win_smoke_parse_args(argc, argv, &options);
    if (parse_result == 1)
    {
        leap_win_smoke_print_usage(argv[0]);
        return 0;
    }

    if (parse_result != 0)
    {
        leap_win_smoke_print_usage(argv[0]);
        return 1;
    }

    if (options.list_adapters != 0)
    {
        leap_win_smoke_print_adapter_hint();
        return 0;
    }

    leap_win_smoke_console_init(&options);
    leap_win_smoke_report_init(&report, &options);

    printf("LEAP Windows wire smoke\n");

    if (leap_win_smoke_open_transport(&transport, &options) != 0)
    {
        return 1;
    }

    printf("adapter: %s\n", transport.device_name);
    leap_win_print_mac("  MAC: ", transport.local_mac);

    memset(&device_config, 0, sizeof(device_config));
    device_config.mgmt.default_lease_us    = LEAP_WIN_SMOKE_DEFAULT_LEASE_US;
    device_config.mgmt.default_watchdog_us = 500000u;
    leap_device_stack_init_full(&device_stack, &device_config);

    memset(&pd_binding, 0, sizeof(pd_binding));
    pd_binding.digital_outputs = &digital_outputs;
    pd_binding.digital_inputs  = &digital_inputs;
    pd_binding.io_status       = &io_status;
    leap_device_stack_bind_pd_io(&device_stack, &pd_binding);

    memcpy(device_stack.dir.config.identity.primary_mac, transport.local_mac, 6);
    memcpy(
        device_stack.disc.config.identity.primary_mac,
        transport.local_mac,
        6);
    leap_dir_device_sync_disc(&device_stack.dir, &device_stack.disc);
    leap_mgmt_device_on_transport_ready(&device_stack.mgmt);

    hello_reply_gate = 0;
    memset(&coop, 0, sizeof(coop));
    coop.transport         = &transport;
    coop.device            = &device_stack;
    coop.device_frames     = &device_frames;
    coop.hello_reply_gate  = &hello_reply_gate;

    memset(&ctrl_config, 0, sizeof(ctrl_config));
    memcpy(ctrl_config.mgmt.controller_mac, transport.local_mac, 6);
    ctrl_config.bootstrap_lease_us = LEAP_WIN_SMOKE_DEFAULT_LEASE_US;
    ctrl_config.recv_timeout_ms    = 3000;
    ctrl_config.pd.cycle_period_ms = options.cycle_ms;
    leap_controller_stack_init(&ctrl_stack, &ctrl_config);

    memset(&stack_io, 0, sizeof(stack_io));
    stack_io.user_ctx     = &coop;
    stack_io.send_frame   = win_smoke_ctrl_io_send;
    stack_io.recv_frame   = win_smoke_ctrl_io_recv;
    stack_io.monotonic_us = win_smoke_ctrl_io_monotonic;

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.user_ctx            = &coop;
    pd_io.send_pd             = win_smoke_pd_send;
    pd_io.send_heartbeat      = win_smoke_pd_send_heartbeat;
    pd_io.wait_exchange_reply = win_smoke_pd_wait_exchange_reply;
    pd_io.monotonic_us        = win_smoke_pd_monotonic;

    tick_flags = 0u;
    (void)leap_device_stack_tick(
        &device_stack,
        leap_raw_winpcap_monotonic_us(),
        &tick_flags);

    if (leap_controller_stack_bootstrap(&ctrl_stack, &stack_io, peer_mac) !=
        LEAP_CTRL_STACK_OK)
    {
        win_smoke_fail_bootstrap(&ctrl_stack, device_frames, &transport);
        leap_win_smoke_fail(&report, "bootstrap");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    printf("bootstrap complete - peer ");
    leap_win_print_mac(NULL, peer_mac);
    leap_win_smoke_pass(&report, "bootstrap");

    if (leap_win_smoke_validate_bootstrap(
            &ctrl_stack,
            &device_stack,
            transport.local_mac) != 0)
    {
        leap_win_smoke_fail(&report, "bootstrap validation");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    leap_win_smoke_pass(&report, "bootstrap validation");

    if (options.verbose != 0)
    {
        printf(
            "  controller seq=%u pd_seq=%u\n",
            (unsigned)ctrl_stack.mgmt.sequence,
            (unsigned)ctrl_stack.pd.pd_sequence);
    }

    if (leap_controller_stack_pd_single_write(
            &ctrl_stack, &pd_io, options.pd_outputs) != LEAP_PD_CTRL_OK)
    {
        fprintf(stderr, "error: initial PD write failed\n");
        leap_win_smoke_fail(&report, "PD write");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    win_smoke_pump_pending(&coop, 4u);
    leap_win_smoke_pass(&report, "PD write");

    if (leap_win_smoke_validate_outputs(digital_outputs, options.pd_outputs) != 0)
    {
        leap_win_smoke_fail(&report, "outputs");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    leap_win_smoke_pass(&report, "outputs");

    if (options.skip_cyclic == 0)
    {
        if (leap_win_smoke_run_cyclic(
                &ctrl_stack, &pd_io, &options, &report) != 0)
        {
            leap_win_smoke_fail(&report, "cyclic PD");
            leap_raw_winpcap_close(&transport);
            return 1;
        }
    }

    if (options.skip_lease_test == 0)
    {
        (void)leap_controller_stack_release(&ctrl_stack, &stack_io);
        win_smoke_pump_pending(&coop, 32u);
        leap_win_smoke_reset_hello_gate(&hello_reply_gate);

        memset(&device_config, 0, sizeof(device_config));
        device_config.mgmt.default_lease_us    = LEAP_WIN_SMOKE_DEFAULT_LEASE_US;
        device_config.mgmt.default_watchdog_us = 500000u;
        leap_device_stack_init_full(&device_stack, &device_config);
        leap_device_stack_bind_pd_io(&device_stack, &pd_binding);
        memcpy(device_stack.dir.config.identity.primary_mac, transport.local_mac, 6);
        memcpy(
            device_stack.disc.config.identity.primary_mac,
            transport.local_mac,
            6);
        leap_dir_device_sync_disc(&device_stack.dir, &device_stack.disc);
        leap_mgmt_device_on_transport_ready(&device_stack.mgmt);
        digital_outputs = 0u;

        if (leap_win_smoke_run_lease_expiry(
                &ctrl_stack,
                &stack_io,
                &pd_io,
                &device_stack,
                &hello_reply_gate,
                transport.local_mac,
                &digital_outputs,
                options.pd_outputs,
                win_smoke_pump_cb,
                &coop,
                &report) != 0)
        {
            leap_win_smoke_fail(&report, "lease expiry test");
            leap_raw_winpcap_close(&transport);
            return 1;
        }
    }

    if (device_frames == 0u)
    {
        fprintf(stderr, "error: device did not receive any LEAP frames\n");
        leap_win_smoke_fail(&report, "device traffic");
        leap_controller_stack_release(&ctrl_stack, &stack_io);
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    leap_win_smoke_pass(&report, "device traffic");

    leap_controller_stack_release(&ctrl_stack, &stack_io);
    leap_win_smoke_print_transport_stats(&transport);
    leap_raw_winpcap_close(&transport);

    leap_win_smoke_print_summary(&report);

    printf(
        "Windows wire smoke: OK (device_frames=%lu pd_seq=%u)\n",
        device_frames,
        (unsigned)ctrl_stack.pd.pd_sequence);
    return 0;
}

#else

int main(void)
{
    fprintf(stderr, "leap_win_smoke requires Windows/Npcap.\n");
    return 1;
}

#endif
