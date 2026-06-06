/*
 * LP-AM243 LaunchPad — LEAP device board configuration
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_AM243_BOARD_CONFIG_H
#define LEAP_AM243_BOARD_CONFIG_H

#include "leap/leap_protocol.h"

/*
 * PD digital outputs map to LaunchPad test LEDs (active high):
 *   bit0 TEST_LED1_GRN  (GPMC0_AD7)
 *   bit1 TEST_LED2_RED  (UART0_RTSn)
 *   bit2 TEST_LED3_RED  (PRG1_PRU1_GPO18)
 *   bit3 TEST_LED4_GRN  (PRG1_PRU1_GPO19)
 *
 * Ethernet PHY reset / RGMII mux GPIOs are board-revision specific (E2 vs E3).
 * leap_hw_eth_bringup() owns them — do not use as PD outputs.
 *
 * PD digital inputs:
 *   bit0 USER push button (UART0_CTSn, active low)
 */
#define LEAP_AM243_PRODUCT_CODE      0x0243A243u
#define LEAP_AM243_FIRMWARE_REVISION 1u
#define LEAP_AM243_PROFILE_ID        LEAP_PROFILE_DIGITAL_IO_8X8
#define LEAP_AM243_DO_COUNT          8u
#define LEAP_AM243_DI_COUNT          8u

#define LEAP_AM243_STATUS_LED_BIT    0u

/*
 * UART logging: warnings and errors only. Define LEAP_DEVICE_HOST_TRACE_FORCE
 * (e.g. -DLEAP_DEVICE_HOST_TRACE_FORCE) to enable LEAP_AM243_LOG_INFO trace.
 */
#ifndef LEAP_DEVICE_HOST_TRACE_FORCE
#define LEAP_DEVICE_HOST_TRACE_ENABLE 0
#endif

#endif /* LEAP_AM243_BOARD_CONFIG_H */
