/*
 * test_negative_frames.c
 *
 * Negative-path tests: stale frames and duplicate process sequences.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "test_util.h"

#include "leap/leap_crc.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_diag_device.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_pd_common.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define NEG_FRAME_BUF_SIZE 256u

static const uint8_t k_mac_a[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_peer_mac[6] = { 0x02, 0x66, 0x77, 0x88, 0x99, 0xAA };

static void neg_finalize_header_crc(uint8_t* frame)
{
    LeapHeader* header = (LeapHeader*)frame;
    uint8_t     temp[LEAP_HEADER_LENGTH_V1];

    memcpy(temp, frame, LEAP_HEADER_LENGTH_V1);
    temp[26] = 0u;
    temp[27] = 0u;
    header->header_crc16 = leap_crc16_xmodem(temp, LEAP_HEADER_LENGTH_V1);
}

static int neg_build_frame(
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
    size_t      total_length;

    if (out == NULL || out_length == NULL ||
        out_capacity < (size_t)LEAP_HEADER_LENGTH_V1 + payload_length)
    {
        return -1;
    }

    total_length = (size_t)LEAP_HEADER_LENGTH_V1 + payload_length;
    memset(out, 0, total_length);

    header = (LeapHeader*)out;
    header->magic          = LEAP_MAGIC_U32;
    header->version_major  = LEAP_VERSION_MAJOR;
    header->version_minor  = LEAP_VERSION_MINOR;
    header->header_length  = LEAP_HEADER_LENGTH_V1;
    header->service_id     = service_id;
    header->message_type   = message_type;
    header->session_id     = session_id;
    header->payload_length = (uint16_t)payload_length;

    if (payload != NULL && payload_length > 0u)
    {
        memcpy(out + LEAP_HEADER_LENGTH_V1, payload, payload_length);
        header->payload_crc32c = leap_crc32c(out + LEAP_HEADER_LENGTH_V1, payload_length);
    }

    neg_finalize_header_crc(out);
    *out_length = total_length;
    return 0;
}

static uint32_t neg_stack_bootstrap_op(
    LeapDeviceStack* stack,
    uint16_t*        outputs_io)
{
    LeapDeviceStackResult  result;
    LeapOpenSessionRequest open_req;
    LeapSetStateRequest    set_req;
    LeapPdBuildParams      pd_params;
    uint8_t                pd_payload[128];
    size_t                 pd_payload_length;
    uint8_t                frame[NEG_FRAME_BUF_SIZE];
    size_t                 frame_length = 0u;
    uint32_t               session_id   = 0u;
    LeapPdDeviceIoBinding  pd_io;

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.digital_outputs = outputs_io;
    leap_device_stack_bind_pd_io(stack, &pd_io);

    memset(&open_req, 0, sizeof(open_req));
    memcpy(open_req.controller_mac, k_mac_a, 6);
    open_req.open_flags              = LEAP_OPEN_FLAG_REQUEST_OWNER;
    open_req.requested_lease_time_us = 1000000u;
    ASSERT_TRUE_RET(
        neg_build_frame(
            frame,
            NEG_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_OPEN_SESSION,
            0u,
            (const uint8_t*)&open_req,
            sizeof(open_req)) == 0,
        0u);
    ASSERT_EQ_INT_RET(
        leap_device_stack_process_frame(stack, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK,
        0u);
    session_id = ((const LeapOpenSessionReply*)result.mgmt_reply.payload)->assigned_session_id;

    memset(&set_req, 0, sizeof(set_req));
    set_req.requested_state = (uint16_t)LEAP_STATE_OP;
    ASSERT_TRUE_RET(
        neg_build_frame(
            frame,
            NEG_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_MGMT,
            LEAP_MGMT_SET_STATE,
            session_id,
            (const uint8_t*)&set_req,
            sizeof(set_req)) == 0,
        0u);
    ASSERT_EQ_INT_RET(
        leap_device_stack_process_frame(stack, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK,
        0u);

    memset(&pd_params, 0, sizeof(pd_params));
    pd_params.profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    pd_params.process_sequence = 100u;
    pd_params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;
    pd_payload_length = leap_pd_build_digital_write(
        pd_payload,
        sizeof(pd_payload),
        &pd_params,
        0x0001u);
    ASSERT_TRUE_RET(pd_payload_length > 0u, 0u);
    ASSERT_TRUE_RET(
        neg_build_frame(
            frame,
            NEG_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_PD,
            LEAP_PD_WRITE_ENDPOINT,
            session_id,
            pd_payload,
            pd_payload_length) == 0,
        0u);
    ASSERT_EQ_INT_RET(
        leap_device_stack_process_frame(stack, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_DEVICE_STACK_OK,
        0u);
    ASSERT_EQ_U16_RET(result.pd_outputs_applied, 0x0001u, 0u);
    *outputs_io = 0x0001u;

    return session_id;
}

TEST(test_stack_rejects_duplicate_pd_without_output_change)
{
    LeapDeviceStack        stack;
    LeapDeviceStackResult  result;
    LeapMgmtDeviceConfig   config;
    LeapPdBuildParams      pd_params;
    uint8_t                pd_payload[128];
    size_t                 pd_payload_length;
    uint8_t                frame[NEG_FRAME_BUF_SIZE];
    size_t                 frame_length = 0u;
    uint16_t               outputs      = 0u;
    uint32_t               session_id;

    memset(&config, 0, sizeof(config));
    config.default_lease_us    = 1000000u;
    config.default_watchdog_us = 200000u;
    leap_device_stack_init(&stack, &config);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);
    leap_mgmt_device_on_profile_selected(&stack.mgmt);

    session_id = neg_stack_bootstrap_op(&stack, &outputs);
    ASSERT_EQ_U16(outputs, 0x0001u);

    memset(&pd_params, 0, sizeof(pd_params));
    pd_params.profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    pd_params.process_sequence = 100u;
    pd_params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;
    pd_payload_length = leap_pd_build_digital_write(
        pd_payload,
        sizeof(pd_payload),
        &pd_params,
        0x00FFu);
    ASSERT_TRUE(pd_payload_length > 0u);
    ASSERT_TRUE(
        neg_build_frame(
            frame,
            NEG_FRAME_BUF_SIZE,
            &frame_length,
            (uint16_t)LEAP_SERVICE_PD,
            LEAP_PD_WRITE_ENDPOINT,
            session_id,
            pd_payload,
            pd_payload_length) == 0);
    ASSERT_EQ_INT(
        leap_device_stack_process_frame(&stack, k_mac_a, 0u, frame, frame_length, &result),
        LEAP_DEVICE_STACK_PD_REJECTED);
    ASSERT_EQ_U16(result.error_code, LEAP_STATUS_DUPLICATE_SEQUENCE);
    ASSERT_EQ_U16(outputs, 0x0001u);
    ASSERT_TRUE(stack.diag.duplicate_sequences == 1u);
}

TEST(test_diag_counts_duplicate_process_sequence)
{
    LeapDiagDeviceContext diag;
    LeapPdDeviceResult    pd_result;

    leap_diag_device_init(&diag, NULL);
    leap_diag_device_on_transport_ready(&diag, 1000u);

    memset(&pd_result, 0, sizeof(pd_result));
    pd_result.status     = LEAP_PD_DEVICE_REJECTED;
    pd_result.error_code = LEAP_STATUS_DUPLICATE_SEQUENCE;

    leap_diag_device_on_pd_result(&diag, &pd_result, 2000u);
    ASSERT_TRUE(diag.duplicate_sequences == 1u);
    ASSERT_TRUE(diag.stale_process_frames == 1u);
    ASSERT_TRUE(diag.process_cycles_missed == 1u);
}

typedef struct NegPdCtrlMockIo
{
    uint64_t now_us;
    uint8_t  exchange_reply[NEG_FRAME_BUF_SIZE];
    size_t   exchange_reply_length;
} NegPdCtrlMockIo;

static int neg_mock_send_pd(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint16_t       message_type,
    const uint8_t* payload,
    size_t         payload_length,
    uint32_t       session_id,
    uint32_t       sequence)
{
    NegPdCtrlMockIo* mock = (NegPdCtrlMockIo*)user_ctx;

    (void)peer_mac;
    (void)message_type;
    (void)payload;
    (void)payload_length;
    (void)session_id;
    (void)sequence;

    if (mock == NULL)
    {
        return -1;
    }

    mock->now_us += 3000u;
    return 0;
}

static int neg_mock_wait_exchange_reply(
    void*          user_ctx,
    const uint8_t* peer_mac,
    uint8_t*       reply_payload,
    size_t         reply_capacity,
    size_t*        reply_length,
    int            timeout_ms,
    uint64_t*      reply_recv_us_out)
{
    NegPdCtrlMockIo* mock = (NegPdCtrlMockIo*)user_ctx;

    (void)peer_mac;
    (void)timeout_ms;

    if (mock == NULL || reply_payload == NULL || reply_length == NULL)
    {
        return -1;
    }

    if (mock->exchange_reply_length == 0u ||
        mock->exchange_reply_length > reply_capacity)
    {
        return -1;
    }

    memcpy(reply_payload, mock->exchange_reply, mock->exchange_reply_length);
    *reply_length = mock->exchange_reply_length;
    if (reply_recv_us_out != NULL)
    {
        *reply_recv_us_out = mock->now_us;
    }

    return 0;
}

static uint64_t neg_mock_monotonic_us(void* user_ctx)
{
    NegPdCtrlMockIo* mock = (NegPdCtrlMockIo*)user_ctx;

    if (mock == NULL)
    {
        return 0u;
    }

    return mock->now_us;
}

static void neg_pd_ctrl_setup_session(
    LeapMgmtControllerContext* mgmt,
    LeapPdControllerContext*     pd)
{
    LeapMgmtControllerEvent event;
    LeapOpenSessionReply    open_reply;
    LeapStateReply          state_reply;
    LeapPdControllerConfig  pd_config;

    memset(&pd_config, 0, sizeof(pd_config));
    pd_config.cycle_period_ms           = 10u;
    pd_config.use_exchange              = 1;
    pd_config.enforce_reply_frame_age     = 1;
    pd_config.validate_exchange_reply     = 1;
    pd_config.heartbeat_every_n_cycles    = 100u;
    leap_pd_controller_init(pd, &pd_config);

    memset(&open_reply, 0, sizeof(open_reply));
    open_reply.assigned_session_id      = 7u;
    open_reply.granted_lease_time_us    = 1000000u;
    open_reply.granted_watchdog_time_us = 200000u;
    open_reply.current_state            = (uint16_t)LEAP_STATE_SAFE;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_open_session_reply(
            mgmt,
            (const uint8_t*)&open_reply,
            sizeof(open_reply),
            &event),
        LEAP_MGMT_CTRL_OK);

    memset(&state_reply, 0, sizeof(state_reply));
    state_reply.accepted_state = (uint16_t)LEAP_STATE_OP;
    state_reply.current_state  = (uint16_t)LEAP_STATE_OP;

    ASSERT_EQ_INT(
        leap_mgmt_controller_on_state_reply(
            mgmt,
            (const uint8_t*)&state_reply,
            sizeof(state_reply),
            &event),
        LEAP_MGMT_CTRL_OK);
}

TEST(test_pd_controller_rejects_stale_exchange_reply)
{
    LeapMgmtControllerContext mgmt;
    LeapMgmtControllerConfig  mgmt_config;
    LeapPdControllerContext   pd;
    LeapPdControllerIo        io;
    NegPdCtrlMockIo           mock;
    LeapPdProfileMap          profile;
    LeapPdExchangeView        request_view;
    LeapExchangeStatus        status;
    uint8_t                   request[NEG_FRAME_BUF_SIZE];
    size_t                    request_length;
    size_t                    reply_length;
    volatile int              stop = 0;

    memset(&mgmt_config, 0, sizeof(mgmt_config));
    leap_mgmt_controller_init(&mgmt, &mgmt_config);
    neg_pd_ctrl_setup_session(&mgmt, &pd);

    leap_pd_profile_map_init_default(&profile);
    memset(&mock, 0, sizeof(mock));
    mock.now_us = 1000000u;

    request_length = leap_pd_build_digital_exchange_mapped(
        request,
        sizeof(request),
        10u,
        100000u,
        &profile,
        0x0001u,
        1000000u,
        50000u);
    ASSERT_TRUE(request_length > 0u);
    ASSERT_EQ_INT(
        leap_pd_exchange_view(request, request_length, &request_view),
        LEAP_PD_COMMON_OK);

    memset(&status, 0, sizeof(status));
    status.latest_process_sequence_consumed = 10u;
    status.status_code                      = (uint16_t)LEAP_STATUS_OK;

    reply_length = leap_pd_build_exchange_reply(
        mock.exchange_reply,
        sizeof(mock.exchange_reply),
        request_view.header,
        request_view.write_data,
        request_view.write_length,
        request_view.read_reservation,
        request_view.read_length,
        &status);
    ASSERT_TRUE(reply_length > 0u);
    mock.exchange_reply_length = reply_length;

    mock.now_us = 1060001u;

    memset(&io, 0, sizeof(io));
    io.user_ctx              = &mock;
    io.send_pd               = neg_mock_send_pd;
    io.wait_exchange_reply   = neg_mock_wait_exchange_reply;
    io.monotonic_us          = neg_mock_monotonic_us;

    ASSERT_EQ_INT(
        leap_pd_controller_run_one_cycle(&pd, &mgmt, &io, k_peer_mac, &stop, 0),
        LEAP_PD_CTRL_OK);
    ASSERT_TRUE(pd.stats.reply_stale_rejects == 1u);
    ASSERT_TRUE(pd.stats.reply_rejects == 1u);
    ASSERT_TRUE(pd.stats.exchange_replies == 0u);
}

void leap_run_negative_frame_tests(void)
{
    printf("negative frames\n");
    RUN_TEST(test_stack_rejects_duplicate_pd_without_output_change);
    RUN_TEST(test_diag_counts_duplicate_process_sequence);
    RUN_TEST(test_pd_controller_rejects_stale_exchange_reply);
}
