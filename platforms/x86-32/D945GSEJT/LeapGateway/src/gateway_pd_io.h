/*
 * gateway_pd_io.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_PD_IO_H
#define LEAP_GATEWAY_PD_IO_H

#include "leap/leap_pd_controller.h"
#include "leap_transport.h"

void leap_gateway_pd_io_init(LeapPdControllerIo* io, LeapRtemsTransport* transport);

#endif /* LEAP_GATEWAY_PD_IO_H */
