/*
 * test_crc.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "test_harness.h"

#include "leap/leap_crc.h"
#include "leap/leap_protocol.h"

TEST(test_crc16_xmodem_check_vector)
{
    static const uint8_t data[] = "123456789";
    const uint16_t       crc    = leap_crc16_xmodem(data, sizeof(data) - 1u);

    ASSERT_EQ_U16(crc, LEAP_CRC16_XMODEM_CHECK_123456789);
}

TEST(test_crc32c_check_vector)
{
    static const uint8_t data[] = "123456789";
    const uint32_t       crc    = leap_crc32c(data, sizeof(data) - 1u);

    ASSERT_EQ_U32(crc, LEAP_CRC32C_CHECK_123456789);
}

void leap_run_crc_tests(void)
{
    printf("crc\n");
    RUN_TEST(test_crc16_xmodem_check_vector);
    RUN_TEST(test_crc32c_check_vector);
}
