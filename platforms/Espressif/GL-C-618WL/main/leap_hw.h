// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapGlc618wlIoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t safe_outputs;
    uint16_t io_status;
    int      safe_active;
} LeapGlc618wlIoShadow;

void leap_hw_init(uint16_t num_leds);
void leap_hw_apply_outputs(LeapGlc618wlIoShadow *io, uint16_t outputs);
void leap_hw_enter_safe(LeapGlc618wlIoShadow *io);
void leap_hw_refresh_inputs(LeapGlc618wlIoShadow *io);
void leap_hw_set_status_led(uint8_t on);

#ifdef __cplusplus
}
#endif
