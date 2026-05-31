/*
 * test_pd_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_dir_device.h"
#include "leap/leap_pd_common.h"
#include "leap/leap_protocol.h"

#include <string.h>

TEST(test_pd_build_and_parse_digital_write)
{
    LeapPdBuildParams params;
    LeapPdEndpointView view;
    uint8_t            frame[128];
    size_t             frame_length;
    uint16_t           outputs = 0u;

    memset(&params, 0, sizeof(params));
    params.profile_id       = LEAP_PROFILE_DIGITAL_IO_16X16;
    params.process_sequence = 42u;
    params.endpoint_flags   = LEAP_PD_FLAG_APPLY_OUTPUTS;

    frame_length = leap_pd_build_digital_write(
        frame, sizeof(frame), &params, 0x00A5u);
    ASSERT_TRUE(frame_length > sizeof(LeapEndpointDataHeader));

    ASSERT_EQ_INT(
        leap_pd_endpoint_view(frame, frame_length, &view),
        LEAP_PD_COMMON_OK);
    ASSERT_EQ_U16(view.header->endpoint_id, LEAP_ENDPOINT_DIGITAL_OUTPUTS);
    ASSERT_EQ_INT(
        leap_pd_unpack_digital16x16_outputs(&view, &outputs),
        LEAP_PD_COMMON_OK);
    ASSERT_EQ_U16(outputs, 0x00A5u);
}

TEST(test_pd_build_digital_exchange_layout)
{
    LeapPdExchangeView view;
    uint8_t            frame[128];
    size_t             frame_length;

    frame_length = leap_pd_build_digital_exchange(
        frame, sizeof(frame), 100u, 100000u, LEAP_PROFILE_DIGITAL_IO_16X16, 0x0003u);
    ASSERT_TRUE(frame_length > sizeof(LeapExchangeHeader));

    ASSERT_EQ_INT(
        leap_pd_exchange_view(frame, frame_length, &view),
        LEAP_PD_COMMON_OK);
    ASSERT_EQ_U16(view.header->write_endpoint_id, LEAP_ENDPOINT_DIGITAL_OUTPUTS);
    ASSERT_EQ_U16(view.header->read_endpoint_id, LEAP_ENDPOINT_DIGITAL_INPUTS);
    ASSERT_TRUE(view.write_length == sizeof(LeapProfileDigital16x16));
    ASSERT_TRUE(view.read_length == sizeof(LeapProfileDigital16x16));
}

TEST(test_pd_profile_map_from_dir)
{
    LeapDirDeviceContext dir;
    LeapDirDeviceConfig  config;
    LeapPdProfileMap     map;

    memset(&config, 0, sizeof(config));
    config.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    config.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;
    leap_dir_device_init(&dir, &config);

    ASSERT_EQ_INT(leap_pd_profile_map_from_dir(&dir, &map), LEAP_PD_COMMON_OK);
    ASSERT_TRUE(map.valid != 0);
    ASSERT_EQ_U32(map.profile_id, LEAP_PROFILE_DIGITAL_IO_16X16);
    ASSERT_EQ_U16(map.write_endpoint_id, LEAP_ENDPOINT_DIGITAL_OUTPUTS);
    ASSERT_EQ_U16(map.read_endpoint_id, LEAP_ENDPOINT_DIGITAL_INPUTS);
}

TEST(test_pd_validate_exchange_reply_sequence)
{
    LeapPdProfileMap     profile;
    LeapPdExchangeView   request_view;
    uint8_t              request[256];
    uint8_t              reply[256];
    size_t               request_length;
    size_t               reply_length;
    LeapExchangeStatus   status;
    LeapExchangeStatus   reply_status;

    leap_pd_profile_map_init_default(&profile);

    request_length = leap_pd_build_digital_exchange(
        request,
        sizeof(request),
        55u,
        100000u,
        LEAP_PROFILE_DIGITAL_IO_16X16,
        0x0007u);
    ASSERT_TRUE(request_length > 0u);
    ASSERT_EQ_INT(
        leap_pd_exchange_view(request, request_length, &request_view),
        LEAP_PD_COMMON_OK);

    memset(&status, 0, sizeof(status));
    status.latest_process_sequence_consumed = 55u;
    status.status_code                      = (uint16_t)LEAP_STATUS_OK;

    reply_length = leap_pd_build_exchange_reply(
        reply,
        sizeof(reply),
        request_view.header,
        request_view.write_data,
        request_view.write_length,
        request_view.read_reservation,
        request_view.read_length,
        &status);
    ASSERT_TRUE(reply_length > 0u);

    ASSERT_EQ_INT(
        leap_pd_validate_exchange_reply(
            reply, reply_length, &profile, 55u, NULL, &reply_status),
        LEAP_PD_COMMON_OK);

    ASSERT_EQ_INT(
        leap_pd_validate_exchange_reply(
            reply, reply_length, &profile, 54u, NULL, NULL),
        LEAP_PD_COMMON_SEQUENCE_MISMATCH);
}

void leap_run_pd_common_tests(void)
{
    printf("pd common\n");
    RUN_TEST(test_pd_build_and_parse_digital_write);
    RUN_TEST(test_pd_build_digital_exchange_layout);
    RUN_TEST(test_pd_profile_map_from_dir);
    RUN_TEST(test_pd_validate_exchange_reply_sequence);
}
