/*
 * leap_linux_controller_io.h
 *
 * Linux transport adapter for leap_controller_stack.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_LINUX_CONTROLLER_IO_H
#define LEAP_LINUX_CONTROLLER_IO_H

#include "leap/leap_controller_stack.h"
#include "leap/leap_raw_linux.h"

void leap_linux_controller_io_init(
    LeapControllerStackIo* io,
    LeapRawLinuxSocket*    sock);

#endif /* LEAP_LINUX_CONTROLLER_IO_H */
