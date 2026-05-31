/*
 * test_dir_device.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "leap_test_frame.h"

#include "leap/leap_dir_device.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define TEST_DIR_BUF_SIZE 512u

static const uint8_t k_controller_mac[6] = { 0x02, 0x11, 0x22, 0x33, 0x44, 0x55 };
static const uint8_t k_device_mac[6]     = { 0x02, 0x66, 0x77, 0x88, 0x99, 0xAA };

static void dir_setup(
    LeapDirDeviceContext*  dir,
    LeapDiscDeviceContext* disc,
    LeapMgmtDeviceContext* mgmt)
{
    LeapDirDeviceConfig config;

    memset(&config, 0, sizeof(config));
    memcpy(config.identity.primary_mac, k_device_mac, 6);
    config.default_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    config.active_profile_id  = LEAP_PROFILE_DIGITAL_IO_16X16;

    leap_dir_device_init(dir, &config);
    leap_disc_device_init(disc, NULL);
    leap_mgmt_device_init(mgmt, NULL);
    leap_mgmt_device_on_transport_ready(mgmt);
    leap_dir_device_sync_disc(dir, disc);
}

TEST(test_dir_select_profile_enters_configured)
{
    LeapDirDeviceContext  dir;
    LeapDiscDeviceContext disc;
    LeapMgmtDeviceContext mgmt;
    LeapDirDeviceResult   result;
    LeapSelectProfileRequest select;
    uint8_t               frame[TEST_DIR_BUF_SIZE];
    size_t                frame_length = 0u;
    const LeapProfileReply* profile_reply;

    dir_setup(&dir, &disc, &mgmt);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&mgmt), LEAP_STATE_INIT);

    memset(&select, 0, sizeof(select));
    select.requested_profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIR_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_SELECT_PROFILE,
            0u,
            1u,
            (const uint8_t*)&select,
            sizeof(select)) == 0);

    ASSERT_EQ_INT(
        leap_dir_device_process_frame(
            &dir, &disc, &mgmt, k_controller_mac, frame, frame_length, &result),
        LEAP_DIR_DEVICE_OK);
    ASSERT_EQ_U16(result.message_type, LEAP_DIR_PROFILE_REPLY);
    ASSERT_TRUE((result.flags & LEAP_DIR_DEVICE_FLAG_PROFILE_SELECTED) != 0u);
    ASSERT_TRUE((result.flags & LEAP_DIR_DEVICE_FLAG_STATE_CONFIGURED) != 0u);
    ASSERT_EQ_INT(leap_mgmt_device_get_state(&mgmt), LEAP_STATE_CONFIGURED);

    profile_reply = (const LeapProfileReply*)result.payload;
    ASSERT_EQ_U32(profile_reply->active_profile_id, LEAP_PROFILE_DIGITAL_IO_16X16);
    ASSERT_EQ_U16(profile_reply->endpoint_count, 2u);
}

TEST(test_dir_read_directory_returns_tlvs)
{
    LeapDirDeviceContext  dir;
    LeapDiscDeviceContext disc;
    LeapMgmtDeviceContext mgmt;
    LeapDirDeviceResult   result;
    LeapReadDirectoryRequest req;
    uint8_t               frame[TEST_DIR_BUF_SIZE];
    size_t                frame_length = 0u;
    const LeapReadDirectoryReply* reply_hdr;

    dir_setup(&dir, &disc, &mgmt);
    leap_mgmt_device_on_profile_selected(&mgmt);

    memset(&req, 0, sizeof(req));
    req.max_bytes = 256u;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIR_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_READ_DIRECTORY,
            0u,
            1u,
            (const uint8_t*)&req,
            sizeof(req)) == 0);

    ASSERT_EQ_INT(
        leap_dir_device_process_frame(
            &dir, &disc, &mgmt, k_controller_mac, frame, frame_length, &result),
        LEAP_DIR_DEVICE_OK);
    ASSERT_EQ_U16(result.message_type, LEAP_DIR_READ_DIRECTORY_REPLY);
    ASSERT_TRUE(result.payload_length > sizeof(LeapReadDirectoryReply));

    reply_hdr = (const LeapReadDirectoryReply*)result.payload;
    ASSERT_TRUE(reply_hdr->returned_bytes > 0u);
}

TEST(test_dir_select_profile_rejects_unknown_profile)
{
    LeapDirDeviceContext  dir;
    LeapDiscDeviceContext disc;
    LeapMgmtDeviceContext mgmt;
    LeapDirDeviceResult   result;
    LeapSelectProfileRequest select;
    uint8_t               frame[TEST_DIR_BUF_SIZE];
    size_t                frame_length = 0u;

    dir_setup(&dir, &disc, &mgmt);

    memset(&select, 0, sizeof(select));
    select.requested_profile_id = 0xDEADBEEFu;

    ASSERT_TRUE(
        leap_test_frame_build(
            frame,
            TEST_DIR_BUF_SIZE,
            &frame_length,
            0u,
            (uint16_t)LEAP_SERVICE_DIR,
            LEAP_DIR_SELECT_PROFILE,
            0u,
            1u,
            (const uint8_t*)&select,
            sizeof(select)) == 0);

    ASSERT_EQ_INT(
        leap_dir_device_process_frame(
            &dir, &disc, &mgmt, k_controller_mac, frame, frame_length, &result),
        LEAP_DIR_DEVICE_PROFILE_MISMATCH);
    ASSERT_EQ_U16(result.error_code, LEAP_STATUS_PROFILE_MISMATCH);
}

void leap_run_dir_device_tests(void)
{
    printf("dir device\n");
    RUN_TEST(test_dir_select_profile_enters_configured);
    RUN_TEST(test_dir_read_directory_returns_tlvs);
    RUN_TEST(test_dir_select_profile_rejects_unknown_profile);
}
