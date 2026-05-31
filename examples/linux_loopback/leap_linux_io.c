/*
 * leap_linux_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_io.h"

#include "leap/leap_protocol.h"

#include <stdio.h>

void leap_linux_io_init(LeapLinuxIoShadow* io)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = 0u;
    io->digital_inputs  = 0x0003u;
    io->safe_outputs    = 0u;
    io->io_status       = LEAP_DIO_STATUS_OK;
    io->safe_active     = 1;
    io->outputs_dirty   = 0;
}

void leap_linux_io_apply_outputs(LeapLinuxIoShadow* io, uint16_t outputs)
{
    if (io == NULL)
    {
        return;
    }

    /*
     * Multi-peer / high-rate PD: skip redundant shadow updates when the command
     * value is unchanged (lease refresh may still occur at the PD layer).
     */
    if (io->safe_active == 0 && io->digital_outputs == outputs)
    {
        io->outputs_dirty = 0;
        return;
    }

    io->safe_active     = 0;
    io->digital_outputs = outputs;
    io->digital_inputs  = (uint16_t)((io->digital_inputs + 1u) & 0x000Fu);
    io->io_status       = LEAP_DIO_STATUS_OK;
    io->outputs_dirty   = 1;

    printf("I/O shadow: outputs=0x%04X inputs=0x%04X (live)\n",
           io->digital_outputs,
           io->digital_inputs);
}

void leap_linux_io_enter_safe(LeapLinuxIoShadow* io)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 1;
    io->digital_outputs = io->safe_outputs;
    io->outputs_dirty   = 1;

    printf("I/O shadow: safe outputs active (outputs=0x%04X)\n", io->digital_outputs);
}
