/*
 * LP-AM243 LaunchPad hardware abstraction for LEAP PD I/O.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_AM243_HW_H
#define LEAP_AM243_HW_H

#include <stdint.h>

typedef struct LeapAm243IoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t io_status;
    uint8_t  safe_active;
} LeapAm243IoShadow;

/* Route RGMII mux to ICSSG (LOW). Safe to call at port open; no PHY reset. */
void     leap_hw_eth_assert_icssg_path(void);
/* Release PHY reset and route RGMII to ICSSG (call before Enet init). */
void     leap_hw_eth_bringup(void);
void     leap_hw_init(void);
void     leap_hw_refresh_inputs(LeapAm243IoShadow *io);
void     leap_hw_apply_outputs(LeapAm243IoShadow *io, uint16_t outputs);
void     leap_hw_enter_safe(LeapAm243IoShadow *io);
void     leap_hw_set_status_led(uint8_t on);
uint64_t leap_hw_monotonic_us(void);

#endif /* LEAP_AM243_HW_H */
