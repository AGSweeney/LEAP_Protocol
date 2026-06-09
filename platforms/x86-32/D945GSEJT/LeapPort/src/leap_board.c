/*
 * leap_board.c — D945GSEJT digital I/O over the LPT1 (parallel) port.
 *
 * Outputs D0..D7 drive the LPT data register (DB25 pins 2..9 / IDC header
 * pins 3,5,7,9,11,13,15,17). Inputs are read from the LPT status register and
 * the upper input bits are mirrored from the outputs to fill the 8x8 profile.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_board.h"

#include "leap_config.h"
#include "leap/leap_protocol.h"

#include <stddef.h>
#include <string.h>

#define LEAP_LPT_DATA_PORT    ((uint16_t)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_DATA_OFFSET))
#define LEAP_LPT_STATUS_PORT  ((uint16_t)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_STATUS_OFFSET))
#define LEAP_LPT_CONTROL_PORT ((uint16_t)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_CONTROL_OFFSET))

static inline uint8_t leap_lpt_in8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void leap_lpt_out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t leap_board_read_native_inputs(void)
{
    const uint8_t status = leap_lpt_in8(LEAP_LPT_STATUS_PORT);
    uint16_t inputs = 0u;

    /* 5 physical status inputs from pins 10, 11, 12, 13, 15. */
    inputs |= (uint16_t)(((status >> 6) & 0x1u) << 0);       /* pin 10 (ACK) */
    inputs |= (uint16_t)((((status >> 7) ^ 0x1u) & 0x1u) << 1); /* pin 11 (BUSY, inverted) */
    inputs |= (uint16_t)(((status >> 5) & 0x1u) << 2);       /* pin 12 (PAPER OUT) */
    inputs |= (uint16_t)(((status >> 4) & 0x1u) << 3);       /* pin 13 (SELECT) */
    inputs |= (uint16_t)(((status >> 3) & 0x1u) << 4);       /* pin 15 (ERROR) */

    return inputs;
}

void leap_rtems_board_init(LeapRtemsBoardIo* io)
{
    if (io == NULL)
    {
        return;
    }

    memset(io, 0, sizeof(*io));

    /* Keep data register in output mode ("without switching"). */
    {
        uint8_t control = leap_lpt_in8(LEAP_LPT_CONTROL_PORT);
        control = (uint8_t)(control & (uint8_t)~LEAP_LPT_CONTROL_BIDIR);
        leap_lpt_out8(LEAP_LPT_CONTROL_PORT, control);
    }

    leap_lpt_out8(LEAP_LPT_DATA_PORT, 0u);
    io->io_status = LEAP_DIO_STATUS_OK;
}

void leap_rtems_board_apply_outputs(LeapRtemsBoardIo* io, uint16_t outputs)
{
    const uint8_t out8 = (uint8_t)(outputs & 0xffu);

    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = (uint16_t)out8;
    leap_lpt_out8(LEAP_LPT_DATA_PORT, out8);
}

void leap_rtems_board_enter_safe(LeapRtemsBoardIo* io)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = 0u;
    leap_rtems_board_apply_outputs(io, 0u);
}

void leap_rtems_board_sample_inputs(LeapRtemsBoardIo* io)
{
    uint16_t inputs;

    if (io == NULL)
    {
        return;
    }

    inputs = leap_board_read_native_inputs(); /* bits 0..4 */

    /*
     * Fill the 8-input profile without changing LPT direction:
     * mirror output bits D0..D2 into input bits 5..7.
     */
    inputs |= (uint16_t)((io->digital_outputs & 0x7u) << 5);

    io->digital_inputs = inputs;
}
