/*
 * leap_board_linux.c — D945GSEJT LPT1 digital I/O on Alpine Linux (x86).
 *
 * Uses iopl(3) or ioperm() for legacy port I/O at 0x378. Requires root.
 * Pinout: D945GSEJT 26-pin parallel header (Table 19) — see leap_board.h.
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

    /* 5 physical status inputs — header pins 19, 21, 23, 25, 4. */
    inputs |= (uint16_t)(((status >> LEAP_LPT_STATUS_ACK_BIT) & 0x1u) << 0);    /* pin 19 ACK# */
    inputs |= (uint16_t)((((status >> LEAP_LPT_STATUS_BUSY_BIT) ^ 0x1u) & 0x1u) << 1); /* pin 21 BUSY */
    inputs |= (uint16_t)(((status >> LEAP_LPT_STATUS_PERROR_BIT) & 0x1u) << 2); /* pin 23 PERROR */
    inputs |= (uint16_t)(((status >> LEAP_LPT_STATUS_SELECT_BIT) & 0x1u) << 3); /* pin 25 SELECT */
    inputs |= (uint16_t)(((status >> LEAP_LPT_STATUS_FAULT_BIT) & 0x1u) << 4);  /* pin 4 FAULT# */

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

        control = (uint8_t)((control & (uint8_t)~LEAP_LPT_CONTROL_BIDIR) |
                            LEAP_LPT_CONTROL_OUTPUTS_IDLE);
        leap_lpt_out8(LEAP_LPT_CONTROL_PORT, control);
        leap_lpt_out8(LEAP_LPT_DATA_PORT, LEAP_LPT_LOGICAL_TO_HW(0u));
    }

    io->io_status = (g_port_io_ready != 0) ? LEAP_DIO_STATUS_OK : LEAP_DIO_STATUS_OUTPUT_SHORT;
}

void
leap_rtems_board_apply_outputs(LeapRtemsBoardIo* io, uint16_t outputs)
{
    const uint8_t logical = (uint8_t)(outputs & 0xffu);

    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = (uint16_t)logical;

    if (g_port_io_ready != 0)
    {
        leap_lpt_out8(LEAP_LPT_DATA_PORT, LEAP_LPT_LOGICAL_TO_HW(logical));
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

const char*
leap_board_description(void)
{
    return "LPT1 8x8 I/O";
}

const char*
leap_board_pci_address(void)
{
    return NULL;
}

void
leap_board_read_registers(uint8_t* data, uint8_t* status, uint8_t* control)
{
    if (data != NULL)
    {
        *data = (g_port_io_ready != 0) ? leap_lpt_in8(LEAP_LPT_DATA_PORT) : 0u;
    }
    if (status != NULL)
    {
        *status = (g_port_io_ready != 0) ? leap_lpt_in8(LEAP_LPT_STATUS_PORT) : 0u;
    }
    if (control != NULL)
    {
        *control = (g_port_io_ready != 0) ? leap_lpt_in8(LEAP_LPT_CONTROL_PORT) : 0u;
    }
}

uint16_t
leap_board_read_physical_inputs(void)
{
    if (g_port_io_ready == 0)
    {
        return 0u;
    }

    return leap_board_read_native_inputs();
}
