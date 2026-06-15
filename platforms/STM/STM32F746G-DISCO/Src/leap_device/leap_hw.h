/*
 * STM32F746G-Discovery simulated LEAP PD I/O.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_STM746_HW_H
#define LEAP_STM746_HW_H

#include <stdint.h>

typedef struct LeapStm746IoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t safe_outputs;
    uint16_t io_status;
    uint8_t  safe_active;
} LeapStm746IoShadow;

void leap_hw_init(void);
void leap_hw_refresh_inputs(LeapStm746IoShadow *io);
void leap_hw_apply_outputs(LeapStm746IoShadow *io, uint16_t outputs);
void leap_hw_enter_safe(LeapStm746IoShadow *io);
void leap_hw_set_locate_led(uint8_t on);
uint64_t leap_hw_monotonic_us(void);

#endif /* LEAP_STM746_HW_H */
