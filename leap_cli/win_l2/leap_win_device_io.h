/*
 * leap_win_device_io.h
 *
 * Simulated digital I/O shadow for the Windows LEAP device example.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_WIN_DEVICE_IO_H
#define LEAP_WIN_DEVICE_IO_H

#include <stdint.h>

typedef struct LeapWinDeviceIoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t safe_outputs;
    uint16_t io_status;
    int      safe_active;
    int      outputs_dirty;
} LeapWinDeviceIoShadow;

void leap_win_device_io_init(LeapWinDeviceIoShadow* io);

void leap_win_device_io_apply_outputs(LeapWinDeviceIoShadow* io, uint16_t outputs);

void leap_win_device_io_enter_safe(LeapWinDeviceIoShadow* io);

#endif /* LEAP_WIN_DEVICE_IO_H */
