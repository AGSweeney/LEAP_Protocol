/*
 * leap_linux_io.h
 *
 * Simulated digital I/O shadow for the Linux loopback device example.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LINUX_IO_H
#define LEAP_LINUX_IO_H

#include <stdint.h>

typedef struct LeapLinuxIoShadow
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t safe_outputs;
    int      safe_active;
} LeapLinuxIoShadow;

void leap_linux_io_init(LeapLinuxIoShadow* io);

void leap_linux_io_apply_outputs(LeapLinuxIoShadow* io, uint16_t outputs);

void leap_linux_io_enter_safe(LeapLinuxIoShadow* io);

#endif /* LEAP_LINUX_IO_H */
