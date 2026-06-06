/*
 * clearcore_leap_io.h
 *
 * ClearCore digital I/O shadow for LEAP PD binding.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CLEARCORE_LEAP_IO_H_
#define CLEARCORE_LEAP_IO_H_

#include <stdint.h>

#define CLEARCORE_LEAP_DO_COUNT     6u
#define CLEARCORE_LEAP_DI_COUNT     6u
/* LEAP_PROFILE_DIGITAL_IO_8X8 — keep literal; this header must not include leap_core */
#define CLEARCORE_LEAP_PROFILE_ID     0x00010001u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ClearcoreLeapIoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t safe_outputs;
    uint16_t io_status;
    int      safe_active;
    int      outputs_dirty;
    /* Bit N set: ConnectorIO N is in OUTPUT_DIGITAL (not sampled as input). */
    uint8_t  pin_output_mask;
    /* Bit N set: GPIO N is currently driven high. */
    uint8_t  pin_state_mask;
} ClearcoreLeapIoShadow;

void clearcore_leap_io_init(ClearcoreLeapIoShadow *io);

void clearcore_leap_io_apply_outputs(ClearcoreLeapIoShadow *io, uint16_t outputs);

void clearcore_leap_io_enter_safe(ClearcoreLeapIoShadow *io);

void clearcore_leap_io_refresh_inputs(ClearcoreLeapIoShadow *io);

#ifdef __cplusplus
}
#endif

#endif /* CLEARCORE_LEAP_IO_H_ */
