/*
 * leap_linux_pd.h
 *
 * Linux transport adapter for leap_pd_controller.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LINUX_PD_H
#define LEAP_LINUX_PD_H

#include "leap/leap_pd_controller.h"
#include "leap/leap_raw_linux.h"

typedef struct LeapLinuxPdTransport
{
    LeapRawLinuxSocket* sock;
} LeapLinuxPdTransport;

void leap_linux_pd_init_io(
    LeapPdControllerIo*    io,
    LeapLinuxPdTransport* transport);

#endif /* LEAP_LINUX_PD_H */
