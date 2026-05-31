/*
 * examples/linux_loopback/controller_main.c
 *
 * Linux AF_PACKET LEAP controller:
 *   HELLO -> SELECT_PROFILE -> OPEN_SESSION -> SET_STATE(OP) -> PD
 *
 * Usage:
 *   sudo ./leap_linux_controller [interface]
 *   sudo ./leap_linux_controller --cyclic [interface]
 *   sudo ./leap_linux_controller --cyclic-ms 100 [interface]
 *   sudo ./leap_linux_controller --lease-demo [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"

#include "leap_linux_pd.h"

#include "leap/leap_dir_controller.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_controller.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_protocol.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LEAP_RX_BUF_SIZE 1600u
#define LEAP_LEASE_DEMO_US 2000000u
#define LEAP_LEASE_DEMO_IDLE_S 3u

static const uint8_t k_bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static volatile sig_atomic_t g_controller_stop = 0;

static void controller_on_sigint(int signo)
{
    (void)signo;
    g_controller_stop = 1;
}

static int controller_wait_reply(
    const LeapRawLinuxSocket* sock,
    uint8_t*                  peer_mac,
    uint16_t                  expect_service,
    uint16_t                  expect_message,
    LeapFrameView*            view,
    uint8_t*                  rx,
    size_t                    rx_capacity)
{
    size_t rx_len = 0u;

    if (leap_linux_recv_leap(sock, peer_mac, rx, rx_capacity, &rx_len, 5000) != 0)
    {
        fprintf(stderr, "timeout waiting for reply (service=0x%04X message=0x%04X)\n",
                expect_service,
                expect_message);
        return -1;
    }

    if (leap_frame_parse(rx, rx_len, view) != LEAP_FRAME_OK)
    {
        fprintf(stderr, "invalid LEAP reply frame\n");
        return -1;
    }

    if (view->header.service_id != expect_service ||
        view->header.message_type != expect_message)
    {
        fprintf(stderr,
                "unexpected reply (got service=0x%04X message=0x%04X)\n",
                view->header.service_id,
                view->header.message_type);
        return -1;
    }

    return 0;
}

static int controller_bootstrap(
    LeapRawLinuxSocket*          transport,
    LeapMgmtControllerContext*   mgmt,
    uint32_t                     lease_us,
    uint8_t*                     peer_mac)
{
    LeapHelloRequest              hello;
    LeapMgmtControllerEvent       event;
    LeapDirControllerProfileInfo  profile_info;
    uint8_t                       payload[128];
    size_t                        payload_length;
    uint8_t                       rx[LEAP_RX_BUF_SIZE];
    LeapFrameView                 view;

    memset(&hello, 0, sizeof(hello));
    if (leap_linux_send_leap(
            transport,
            k_bcast,
            LEAP_FLAG_BROADCAST,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            leap_mgmt_controller_next_sequence(mgmt),
            0u,
            (const uint8_t*)&hello,
            sizeof(hello)) != 0)
    {
        return -1;
    }

    printf("sent HELLO (broadcast)\n");

    if (controller_wait_reply(
            transport,
            peer_mac,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO_REPLY,
            &view,
            rx,
            sizeof(rx)) != 0)
    {
        return -1;
    }

    if (view.payload_length < sizeof(LeapHelloReply))
    {
        fprintf(stderr, "HELLO_REPLY too short\n");
        return -1;
    }

    if (leap_mgmt_controller_on_hello_reply(
            mgmt,
            peer_mac,
            view.payload,
            view.payload_length,
            &event) != LEAP_MGMT_CTRL_OK)
    {
        return -1;
    }

    {
        const LeapHelloReply* hello_reply = (const LeapHelloReply*)view.payload;
        printf("received HELLO_REPLY\n");
        leap_linux_print_mac("  device MAC: ", peer_mac);
        printf("  device state: %u\n", (unsigned)hello_reply->current_state);
        printf("  profile: 0x%08X\n", hello_reply->active_profile_id);
    }

    payload_length = leap_dir_controller_build_select_profile(
        payload,
        sizeof(payload),
        mgmt->default_profile_id != 0u ? mgmt->default_profile_id
                                         : LEAP_PROFILE_DIGITAL_IO_16X16,
        0u);
    if (payload_length == 0u)
    {
        return -1;
    }

    if (leap_linux_send_leap(
            transport,
            peer_mac,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_SELECT_PROFILE,
            0u,
            leap_mgmt_controller_next_sequence(mgmt),
            0u,
            payload,
            payload_length) != 0)
    {
        return -1;
    }

    printf("sent SELECT_PROFILE (0x%08X)\n",
           mgmt->default_profile_id != 0u ? mgmt->default_profile_id
                                          : LEAP_PROFILE_DIGITAL_IO_16X16);

    if (controller_wait_reply(
            transport,
            peer_mac,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_PROFILE_REPLY,
            &view,
            rx,
            sizeof(rx)) != 0)
    {
        return -1;
    }

    if (leap_dir_controller_on_profile_reply(
            view.payload, view.payload_length, &profile_info) != LEAP_DIR_CTRL_OK)
    {
        return -1;
    }

    printf("received PROFILE_REPLY\n");
    printf("  active profile: 0x%08X\n", profile_info.active_profile_id);
    printf("  endpoints: %u\n", (unsigned)profile_info.endpoint_count);

    payload_length = leap_mgmt_controller_build_open_session(
        mgmt, payload, sizeof(payload), lease_us, 0u);
    if (payload_length == 0u)
    {
        return -1;
    }

    if (leap_linux_send_leap(
            transport,
            peer_mac,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            leap_mgmt_controller_next_sequence(mgmt),
            0u,
            payload,
            payload_length) != 0)
    {
        return -1;
    }

    printf("sent OPEN_SESSION (owner request, lease=%u us)\n", lease_us);

    if (controller_wait_reply(
            transport,
            peer_mac,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_OPEN_SESSION_REPLY,
            &view,
            rx,
            sizeof(rx)) != 0)
    {
        return -1;
    }

    if (leap_mgmt_controller_on_mgmt_reply(mgmt, &view, &event) != LEAP_MGMT_CTRL_OK)
    {
        return -1;
    }

    {
        const LeapOpenSessionReply* open_reply =
            (const LeapOpenSessionReply*)view.payload;
        printf("received OPEN_SESSION_REPLY\n");
        printf("  session_id: 0x%08X\n", open_reply->assigned_session_id);
        printf("  granted lease: %u us\n", open_reply->granted_lease_time_us);
        printf("  device state: %u (expected SAFE=3)\n",
               (unsigned)open_reply->current_state);
    }

    payload_length = leap_mgmt_controller_build_set_state(
        mgmt, payload, sizeof(payload), LEAP_STATE_OP);
    if (payload_length == 0u)
    {
        return -1;
    }

    if (leap_linux_send_leap(
            transport,
            peer_mac,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_SET_STATE,
            leap_mgmt_controller_session_id(mgmt),
            leap_mgmt_controller_next_sequence(mgmt),
            0u,
            payload,
            payload_length) != 0)
    {
        return -1;
    }

    printf("sent SET_STATE -> OP\n");

    if (controller_wait_reply(
            transport,
            peer_mac,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_STATE_REPLY,
            &view,
            rx,
            sizeof(rx)) != 0)
    {
        return -1;
    }

    if (leap_mgmt_controller_on_mgmt_reply(mgmt, &view, &event) != LEAP_MGMT_CTRL_OK)
    {
        return -1;
    }

    {
        const LeapStateReply* state_reply = (const LeapStateReply*)view.payload;
        printf("received STATE_REPLY\n");
        printf("  accepted state: %u\n", (unsigned)state_reply->accepted_state);
        printf("  current state: %u (expected OP=4)\n",
               (unsigned)state_reply->current_state);
    }

    return 0;
}

int main(int argc, char** argv)
{
    LeapLinuxControllerOptions  options;
    LeapRawLinuxSocket          transport;
    LeapRawLinuxOpenOptions     open_options;
    LeapMgmtControllerContext   mgmt;
    LeapMgmtControllerConfig    mgmt_config;
    LeapPdControllerContext     pd;
    LeapPdControllerConfig      pd_config;
    LeapPdControllerIo          pd_io;
    LeapLinuxPdTransport        pd_transport;
    uint8_t                     peer_mac[6];
    uint32_t                    lease_us = 5000000u;

#if !defined(__linux__)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_linux_controller requires Linux AF_PACKET support.\n");
    return 1;
#else
    leap_linux_controller_parse_args(argc, argv, &options);
    if (options.lease_demo != 0)
    {
        lease_us = LEAP_LEASE_DEMO_US;
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous     = options.promiscuous;
    open_options.filter_dest_mac = 1;

    if (leap_raw_linux_open_ex(
            &transport,
            options.ifname,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        leap_linux_print_transport_error("open");
        return 1;
    }

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    memcpy(mgmt_config.controller_mac, transport.local_mac, 6);
    leap_mgmt_controller_init(&mgmt, &mgmt_config);

    memset(&pd_config, 0, sizeof(pd_config));
    pd_config.cycle_period_ms          = options.cyclic_period_ms;
    pd_config.use_exchange             = options.exchange;
    pd_config.stats_log_interval       = options.stats ? options.stats_interval : 0u;
    pd_config.heartbeat_every_n_cycles = 10u;
    leap_pd_controller_init(&pd, &pd_config);

    pd_transport.sock  = &transport;
    pd_transport.mgmt  = &mgmt;
    leap_linux_pd_init_io(&pd_io, &pd_transport);

    printf("LEAP controller on %s", options.ifname);
    if (options.lease_demo != 0)
    {
        printf(" (lease-demo)");
    }
    else if (options.cyclic != 0)
    {
        printf(" (cyclic PD %u ms", options.cyclic_period_ms);
        if (options.exchange != 0)
        {
            printf(", exchange");
        }
        printf(")");
    }
    if (options.promiscuous != 0)
    {
        printf(" [promisc]");
    }
    printf("\n");
    leap_linux_print_mac("  local MAC: ", transport.local_mac);

    if (controller_bootstrap(&transport, &mgmt, lease_us, peer_mac) != 0)
    {
        leap_raw_linux_close(&transport);
        return 1;
    }

    if (options.lease_demo != 0)
    {
        printf("lease-demo: idling %u s without heartbeat or PD (watch device log)...\n",
               LEAP_LEASE_DEMO_IDLE_S);
        sleep(LEAP_LEASE_DEMO_IDLE_S);
        printf("lease-demo complete — device should show safe outputs active\n");
        leap_raw_linux_close(&transport);
        return 0;
    }

    if (options.cyclic != 0)
    {
        signal(SIGINT, controller_on_sigint);
        if (leap_pd_controller_run_cyclic(
                &pd,
                &mgmt,
                &pd_io,
                peer_mac,
                (volatile int*)&g_controller_stop) != LEAP_PD_CTRL_OK)
        {
            leap_raw_linux_close(&transport);
            return 1;
        }
        leap_raw_linux_close(&transport);
        return 0;
    }

    if (leap_pd_controller_send_single_write(&pd, &mgmt, &pd_io, peer_mac, 0x0015u) !=
        LEAP_PD_CTRL_OK)
    {
        leap_raw_linux_close(&transport);
        return 1;
    }

    if (options.stats != 0)
    {
        leap_pd_controller_log_stats(&pd);
        leap_linux_print_transport_stats(&transport);
        printf("send retries (app): %llu\n",
               (unsigned long long)leap_linux_send_retry_count());
    }

    printf("sent PD WRITE_ENDPOINT (outputs=0x0015, seq=1000)\n");
    printf("check device log for 'I/O shadow: outputs=0x0015'\n");
    printf("flow complete — DISC + DIR + MGMT + PD on raw Ethernet\n");

    leap_raw_linux_close(&transport);
    return 0;
#endif
}
