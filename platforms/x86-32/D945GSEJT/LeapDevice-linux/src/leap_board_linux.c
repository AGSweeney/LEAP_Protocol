/*
 * leap_board_linux.c — D945GSEJT LPT1 digital I/O on Alpine Linux (x86).
 *
 * Uses iopl(3) or ioperm() for legacy port I/O at 0x378. Requires root.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_board.h"

#include "leap_config.h"
#include "leap/leap_protocol.h"

#include <stddef.h>
#include <string.h>
#include <sys/io.h>

#define LEAP_LPT_DATA_PORT    ((uint16_t)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_DATA_OFFSET))
#define LEAP_LPT_STATUS_PORT  ((uint16_t)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_STATUS_OFFSET))
#define LEAP_LPT_CONTROL_PORT ((uint16_t)(LEAP_LPT1_BASE_ADDR + LEAP_LPT_REG_CONTROL_OFFSET))

static int g_port_io_ready;

static int
leap_board_enable_port_io(void)
{
    if (g_port_io_ready != 0)
    {
        return 0;
    }

    if (iopl(3) == 0)
    {
        g_port_io_ready = 1;
        return 0;
    }

    if (ioperm(LEAP_LPT1_BASE_ADDR, 8u, 1) == 0)
    {
        g_port_io_ready = 1;
        return 0;
    }

    return -1;
}

static inline uint8_t
leap_lpt_in8(uint16_t port)
{
    uint8_t value;

    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void
leap_lpt_out8(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t
leap_board_read_native_inputs(void)
{
    const uint8_t status = leap_lpt_in8(LEAP_LPT_STATUS_PORT);
    uint16_t        inputs = 0u;

    inputs |= (uint16_t)(((status >> 6) & 0x1u) << 0);
    inputs |= (uint16_t)((((status >> 7) ^ 0x1u) & 0x1u) << 1);
    inputs |= (uint16_t)(((status >> 5) & 0x1u) << 2);
    inputs |= (uint16_t)(((status >> 4) & 0x1u) << 3);
    inputs |= (uint16_t)(((status >> 3) & 0x1u) << 4);

    return inputs;
}

void
leap_rtems_board_init(LeapRtemsBoardIo* io)
{
    if (io == NULL)
    {
        return;
    }

    (void)leap_board_enable_port_io();

    memset(io, 0, sizeof(*io));

    if (g_port_io_ready != 0)
    {
        uint8_t control = leap_lpt_in8(LEAP_LPT_CONTROL_PORT);

        control = (uint8_t)(control & (uint8_t)~LEAP_LPT_CONTROL_BIDIR);
        leap_lpt_out8(LEAP_LPT_CONTROL_PORT, control);
        leap_lpt_out8(LEAP_LPT_DATA_PORT, 0u);
    }

    io->io_status = (g_port_io_ready != 0) ? LEAP_DIO_STATUS_OK : LEAP_DIO_STATUS_OUTPUT_SHORT;
}

void
leap_rtems_board_apply_outputs(LeapRtemsBoardIo* io, uint16_t outputs)
{
    const uint8_t out8 = (uint8_t)(outputs & 0xffu);

    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = (uint16_t)out8;

    if (g_port_io_ready != 0)
    {
        leap_lpt_out8(LEAP_LPT_DATA_PORT, out8);
    }
}

void
leap_rtems_board_enter_safe(LeapRtemsBoardIo* io)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = 0u;
    leap_rtems_board_apply_outputs(io, 0u);
}

void
leap_rtems_board_sample_inputs(LeapRtemsBoardIo* io)
{
    uint16_t inputs;

    if (io == NULL)
    {
        return;
    }

    if (g_port_io_ready == 0)
    {
        io->digital_inputs = 0u;
        return;
    }

    inputs = leap_board_read_native_inputs();
    inputs |= (uint16_t)((io->digital_outputs & 0x7u) << 5);
    io->digital_inputs = inputs;
}

int
leap_board_port_io_ready(void)
{
    return g_port_io_ready;
}
