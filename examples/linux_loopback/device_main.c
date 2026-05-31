/*
 * examples/linux_loopback/device_main.c
 *
 * Linux AF_PACKET LEAP device: DISC + MGMT with tick loop.
 *
 * Usage: sudo ./leap_linux_device [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"

#include "leap/leap_device_stack.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#define LEAP_RX_BUF_SIZE 1600u

static void device_send_reply(
    const LeapRawLinuxSocket*   transport,
    const uint8_t*              dst_mac,
    const LeapDeviceStackResult* result,
    uint16_t                    message_type,
    const uint8_t*              payload,
    size_t                        payload_length)
{
    if (leap_linux_send_leap(
            transport,
            dst_mac,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            result->frame.header.service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            payload,
            payload_length) != 0)
    {
        return;
    }

    printf("sent reply (service=0x%04X message=0x%04X)\n",
           result->frame.header.service_id,
           message_type);
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
        break;
    default:
        break;
    }
}

static void device_log_tick(uint32_t flags, LeapState_u16 state)
{
    if ((flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u)
    {
        printf("tick: lease/watchdog expired -> SAFE (state=%u)\n", (unsigned)state);
    }
    else if ((flags & LEAP_DEVICE_STACK_FLAG_OWNERSHIP_CHANGED) != 0u)
    {
        printf("tick: ownership changed (state=%u)\n", (unsigned)state);
    }
}

int main(int argc, char** argv)
{
    const char*           ifname = "lo";
    LeapRawLinuxSocket    transport;
    LeapDeviceStack       stack;
    LeapDeviceStackResult result;
    LeapMgmtDeviceConfig  mgmt_config;
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
    if (argc > 1)
    {
        ifname = argv[1];
    }

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    mgmt_config.default_lease_us    = 5000000u;
    mgmt_config.default_watchdog_us = 500000u;
    mgmt_config.max_lease_us        = 10000000u;
    mgmt_config.max_watchdog_us     = 1000000u;

    leap_device_stack_init(&stack, &mgmt_config);

    if (leap_raw_linux_open(&transport, ifname, LEAP_ETHERTYPE_DEVELOPMENT) != 0)
    {
        leap_linux_print_transport_error("open");
        fprintf(stderr, "need CAP_NET_RAW/root on interface %s\n", ifname);
        return 1;
    }

    memcpy(stack.disc.config.identity.primary_mac, transport.local_mac, 6);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);

    printf("LEAP device on %s\n", ifname);
    leap_linux_print_mac("  local MAC: ", transport.local_mac);
    printf("  state: CONFIGURED — waiting for frames\n");

    for (;;)
    {
        now_us = leap_raw_linux_monotonic_us();

        if (leap_linux_recv_leap(
                &transport,
                src_mac,
                rx,
                sizeof(rx),
                &rx_len,
                100) == 0)
        {
            if (leap_device_stack_process_frame(
                    &stack,
                    src_mac,
                    now_us,
                    rx,
                    rx_len,
                    &result) == LEAP_DEVICE_STACK_OK)
            {
                device_log_rx(&result);

                if ((result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u)
                {
                    device_send_reply(
                        &transport,
                        src_mac,
                        &result,
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
                        &transport,
                        src_mac,
                        &result,
                        result.mgmt_reply.message_type,
                        result.mgmt_reply.payload,
                        result.mgmt_reply.payload_length);
                }
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
