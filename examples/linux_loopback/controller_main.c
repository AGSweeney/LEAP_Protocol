/*
 * examples/linux_loopback/controller_main.c
 *
 * Linux AF_PACKET LEAP controller:
 *   HELLO -> OPEN_SESSION -> SET_STATE(OP) -> PD (+ optional cyclic mode)
 *
 * Usage:
 *   sudo ./leap_linux_controller [interface]
 *   sudo ./leap_linux_controller --cyclic [interface]
 *   sudo ./leap_linux_controller --lease-demo [interface]
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_common.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LEAP_RX_BUF_SIZE 1600u
#define LEAP_LEASE_DEMO_US 2000000u
#define LEAP_LEASE_DEMO_IDLE_S 3u
#define LEAP_CYCLIC_PERIOD_US 100000u
#define LEAP_HEARTBEAT_EVERY_N_CYCLES 10u

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

static size_t controller_build_pd_write(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t process_sequence,
    uint16_t digital_outputs)
{
    LeapEndpointDataHeader  hdr;
    LeapProfileDigital16x16 profile;
    size_t                  total;

    total = sizeof(hdr) + sizeof(profile);
    if (out_capacity < total)
    {
        return 0u;
    }

    memset(&hdr, 0, sizeof(hdr));
    hdr.endpoint_id      = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    hdr.data_length      = (uint16_t)sizeof(profile);
    hdr.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;
    hdr.process_sequence = process_sequence;
    hdr.profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;

    memset(&profile, 0, sizeof(profile));
    profile.digital_outputs = digital_outputs;

    memcpy(out, &hdr, sizeof(hdr));
    memcpy(out + sizeof(hdr), &profile, sizeof(profile));
    return total;
}

static int controller_send_heartbeat(
    const LeapRawLinuxSocket* sock,
    const uint8_t*            peer_mac,
    uint32_t                  session_id,
    uint32_t*                 sequence)
{
    LeapHeartbeatPayload hb;

    memset(&hb, 0, sizeof(hb));
    if (leap_linux_send_leap_retry(
            sock,
            peer_mac,
            0u,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_HEARTBEAT,
            session_id,
            (*sequence)++,
            0u,
            (const uint8_t*)&hb,
            sizeof(hb),
            3) != 0)
    {
        return -1;
    }

    printf("sent HEARTBEAT (lease refresh)\n");
    return 0;
}

static int controller_bootstrap_mgmt(
    LeapRawLinuxSocket* transport,
    const char*         ifname,
    uint32_t            lease_us,
    uint8_t*            peer_mac,
    uint32_t*           session_id,
    uint32_t*           sequence)
{
    LeapHelloRequest      hello;
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest   set_req;
    uint8_t               rx[LEAP_RX_BUF_SIZE];
    LeapFrameView         view;
    const LeapHelloReply* hello_reply;
    const LeapOpenSessionReply* open_reply;
    const LeapStateReply* state_reply;

    (void)ifname;

    memset(&hello, 0, sizeof(hello));
    if (leap_linux_send_leap(
            transport,
            k_bcast,
            LEAP_FLAG_BROADCAST,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            (*sequence)++,
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

    hello_reply = (const LeapHelloReply*)view.payload;
    printf("received HELLO_REPLY\n");
    leap_linux_print_mac("  device MAC: ", peer_mac);
    printf("  device state: %u\n", (unsigned)hello_reply->current_state);
    printf("  profile: 0x%08X\n", hello_reply->active_profile_id);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, transport->local_mac, 6);
    open_req.open_flags                 = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us    = lease_us;
    open_req.requested_watchdog_time_us = 500000u;

    if (leap_linux_send_leap(
            transport,
            peer_mac,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            (*sequence)++,
            0u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) != 0)
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

    if (view.payload_length < sizeof(LeapOpenSessionReply))
    {
        fprintf(stderr, "OPEN_SESSION_REPLY too short\n");
        return -1;
    }

    open_reply   = (const LeapOpenSessionReply*)view.payload;
    *session_id  = open_reply->assigned_session_id;
    printf("received OPEN_SESSION_REPLY\n");
    printf("  session_id: 0x%08X\n", *session_id);
    printf("  granted lease: %u us\n", open_reply->granted_lease_time_us);
    printf("  device state: %u (expected SAFE=3)\n", (unsigned)open_reply->current_state);

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;

    if (leap_linux_send_leap(
            transport,
            peer_mac,
            LEAP_FLAG_ACK_REQUESTED,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_SET_STATE,
            *session_id,
            (*sequence)++,
            0u,
            (const uint8_t*)&set_req,
            sizeof(set_req)) != 0)
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

    if (view.payload_length < sizeof(LeapStateReply))
    {
        fprintf(stderr, "STATE_REPLY too short\n");
        return -1;
    }

    state_reply = (const LeapStateReply*)view.payload;
    printf("received STATE_REPLY\n");
    printf("  accepted state: %u\n", (unsigned)state_reply->accepted_state);
    printf("  current state: %u (expected OP=4)\n", (unsigned)state_reply->current_state);
    return 0;
}

static int controller_run_cyclic_pd(
    const LeapRawLinuxSocket* transport,
    const uint8_t*            peer_mac,
    uint32_t                  session_id,
    uint32_t*                 sequence)
{
    uint8_t  pd_payload[sizeof(LeapEndpointDataHeader) + sizeof(LeapProfileDigital16x16)];
    size_t   pd_payload_length;
    uint32_t pd_seq = 1000u;
    uint32_t cycle  = 0u;
    uint16_t outputs;

    printf("cyclic PD mode (~100 ms) — Ctrl+C to stop\n");

    while (g_controller_stop == 0)
    {
        outputs = (uint16_t)(0x0001u << (cycle % 5u));
        pd_payload_length = controller_build_pd_write(
            pd_payload,
            sizeof(pd_payload),
            pd_seq++,
            outputs);

        if (pd_payload_length == 0u)
        {
            return -1;
        }

        if (leap_linux_send_leap_retry(
                transport,
                peer_mac,
                0u,
                (uint16_t)LEAP_SERVICE_PD,
                LEAP_PD_WRITE_ENDPOINT,
                session_id,
                (*sequence)++,
                0u,
                pd_payload,
                pd_payload_length,
                3) != 0)
        {
            fprintf(stderr, "cyclic PD send failed at cycle %u\n", cycle);
            return -1;
        }

        if ((cycle % 20u) == 0u)
        {
            printf("cyclic PD seq=%u outputs=0x%04X\n", pd_seq - 1u, outputs);
        }

        cycle++;
        if ((cycle % LEAP_HEARTBEAT_EVERY_N_CYCLES) == 0u)
        {
            (void)controller_send_heartbeat(
                transport,
                peer_mac,
                session_id,
                sequence);
        }

        usleep(LEAP_CYCLIC_PERIOD_US);
    }

    printf("cyclic PD stopped\n");
    return 0;
}

int main(int argc, char** argv)
{
    LeapLinuxControllerOptions options;
    LeapRawLinuxSocket         transport;
    uint8_t                    peer_mac[6];
    uint8_t                    pd_payload[sizeof(LeapEndpointDataHeader) + sizeof(LeapProfileDigital16x16)];
    size_t                     pd_payload_length;
    uint32_t                   session_id = 0u;
    uint32_t                   sequence   = 1u;
    uint32_t                   lease_us   = 5000000u;

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

    if (leap_raw_linux_open(&transport, options.ifname, LEAP_ETHERTYPE_DEVELOPMENT) != 0)
    {
        leap_linux_print_transport_error("open");
        return 1;
    }

    printf("LEAP controller on %s", options.ifname);
    if (options.lease_demo != 0)
    {
        printf(" (lease-demo)");
    }
    else if (options.cyclic != 0)
    {
        printf(" (cyclic PD)");
    }
    printf("\n");
    leap_linux_print_mac("  local MAC: ", transport.local_mac);

    if (controller_bootstrap_mgmt(
            &transport,
            options.ifname,
            lease_us,
            peer_mac,
            &session_id,
            &sequence) != 0)
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
        if (controller_run_cyclic_pd(&transport, peer_mac, session_id, &sequence) != 0)
        {
            leap_raw_linux_close(&transport);
            return 1;
        }
        leap_raw_linux_close(&transport);
        return 0;
    }

    pd_payload_length = controller_build_pd_write(
        pd_payload,
        sizeof(pd_payload),
        1001u,
        0x0015u);

    if (pd_payload_length == 0u ||
        leap_linux_send_leap_retry(
            &transport,
            peer_mac,
            0u,
            (uint16_t)LEAP_SERVICE_PD,
            LEAP_PD_WRITE_ENDPOINT,
            session_id,
            sequence++,
            0u,
            pd_payload,
            pd_payload_length,
            3) != 0)
    {
        leap_raw_linux_close(&transport);
        return 1;
    }

    printf("sent PD WRITE_ENDPOINT (outputs=0x0015, seq=1001)\n");
    printf("check device log for 'I/O shadow: outputs=0x0015'\n");
    printf("flow complete — DISC + MGMT + PD on raw Ethernet\n");

    leap_raw_linux_close(&transport);
    return 0;
#endif
}
