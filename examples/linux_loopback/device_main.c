/*
 * examples/linux_loopback/device_main.c
 *
 * Linux AF_PACKET LEAP device: DISC + DIR + MGMT + PD with tick loop and I/O shadow.
 *
 * Usage:
 *   sudo ./leap_linux_device [interface]
 *   sudo ./leap_linux_device --stats [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"
#include "leap_linux_io.h"
#include "leap_linux_stats.h"

#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_log.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#define LEAP_RX_BUF_SIZE 1600u
#define LEAP_DEVICE_STATS_INTERVAL 100u

static LeapLinuxIoShadow     g_io;
static LeapLinuxDeviceStats  g_stats;
static int                   g_stats_enabled = 0;

static void device_send_reply(
    LeapDeviceStack*             stack,
    const LeapRawLinuxSocket*    transport,
    const uint8_t*               dst_mac,
    const LeapDeviceStackResult* result,
    uint16_t                     service_id,
    uint16_t                     message_type,
    const uint8_t*               payload,
    size_t                         payload_length)
{
    if (leap_linux_send_leap(
            transport,
            dst_mac,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            payload,
            payload_length) != 0)
    {
        if (stack != NULL)
        {
            leap_device_stack_notify_tx_drop(stack);
        }
        return;
    }

    if (stack != NULL)
    {
        leap_device_stack_notify_tx_ok(stack, leap_raw_linux_monotonic_us());
    }

    printf("sent reply (service=0x%04X message=0x%04X)\n", service_id, message_type);
}

static void device_send_error_reply(
    LeapDeviceStack*             stack,
    const LeapRawLinuxSocket*    transport,
    const uint8_t*               dst_mac,
    const LeapDeviceStackResult* result,
    uint16_t                     service_id,
    uint16_t                     message_type,
    uint16_t                     status_code)
{
    LeapErrorPayload err;

    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

    if (leap_linux_send_leap(
            transport,
            dst_mac,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            (const uint8_t*)&err,
            sizeof(err)) != 0)
    {
        if (stack != NULL)
        {
            leap_device_stack_notify_tx_drop(stack);
        }
        return;
    }

    if (stack != NULL)
    {
        leap_device_stack_notify_tx_ok(stack, leap_raw_linux_monotonic_us());
    }

    printf("sent error reply (service=0x%04X message=0x%04X status=0x%04X)\n",
           service_id,
           message_type,
           status_code);
}

static void device_log_rx(const LeapDeviceStackResult* result)
{
    switch (result->service_id)
    {
    case LEAP_SERVICE_DISC:
        if (result->frame.header.message_type == LEAP_DISC_HELLO)
        {
            printf("received HELLO\n");
        }
        break;
    case LEAP_SERVICE_DIR:
        if (result->frame.header.message_type == LEAP_DIR_SELECT_PROFILE)
        {
            printf("received SELECT_PROFILE\n");
        }
        else if (result->frame.header.message_type == LEAP_DIR_READ_DIRECTORY)
        {
            printf("received READ_DIRECTORY\n");
        }
        break;
    case LEAP_SERVICE_MGMT:
        if (result->frame.header.message_type == LEAP_MGMT_OPEN_SESSION)
        {
            printf("received OPEN_SESSION\n");
        }
        else if (result->frame.header.message_type == LEAP_MGMT_SET_STATE)
        {
            printf("received SET_STATE -> state now %u\n",
                   (unsigned)result->device_state);
        }
        else if (result->frame.header.message_type == LEAP_MGMT_HEARTBEAT)
        {
            printf("received HEARTBEAT (lease refreshed)\n");
        }
        break;
    case LEAP_SERVICE_PD:
        if (result->frame.header.message_type == LEAP_PD_WRITE_ENDPOINT)
        {
            printf("received PD WRITE_ENDPOINT (state=%u)\n",
                   (unsigned)result->device_state);
        }
        else if (result->frame.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS)
        {
            printf("received PD EXCHANGE_ENDPOINTS (state=%u)\n",
                   (unsigned)result->device_state);
        }
        break;
    default:
        break;
    }
}

static void device_apply_pd_result(const LeapDeviceStackResult* result)
{
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) == 0u)
    {
        return;
    }

    leap_linux_io_apply_outputs(&g_io, result->pd_outputs_applied);

    printf("PD outputs applied: 0x%04X",
           result->pd_outputs_applied);
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_INPUTS_READ) != 0u)
    {
        printf(" inputs=0x%04X", result->pd_inputs_snapshot);
    }
    printf("\n");
}

static void device_log_tick(uint32_t flags, LeapState_u16 state)
{
    if ((flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u)
    {
        printf("tick: lease/watchdog expired -> SAFE (state=%u)\n", (unsigned)state);
        leap_linux_io_enter_safe(&g_io);
    }
    else if ((flags & LEAP_DEVICE_STACK_FLAG_OWNERSHIP_CHANGED) != 0u)
    {
        printf("tick: ownership changed (state=%u)\n", (unsigned)state);
    }
}

static void device_parse_args(int argc, char** argv, const char** ifname_out)
{
    int i;

    *ifname_out = "lo";

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--stats") == 0)
        {
            g_stats_enabled = 1;
        }
        else if (argv[i][0] != '-')
        {
            *ifname_out = argv[i];
        }
    }
}

int main(int argc, char** argv)
{
    const char*           ifname = "lo";
    LeapRawLinuxSocket    transport;
    LeapDeviceStack       stack;
    LeapDeviceStackResult result;
    LeapDeviceStackConfig stack_config;
    LeapPdDeviceIoBinding pd_io;
    uint8_t               rx[LEAP_RX_BUF_SIZE];
    uint8_t               src_mac[6];
    size_t                rx_len = 0u;
    uint32_t              tick_flags = 0u;
    uint64_t              now_us;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_device requires Linux AF_PACKET support.\n");
    return 1;
#else
    device_parse_args(argc, argv, &ifname);
    leap_log_reset_origin();

    leap_linux_io_init(&g_io);
    leap_linux_device_stats_init(&g_stats);

    memset(&stack_config, 0, sizeof(stack_config));
    stack_config.mgmt.default_lease_us    = 5000000u;
    stack_config.mgmt.default_watchdog_us = 500000u;
    stack_config.mgmt.max_lease_us        = 10000000u;
    stack_config.mgmt.max_watchdog_us     = 1000000u;
    (void)leap_dir_device_config_set_digital_io(
        &stack_config.dir,
        LEAP_PROFILE_DIGITAL_IO_16X16,
        16u,
        16u);

    leap_device_stack_init_full(&stack, &stack_config);

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.digital_outputs = &g_io.digital_outputs;
    pd_io.digital_inputs  = &g_io.digital_inputs;
    pd_io.io_status       = &g_io.io_status;
    leap_device_stack_bind_pd_io(&stack, &pd_io);

    if (leap_raw_linux_open(&transport, ifname, LEAP_ETHERTYPE_DEVELOPMENT) != 0)
    {
        leap_linux_print_transport_error("open");
        fprintf(stderr, "need CAP_NET_RAW/root on interface %s\n", ifname);
        return 1;
    }

    memcpy(stack.dir.config.identity.primary_mac, transport.local_mac, 6);
    memcpy(stack.disc.config.identity.primary_mac, transport.local_mac, 6);
    leap_dir_device_sync_disc(&stack.dir, &stack.disc);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);

    printf("LEAP device on %s\n", ifname);
    leap_linux_print_mac("  local MAC: ", transport.local_mac);
    printf("  state: INIT — waiting for SELECT_PROFILE (I/O shadow in safe mode)\n");
    if (g_stats_enabled != 0)
    {
        printf("  stats: enabled (log every %u frames)\n", LEAP_DEVICE_STATS_INTERVAL);
    }

    for (;;)
    {
        now_us = leap_raw_linux_monotonic_us();
        leap_linux_poll_link_and_log(&transport);

        if (leap_linux_recv_leap(
                &transport,
                src_mac,
                rx,
                sizeof(rx),
                &rx_len,
                100) == 0)
        {
            LeapDeviceStackStatus status;

            leap_linux_device_stats_on_frame(&g_stats);

            status = leap_device_stack_process_frame(
                &stack,
                src_mac,
                now_us,
                rx,
                rx_len,
                &result);

            if (status == LEAP_DEVICE_STACK_OK)
            {
                device_log_rx(&result);
                device_apply_pd_result(&result);
                leap_linux_device_stats_on_result(&g_stats, result.flags);

                if ((result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u)
                {
                    device_send_reply(
                        &stack,
                        &transport,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_DISC,
                        result.disc_message_type,
                        result.disc_payload,
                        result.disc_payload_length);
                }
                else if ((result.flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u)
                {
                    if ((result.flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u)
                    {
                        printf("ownership granted -> SAFE\n");
                    }

                    device_send_reply(
                        &stack,
                        &transport,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_MGMT,
                        result.mgmt_reply.message_type,
                        result.mgmt_reply.payload,
                        result.mgmt_reply.payload_length);
                }
                else if ((result.flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u)
                {
                    if ((result.flags & LEAP_DEVICE_STACK_FLAG_DIR_PROFILE_SELECTED) != 0u)
                    {
                        printf("profile selected -> CONFIGURED\n");
                    }

                    device_send_reply(
                        &stack,
                        &transport,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_DIR,
                        result.dir_message_type,
                        result.dir_payload,
                        result.dir_payload_length);
                }
                else if ((result.flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u)
                {
                    device_send_reply(
                        &stack,
                        &transport,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_PD,
                        result.pd_reply_message_type,
                        result.pd_reply_payload,
                        result.pd_reply_payload_length);
                }
            }
            else if (status == LEAP_DEVICE_STACK_PD_REJECTED)
            {
                device_send_error_reply(
                    &stack,
                    &transport,
                    src_mac,
                    &result,
                    (uint16_t)LEAP_SERVICE_PD,
                    result.frame.header.message_type,
                    result.error_code);
                leap_linux_device_stats_on_reject(&g_stats);
                printf("PD rejected (status=0x%04X state=%u)\n",
                       result.error_code,
                       (unsigned)result.device_state);
            }
            else if (status == LEAP_DEVICE_STACK_DIR_ERROR)
            {
                device_send_error_reply(
                    &stack,
                    &transport,
                    src_mac,
                    &result,
                    (uint16_t)LEAP_SERVICE_DIR,
                    result.frame.header.message_type,
                    result.error_code);
                printf("DIR error (status=0x%04X state=%u)\n",
                       result.error_code,
                       (unsigned)result.device_state);
            }
            else if (status == LEAP_DEVICE_STACK_DIAG_ERROR)
            {
                device_send_error_reply(
                    &stack,
                    &transport,
                    src_mac,
                    &result,
                    (uint16_t)LEAP_SERVICE_DIAG,
                    result.frame.header.message_type,
                    result.error_code);
                printf("DIAG error (status=0x%04X state=%u)\n",
                       result.error_code,
                       (unsigned)result.device_state);
            }

            if (g_stats_enabled != 0 &&
                (g_stats.frames_rx % LEAP_DEVICE_STATS_INTERVAL) == 0u &&
                g_stats.frames_rx > 0u)
            {
                g_stats.tx_send_retries = leap_linux_send_retry_count();
                leap_linux_device_stats_log(&g_stats, &transport);
            }
        }

        tick_flags = 0u;
        (void)leap_device_stack_tick(&stack, now_us, &tick_flags);
        if (tick_flags != 0u)
        {
            device_log_tick(tick_flags, leap_mgmt_device_get_state(&stack.mgmt));
        }
    }
#endif
}
