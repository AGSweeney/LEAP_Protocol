/*
 * leap_linux_io.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_linux_io.h"

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
    io->safe_active     = 1;
}

void leap_linux_io_apply_outputs(LeapLinuxIoShadow* io, uint16_t outputs)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 0;
    io->digital_outputs = outputs;
    io->digital_inputs  = (uint16_t)((io->digital_inputs + 1u) & 0x000Fu);

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

    printf("I/O shadow: safe outputs active (outputs=0x%04X)\n", io->digital_outputs);
}
