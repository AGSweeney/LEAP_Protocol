/*
 * leap_board.h — D945GSEJT digital I/O shadow for LEAP-PD.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_BOARD_H
#define LEAP_RTEMS_BOARD_H

#include <stdint.h>

/* LPT1 base and register offsets (SPP/bi-directional mode). */
#define LEAP_LPT1_BASE_ADDR       0x378u
#define LEAP_LPT_REG_DATA_OFFSET  0x0u
#define LEAP_LPT_REG_STATUS_OFFSET 0x1u
#define LEAP_LPT_REG_CONTROL_OFFSET 0x2u

/* Control bit used for data direction on PS/2-capable ports. */
#define LEAP_LPT_CONTROL_BIDIR    0x20u

typedef struct LeapRtemsBoardIo
{
    uint16_t digital_outputs;
    uint16_t digital_inputs;
    uint16_t io_status;
} LeapRtemsBoardIo;

void leap_rtems_board_init(LeapRtemsBoardIo* io);
void leap_rtems_board_apply_outputs(LeapRtemsBoardIo* io, uint16_t outputs);
void leap_rtems_board_enter_safe(LeapRtemsBoardIo* io);
void leap_rtems_board_sample_inputs(LeapRtemsBoardIo* io);

#endif /* LEAP_RTEMS_BOARD_H */
