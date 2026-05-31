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
} ClearcoreLeapIoShadow;

void clearcore_leap_io_init(ClearcoreLeapIoShadow *io);

void clearcore_leap_io_apply_outputs(ClearcoreLeapIoShadow *io, uint16_t outputs);

void clearcore_leap_io_enter_safe(ClearcoreLeapIoShadow *io);

void clearcore_leap_io_refresh_inputs(ClearcoreLeapIoShadow *io);

#ifdef __cplusplus
}
#endif

#endif /* CLEARCORE_LEAP_IO_H_ */
