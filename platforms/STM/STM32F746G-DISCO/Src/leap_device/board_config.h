/*
 * STM32F746G-Discovery LEAP device board configuration.
 *
 * Profile: LEAP_PROFILE_DIGITAL_IO_8X8 with fully simulated I/O (no GPIO).
 * Inputs mirror the output shadow so EXCHANGE loops close on the wire.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_STM746_BOARD_CONFIG_H
#define LEAP_STM746_BOARD_CONFIG_H

#include "leap/leap_protocol.h"

#define LEAP_STM746_PRODUCT_CODE       0x0746F746u
#define LEAP_STM746_FIRMWARE_REVISION  2u

#define LEAP_STM746_DO_COUNT           8u
#define LEAP_STM746_DI_COUNT           8u
#define LEAP_STM746_PROFILE_ID         LEAP_PROFILE_DIGITAL_IO_8X8

#endif /* LEAP_STM746_BOARD_CONFIG_H */
