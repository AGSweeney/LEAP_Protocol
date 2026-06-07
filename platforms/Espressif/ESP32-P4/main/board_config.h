#pragma once

#include "leap/leap_protocol.h"

/*
 * Waveshare ESP32-P4-WIFI6-POE-ETH
 * https://docs.waveshare.com/ESP32-P4-WIFI6-POE-ETH
 *
 * LEAP PD I/O uses the 40-pin header (schematic P6). Reserved elsewhere:
 * I2C 7/8, I2S 9-13, Ethernet 31/50-52, SD 39-44, PA 53, strapping 0/1.
 */

/* ── LEAP identity ─────────────────────────────────────────────────── */
#define LEAP_P4_PRODUCT_CODE           0x0450E601u
#define LEAP_P4_FIRMWARE_REVISION      1u

/* ── Ethernet (IP101, external RMII REF clock) ─────────────────────── */
#define ETH_PHY_ADDR                   1
#define ETH_MDC_PIN                    31
#define ETH_MDIO_PIN                   52
#define ETH_REF_CLK_PIN                50
#define ETH_PHY_RST_PIN                51

/* ── Locate / identify indicator (OUT_LE on H2 header) ─────────────── */
#define LOCATE_LED_PIN                 38
#define LOCATE_LED_INVERT              0

/* ── LEAP PD digital I/O (8 outputs + 8 inputs on 40-pin header) ───── */
#define LEAP_DO_COUNT                  8u
#define LEAP_DI_COUNT                  8u
#define LEAP_PROFILE_ID                LEAP_PROFILE_DIGITAL_IO_8X8

#define LEAP_P4_GPIO_OUT0              2
#define LEAP_P4_GPIO_OUT1              3
#define LEAP_P4_GPIO_OUT2              4
#define LEAP_P4_GPIO_OUT3              5
#define LEAP_P4_GPIO_OUT4              6
#define LEAP_P4_GPIO_OUT5              20
#define LEAP_P4_GPIO_OUT6              21
#define LEAP_P4_GPIO_OUT7              22

#define LEAP_P4_GPIO_IN0               23
#define LEAP_P4_GPIO_IN1               24
#define LEAP_P4_GPIO_IN2               25
#define LEAP_P4_GPIO_IN3               26
#define LEAP_P4_GPIO_IN4               27
#define LEAP_P4_GPIO_IN5               32
#define LEAP_P4_GPIO_IN6               33
#define LEAP_P4_GPIO_IN7               36
