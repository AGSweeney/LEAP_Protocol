/*
 * test_frame_vectors.c
 *
 * Golden frame vector replay against leap_frame_parse().
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"
#include "test_util.h"

#include "leap/leap_crc.h"
#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#define TEST_FRAME_BUF_SIZE 256u

static int decode_hex_frame(const char* hex, uint8_t* out, size_t* out_length)
{
    return leap_test_hex_decode(hex, out, TEST_FRAME_BUF_SIZE, out_length);
}

TEST(test_vector2_pd_frame_accepts)
{
    static const char* const k_hex =
        "4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38"
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100";
    uint8_t          frame[TEST_FRAME_BUF_SIZE];
    size_t           frame_length = 0u;
    LeapFrameView    view;
    LeapFrameParseResult result;

    ASSERT_TRUE(decode_hex_frame(k_hex, frame, &frame_length) == 0);
    ASSERT_EQ_INT(frame_length, 72);

    result = leap_frame_parse(frame, frame_length, &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_OK);
    ASSERT_EQ_U16(view.payload_length, 40u);
    ASSERT_EQ_INT(view.leap_byte_count, 72);
    ASSERT_EQ_U16(view.header.service_id, LEAP_SERVICE_PD);
    ASSERT_EQ_U16(view.header.message_type, LEAP_PD_WRITE_ENDPOINT);
}

TEST(test_vector2_crc_values_match_document)
{
    static const char* const k_header_hex =
        "4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38";
    static const char* const k_payload_hex =
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100";
    uint8_t  header[TEST_FRAME_BUF_SIZE];
    uint8_t  payload[TEST_FRAME_BUF_SIZE];
    size_t   header_length  = 0u;
    size_t   payload_length = 0u;
    uint16_t header_crc;
    uint32_t payload_crc;

    ASSERT_TRUE(decode_hex_frame(k_header_hex, header, &header_length) == 0);
    ASSERT_TRUE(decode_hex_frame(k_payload_hex, payload, &payload_length) == 0);

    header[26]  = 0u;
    header[27]  = 0u;
    header_crc  = leap_crc16_xmodem(header, header_length);
    payload_crc = leap_crc32c(payload, payload_length);

    ASSERT_EQ_U16(header_crc, 0x1463u);
    ASSERT_EQ_U32(payload_crc, 0x38DCF994u);
}

TEST(test_vector6_padding_frame_accepts)
{
    static const char* const k_hex =
        "4c45415001002001010005001d2c3b4a140500000000000004003188347a4533"
        "040000000000000000000000000000000000";
    uint8_t       frame[TEST_FRAME_BUF_SIZE];
    size_t        frame_length = 0u;
    LeapFrameView view;
    LeapFrameParseResult result;

    ASSERT_TRUE(decode_hex_frame(k_hex, frame, &frame_length) == 0);
    ASSERT_EQ_INT(frame_length, 50);

    result = leap_frame_parse(frame, frame_length, &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_OK);
    ASSERT_EQ_U16(view.payload_length, 4u);
    ASSERT_EQ_INT(view.leap_byte_count, 36);
}

TEST(test_vector7_bad_header_crc_rejects)
{
    static const char* const k_header_hex =
        "4c45415001002001100001001d2c3b4aed030000d6030000280063146b0623c7";
    static const char* const k_payload_hex =
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f100";
    uint8_t frame[TEST_FRAME_BUF_SIZE];
    size_t  header_length  = 0u;
    size_t  payload_length = 0u;
    LeapFrameView view;
    LeapFrameParseResult result;

    ASSERT_TRUE(decode_hex_frame(k_header_hex, frame, &header_length) == 0);
    ASSERT_TRUE(decode_hex_frame(k_payload_hex, frame + header_length, &payload_length) == 0);

    result = leap_frame_parse(frame, header_length + payload_length, &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_ERR_BAD_HEADER_CRC);
}

TEST(test_vector8_bad_payload_crc_rejects)
{
    static const char* const k_header_hex =
        "4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38";
    static const char* const k_payload_hex =
        "1000000008000500ed030000e803000000c4b3a2960100008813000002000100e00115000000f1ff";
    uint8_t frame[TEST_FRAME_BUF_SIZE];
    size_t  header_length  = 0u;
    size_t  payload_length = 0u;
    LeapFrameView view;
    LeapFrameParseResult result;

    ASSERT_TRUE(decode_hex_frame(k_header_hex, frame, &header_length) == 0);
    ASSERT_TRUE(decode_hex_frame(k_payload_hex, frame + header_length, &payload_length) == 0);

    result = leap_frame_parse(frame, header_length + payload_length, &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_ERR_BAD_PAYLOAD_CRC);
}

TEST(test_open_session_reply_accepts)
{
    static const char* const k_header_hex =
        "4c45415001002003010002001d2c3b4a6e0000000000000018005d25da2f75ba";
    static const char* const k_payload_hex =
        "1d2c3b4a20a10700a08601000500030066778899aabb0000";
    uint8_t frame[TEST_FRAME_BUF_SIZE];
    size_t  header_length  = 0u;
    size_t  payload_length = 0u;
    LeapFrameView view;
    LeapFrameParseResult result;

    ASSERT_TRUE(decode_hex_frame(k_header_hex, frame, &header_length) == 0);
    ASSERT_TRUE(decode_hex_frame(k_payload_hex, frame + header_length, &payload_length) == 0);

    result = leap_frame_parse(frame, header_length + payload_length, &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_OK);
    ASSERT_EQ_U16(view.header.service_id, LEAP_SERVICE_MGMT);
    ASSERT_EQ_U16(view.header.message_type, LEAP_MGMT_OPEN_SESSION_REPLY);
}

TEST(test_truncated_frame_rejects)
{
    static const char* const k_hex =
        "4c45415001002001100001001d2c3b4aed030000d60300002800631494f9dc38";
    uint8_t frame[TEST_FRAME_BUF_SIZE];
    size_t  frame_length = 0u;
    LeapFrameView view;
    LeapFrameParseResult result;

    ASSERT_TRUE(decode_hex_frame(k_hex, frame, &frame_length) == 0);
    result = leap_frame_parse(frame, frame_length, &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_ERR_BAD_LENGTH);
}

TEST(test_bad_magic_rejects)
{
    uint8_t frame[LEAP_HEADER_LENGTH_V1];
    LeapFrameView view;
    LeapFrameParseResult result;

    frame[0] = 'X';
    frame[1] = 'X';
    frame[2] = 'X';
    frame[3] = 'X';

    result = leap_frame_parse(frame, sizeof(frame), &view);
    ASSERT_EQ_INT(result, LEAP_FRAME_ERR_BAD_MAGIC);
}

void leap_run_frame_vector_tests(void)
{
    printf("frame vectors\n");
    RUN_TEST(test_vector2_pd_frame_accepts);
    RUN_TEST(test_vector2_crc_values_match_document);
    RUN_TEST(test_vector6_padding_frame_accepts);
    RUN_TEST(test_vector7_bad_header_crc_rejects);
    RUN_TEST(test_vector8_bad_payload_crc_rejects);
    RUN_TEST(test_open_session_reply_accepts);
    RUN_TEST(test_truncated_frame_rejects);
    RUN_TEST(test_bad_magic_rejects);
}
