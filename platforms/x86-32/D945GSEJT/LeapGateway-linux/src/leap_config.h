/*
 * leap_config.h — Linux gateway equivalents of LeapPort transport defaults.
 *
 * Drop-in replacement for LeapPort/src/leap_config.h (only the pieces the
 * shared LeapGateway sources reference).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_CONFIG_H
#define LEAP_RTEMS_CONFIG_H

#include <stdint.h>

#include "leap/leap_protocol.h"

#define LEAP_RTEMS_ETHERTYPE LEAP_ETHERTYPE_DEVELOPMENT

#define LEAP_RTEMS_RX_BUF_SIZE     1600u
#define LEAP_RTEMS_RECV_TIMEOUT_MS 5

/* Max serialized LEAP frame (header + payload). */
#define LEAP_MAX_FRAME_BYTES (LEAP_HEADER_LENGTH_V1 + LEAP_MAX_PAYLOAD_V1)

#endif /* LEAP_RTEMS_CONFIG_H */
