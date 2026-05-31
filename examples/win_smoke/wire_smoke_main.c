/*
 * examples/win_smoke/wire_smoke_main.c
 *
 * Windows Npcap loopback wire smoke: in-process device + controller on one
 * pcap handle. Npcap loopback uses an in-driver relay when the OS does not
 * capture injected frames (see leap_raw_winpcap.c).
 *
 * Usage:
 *   leap_win_smoke.exe
 *   leap_win_smoke.exe \Device\NPF_Loopback
 *
 * Requires Npcap (wpcap.dll) with the Loopback adapter enabled.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_win_io.h"

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
    int                   hello_reply_returned;
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
                chunk_ms) != 0)
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
                view.header.message_type == LEAP_DISC_HELLO_REPLY)
            {
                if (ctx->hello_reply_returned != 0)
                {
                    continue;
                }

                ctx->hello_reply_returned = 1;
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

    return leap_win_send_leap(
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
}

static int win_smoke_pd_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms)
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

        if (memcmp(src_mac, peer_mac, 6) != 0)
        {
            continue;
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

        if (view.payload_length > reply_capacity)
        {
            return -1;
        }

        *reply_length = view.payload_length;
        memcpy(reply_payload, view.payload, view.payload_length);
        return 0;
    }
}

static uint64_t win_smoke_pd_monotonic(void* user_ctx)
{
    (void)user_ctx;
    return leap_raw_winpcap_monotonic_us();
}

int main(int argc, char** argv)
{
    LeapRawWinpcapSocket       transport;
    LeapRawWinpcapOpenOptions  open_options;
    LeapDeviceStack            device_stack;
    LeapDeviceStackConfig      device_config;
    LeapPdDeviceIoBinding      pd_binding;
    LeapControllerStack        ctrl_stack;
    LeapControllerStackConfig  ctrl_config;
    LeapControllerStackIo      stack_io;
    LeapPdControllerIo         pd_io;
    WinSmokeCoopCtx            coop;
    char                       adapter_name[LEAP_RAW_WINPCAP_NAME_MAX];
    uint16_t                   digital_outputs = 0u;
    uint16_t                   digital_inputs  = 0x0004u;
    uint16_t                   io_status       = 0u;
    unsigned long              device_frames   = 0u;
    uint8_t                    peer_mac[6];
    uint32_t                   tick_flags;

    adapter_name[0] = '\0';
    if (argc > 1 && argv[1][0] != '\0')
    {
        (void)snprintf(adapter_name, sizeof(adapter_name), "%s", argv[1]);
    }

    printf("LEAP Windows wire smoke (Npcap loopback)\n");

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = 1;
    open_options.filter_leap_ethertype = 0;

    if (leap_raw_winpcap_open(
            &transport,
            adapter_name[0] != '\0' ? adapter_name : NULL,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        fprintf(
            stderr,
            "pcap open failed: %s\n",
            leap_raw_winpcap_last_error());
        leap_raw_winpcap_list_devices();
        return 1;
    }

    printf("adapter: %s\n", transport.device_name);
    leap_win_print_mac("  MAC: ", transport.local_mac);

    memset(&device_config, 0, sizeof(device_config));
    device_config.mgmt.default_lease_us    = 5000000u;
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

    memset(&coop, 0, sizeof(coop));
    coop.transport     = &transport;
    coop.device        = &device_stack;
    coop.device_frames = &device_frames;

    memset(&ctrl_config, 0, sizeof(ctrl_config));
    memcpy(ctrl_config.mgmt.controller_mac, transport.local_mac, 6);
    ctrl_config.bootstrap_lease_us = 5000000u;
    ctrl_config.recv_timeout_ms    = 3000;
    leap_controller_stack_init(&ctrl_stack, &ctrl_config);

    memset(&stack_io, 0, sizeof(stack_io));
    stack_io.user_ctx     = &coop;
    stack_io.send_frame   = win_smoke_ctrl_io_send;
    stack_io.recv_frame   = win_smoke_ctrl_io_recv;
    stack_io.monotonic_us = win_smoke_ctrl_io_monotonic;

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.user_ctx            = &coop;
    pd_io.send_pd             = win_smoke_pd_send;
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
        LeapRawWinpcapStats stats;

        leap_raw_winpcap_get_stats(&transport, &stats);
        fprintf(
            stderr,
            "bootstrap failed (phase=%u status=%d device_frames=%lu tx=%llu rx=%llu)\n",
            (unsigned)leap_controller_stack_get_phase(&ctrl_stack),
            (int)ctrl_stack.last_status,
            device_frames,
            (unsigned long long)stats.tx_frames_ok,
            (unsigned long long)stats.rx_frames_ok);
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    printf("bootstrap complete — peer ");
    leap_win_print_mac(NULL, peer_mac);

    if (leap_controller_stack_pd_single_write(
            &ctrl_stack, &pd_io, 0x0015u) != LEAP_PD_CTRL_OK)
    {
        fprintf(stderr, "PD single write failed\n");
        leap_controller_stack_release(&ctrl_stack, &stack_io);
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    printf("sent PD WRITE (outputs=0x0015)\n");

    if (device_frames == 0u)
    {
        fprintf(stderr, "device did not receive any LEAP frames\n");
        leap_controller_stack_release(&ctrl_stack, &stack_io);
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    leap_controller_stack_release(&ctrl_stack, &stack_io);
    leap_raw_winpcap_close(&transport);

    printf("Windows wire smoke: OK (device_frames=%lu)\n", device_frames);
    return 0;
}

#else

int main(void)
{
    fprintf(stderr, "leap_win_smoke requires Windows/Npcap.\n");
    return 1;
}

#endif
