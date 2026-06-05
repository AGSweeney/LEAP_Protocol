// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

/*
 * Gledopto GL-C-618WL (Elite 4D-EXMU) - Hardware Pin Definitions
 * ===============================================================
 *
 * Board:  Gledopto GL-C-618WL (Elite 4D-EXMU)
 * MCU:    ESP32 (classic, not S2/S3/C3)
 * PHY:    LAN8720 Ethernet
 *
 * Exposed on the enclosure:
 *   - RJ45 Ethernet
 *   - DC power input
 *   - WS2812 data terminal CH0 (GPIO16)
 *   - WS2812 data terminal CH1 (GPIO2)
 *
 * Internal only (not LEAP PD I/O):
 *   - Relay (GPIO18) energizes the LED power rail at boot
 *   - Function button (GPIO17) on-board UI
 *   - Status LED (GPIO13) internal dumb LED for locate / identify
 *   - RMII / PHY pins
 */

/* ── Ethernet (LAN8720 RMII) ──────────────────────────────────────── */
#define ETH_PHY_ADDR        1
#define ETH_PHY_POWER_PIN   5
#define ETH_MDC_PIN         23
#define ETH_MDIO_PIN        33
#define ETH_CLK_PIN         0

/* ── Exposed LED data outputs (screw terminals) ──────────────────── */
#define LED_DATA_PIN_0      16
#define LED_DATA_PIN_1      2
#define LED_CHANNEL_COUNT   2

/* ── Internal board functions (not exposed terminals) ────────────── */
#define RELAY_PIN           18
#define RELAY_INVERT        0
#define BUTTON_PIN          17
#define EXT_GPIO_PIN        13
#define STATUS_LED_PIN      EXT_GPIO_PIN
#define STATUS_LED_INVERT   0

/* ── Default LED configuration ─────────────────────────────────────
 * CH0: 300x 5050 SMD addressable (WS2812 / SK6812 timing)
 */
#define DEFAULT_NUM_LEDS    300

/*
 * Max per-channel pixel level (0–255). USB bench power cannot drive a 300× 5050
 * strip — use DC input for the controller and a dedicated 5 V at the LED
 * terminals. Raise this only after strip power is wired correctly.
 */
#define LEAP_LED_MAX_LEVEL  0x40u
#define LEAP_LED_WHITE_MAX  0x30u

/* ── LEAP PD digital I/O bit map ───────────────────────────────────
 *
 * Only user-wirable outputs are the two WS2812 data terminals.
 * There are no exposed digital inputs on this controller.
 *
 * Outputs (8 bits):
 *   bit0-3  strip CH0 (GPIO16): red / green / blue / white fill
 *   bit4-7  strip CH1 (GPIO2):  red / green / blue / white fill
 *
 * Inputs:
 *   none exposed — digital_inputs stays 0
 */
#define LEAP_DO_CH0_RED_BIT     0u
#define LEAP_DO_CH0_GREEN_BIT   1u
#define LEAP_DO_CH0_BLUE_BIT    2u
#define LEAP_DO_CH0_WHITE_BIT   3u
#define LEAP_DO_CH1_RED_BIT     4u
#define LEAP_DO_CH1_GREEN_BIT   5u
#define LEAP_DO_CH1_BLUE_BIT    6u
#define LEAP_DO_CH1_WHITE_BIT   7u
