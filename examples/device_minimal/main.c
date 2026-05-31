/*
 * examples/device_minimal/main.c
 *
 * Simulated device bring-up: MGMT session, OP transition, LEAP-PD write.
 * Uses in-memory frame injection (no raw Ethernet required).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_crc.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_protocol.h"

#include <stdio.h>
#include <string.h>

#define FRAME_BUF_SIZE 256u

static const uint8_t k_controller_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_spoof_mac[6]      = { 0x02, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

static void finalize_header_crc(uint8_t* frame)
{
    LeapHeader* header = (LeapHeader*)frame;
    uint8_t     temp[LEAP_HEADER_LENGTH_V1];

    memcpy(temp, frame, LEAP_HEADER_LENGTH_V1);
    temp[26] = 0u;
    temp[27] = 0u;
    header->header_crc16 = leap_crc16_xmodem(temp, LEAP_HEADER_LENGTH_V1);
}

static int build_frame(
    uint8_t*       out,
    size_t         out_capacity,
    size_t*        out_length,
    uint16_t       service_id,
    uint16_t       message_type,
    uint32_t       session_id,
    const uint8_t* payload,
    size_t         payload_length)
{
    LeapHeader* header;
    size_t      total;

    if (out_capacity < (size_t)LEAP_HEADER_LENGTH_V1 + payload_length)
    {
        return -1;
    }

    total = (size_t)LEAP_HEADER_LENGTH_V1 + payload_length;
    memset(out, 0, total);

    header = (LeapHeader*)out;
    header->magic          = LEAP_MAGIC_U32;
    header->version_major  = LEAP_VERSION_MAJOR;
    header->version_minor  = LEAP_VERSION_MINOR;
    header->header_length  = LEAP_HEADER_LENGTH_V1;
    header->service_id     = service_id;
    header->message_type   = message_type;
    header->session_id     = session_id;
    header->payload_length = (uint16_t)payload_length;

    if (payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        header->payload_crc32c = leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length);
    }

    finalize_header_crc(out);
    *out_length = total;
    return 0;
}

static void print_state(const char* label, LeapState_u16 state)
{
    printf("%s: %u\n", label, (unsigned)state);
}

int main(void)
{
    LeapDeviceStack        stack;
    LeapDeviceStackResult  result;
    LeapMgmtDeviceConfig   config;
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    LeapEndpointDataHeader endpoint;
    uint8_t                frame[FRAME_BUF_SIZE];
    size_t                 frame_length = 0u;
    uint32_t               session_id   = 0u;
    uint32_t               tick_flags   = 0u;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;

    leap_device_stack_init(&stack, &config);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);
    print_state("configured", leap_mgmt_device_get_state(&stack.mgmt));

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_controller_mac, 6);
    open_req.open_flags              = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us = 1000000u;
    build_frame(
        frame,
        FRAME_BUF_SIZE,
        &frame_length,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_OPEN_SESSION,
        0u,
        (const uint8_t*)&open_req,
        sizeof(open_req));
    leap_device_stack_process_frame(
        &stack, k_controller_mac, 0u, frame, frame_length, &result);
    session_id = ((const LeapOpenSessionReply*)result.mgmt_reply.payload)->assigned_session_id;
    print_state("after open session", result.device_state);
    printf("assigned session_id: 0x%08X\n", session_id);

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;
    build_frame(
        frame,
        FRAME_BUF_SIZE,
        &frame_length,
        (uint16_t)LEAP_SERVICE_MGMT,
        LEAP_MGMT_SET_STATE,
        session_id,
        (const uint8_t*)&set_req,
        sizeof(set_req));
    leap_device_stack_process_frame(
        &stack, k_controller_mac, 0u, frame, frame_length, &result);
    print_state("after set OP", result.device_state);

    memset(&endpoint, 0, sizeof(endpoint));
    endpoint.endpoint_flags = LEAP_PD_FLAG_APPLY_OUTPUTS;
    build_frame(
        frame,
        FRAME_BUF_SIZE,
        &frame_length,
        (uint16_t)LEAP_SERVICE_PD,
        LEAP_PD_WRITE_ENDPOINT,
        session_id,
        (const uint8_t*)&endpoint,
        sizeof(endpoint));
    leap_device_stack_process_frame(
        &stack, k_controller_mac, 0u, frame, frame_length, &result);
    printf("pd outputs applied: %s\n",
           (result.flags & LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) != 0u ? "yes" : "no");

    build_frame(
        frame,
        FRAME_BUF_SIZE,
        &frame_length,
        (uint16_t)LEAP_SERVICE_PD,
        LEAP_PD_WRITE_ENDPOINT,
        session_id,
        (const uint8_t*)&endpoint,
        sizeof(endpoint));
    leap_device_stack_process_frame(
        &stack, k_spoof_mac, 0u, frame, frame_length, &result);
    print_state("after spoof pd", leap_mgmt_device_get_state(&stack.mgmt));

    leap_device_stack_tick(&stack, 5000000u, &tick_flags);
    print_state("after lease tick", leap_mgmt_device_get_state(&stack.mgmt));

    return 0;
}
