/*
 * gateway_rtems_io.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_RTEMS_IO_H
#define LEAP_GATEWAY_RTEMS_IO_H

#include "leap/leap_controller_stack.h"
#include "leap_transport.h"

void leap_gateway_controller_io_init(
    LeapControllerStackIo* io,
    LeapRtemsTransport*    transport);

#endif /* LEAP_GATEWAY_RTEMS_IO_H */
