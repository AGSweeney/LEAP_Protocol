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

/* Control register @ 0x37A — bits 0..3 drive DB25 pins 1, 14, 16, 17 (inverted). */
#define LEAP_LPT_CONTROL_STROBE_BIT   0u /* pin 1  — spindle/enable on many CNC BOBs */
#define LEAP_LPT_CONTROL_AUTOFD_BIT   1u /* pin 14 — coolant/relay on some boards */
#define LEAP_LPT_CONTROL_INIT_BIT     2u /* pin 16 — relay 2 on some boards */
#define LEAP_LPT_CONTROL_SELECT_BIT   3u /* pin 17 — relay 1 on some boards */
#define LEAP_LPT_CONTROL_BIDIR        0x20u /* bit 5: 1 = data port input */

/*
 * Control outputs are active-low on the connector. Set bits 0..3 so pins 1/14/16/17
 * stay high (relay off on typical opto-isolated CNC breakouts). Not part of LEAP D0..D7.
 */
#define LEAP_LPT_CONTROL_OUTPUTS_IDLE 0x0fu

/*
 * D945GSEJT on-board 26-pin parallel header (Table 19, Standard/SPP).
 * Ground: even pins 10, 12, 14, 16, 18, 20, 22, 24. Pin 26 is key only.
 *
 * Outputs (data register @ 0x378): PD0..PD7 on header pins
 *   3, 5, 7, 9, 11, 13, 15, 17 (LEAP D0..D7).
 *   Hardware is active-low: data register bit 0 drives the line high (off),
 *   bit 1 drives the line low (on). LEAP/logical 1 = on = inverted on the port.
 *
 * Physical status inputs (status register @ 0x379):
 *   LEAP in0: bit 6 ACK#   -> pin 19
 *   LEAP in1: bit 7 BUSY   -> pin 21 (active-high after invert)
 *   LEAP in2: bit 5 PERROR -> pin 23
 *   LEAP in3: bit 4 SELECT -> pin 25
 *   LEAP in4: bit 3 FAULT# -> pin 4
 *
 * LEAP in5..7 mirror D0..D2 (pins 3, 5, 7) to fill the 8x8 profile.
 */
#define LEAP_LPT_STATUS_ACK_BIT     6u
#define LEAP_LPT_STATUS_BUSY_BIT    7u
#define LEAP_LPT_STATUS_PERROR_BIT  5u
#define LEAP_LPT_STATUS_SELECT_BIT  4u
#define LEAP_LPT_STATUS_FAULT_BIT   3u

/* PC LPT data register is inverted: logical off=all ones, logical on=bit clear. */
#define LEAP_LPT_LOGICAL_TO_HW(v) ((uint8_t)(~(uint8_t)(v)))
#define LEAP_LPT_HW_TO_LOGICAL(v) ((uint8_t)(~(uint8_t)(v)))

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
