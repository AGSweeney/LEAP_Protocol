// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

/*
 * Kincony KC868-A16 - Hardware Pin Definitions
 * =============================================
 *
 * Board:  Kincony KC868-A16
 * MCU:    ESP32 (classic)
 * PHY:    LAN8720A Ethernet (RMII clock out on GPIO17)
 *
 * Exposed on the enclosure:
 *   - RJ45 Ethernet
 *   - 16 relay outputs (Y01–Y16) via PCF8574
 *   - 16 opto-isolated inputs (X01–X16) via PCF8574
 *   - 4 analog inputs (A1–A4) — not mapped to LEAP PD yet
 *
 * Internal / auxiliary (not LEAP PD I/O):
 *   - RS485 UART (GPIO13 TX, GPIO16 RX)
 *   - IR receiver GPIO2, IR transmitter GPIO15 (locate indicator)
 *   - HT1/HT2/HT3 local GPIO inputs (GPIO32, GPIO33, GPIO14)
 */

/* ── Ethernet (LAN8720A RMII) ─────────────────────────────────────── */
#define ETH_PHY_ADDR        0
#define ETH_MDC_PIN         23
#define ETH_MDIO_PIN        18
#define ETH_CLK_PIN         17

/* ── I2C (PCF8574 I/O expanders) ──────────────────────────────────── */
#define I2C_SDA_PIN         4
#define I2C_SCL_PIN         5
#define I2C_FREQ_HZ         400000u

#define PCF8574_ADDR_INPUTS_1_8   0x22u
#define PCF8574_ADDR_INPUTS_9_16  0x21u
#define PCF8574_ADDR_OUTPUTS_1_8  0x24u
#define PCF8574_ADDR_OUTPUTS_9_16 0x25u

/* PCF8574 lines are active-low (inverted). */
#define PCF8574_INVERTED    1

/* ── Locate / identify indicator ──────────────────────────────────── */
#define LOCATE_LED_PIN      15
#define LOCATE_LED_INVERT   0

/* ── LEAP PD digital I/O bit map ───────────────────────────────────
 *
 * Outputs (16 bits):
 *   bit0–7   relays Y01–Y08 (outputs_1_8 expander)
 *   bit8–15  relays Y09–Y16 (outputs_9_16 expander)
 *
 * Inputs (16 bits):
 *   bit0–7   inputs X01–X08 (inputs_1_8 expander)
 *   bit8–15  inputs X09–X16 (inputs_9_16 expander)
 */
#define LEAP_DO_COUNT       16u
#define LEAP_DI_COUNT       16u

#define LEAP_DO_Y01_BIT     0u
#define LEAP_DO_Y16_BIT     15u
#define LEAP_DI_X01_BIT     0u
#define LEAP_DI_X16_BIT     15u
