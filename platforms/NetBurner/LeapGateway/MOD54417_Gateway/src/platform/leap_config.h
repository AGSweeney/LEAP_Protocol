/*
 * leap_config.h - LEAP transport defaults for NetBurner LEAP Gateway.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_NB_CONFIG_H
#define LEAP_NB_CONFIG_H

#include <stdint.h>

#include "leap/leap_protocol.h"

#define LEAP_RTEMS_ETHERTYPE LEAP_ETHERTYPE_DEVELOPMENT
#define LEAP_MAX_FRAME_BYTES (LEAP_HEADER_LENGTH_V1 + LEAP_MAX_PAYLOAD_V1)
#define LEAP_ETH_HEADER_BYTES 14u
#define LEAP_MIN_TX_ETH_PAYLOAD 50u
#define LEAP_ETHERTYPE_ALT LEAP_ETHERTYPE_EXPERIMENTAL_ALT

#ifndef LEAP_RTEMS_ANSI_COLOR
#define LEAP_RTEMS_ANSI_COLOR 0
#endif

#if LEAP_RTEMS_ANSI_COLOR
#define LEAP_ANSI_RESET  "\x1b[0m"
#define LEAP_ANSI_BANNER "\x1b[1;36m"
#define LEAP_ANSI_OK     "\x1b[1;32m"
#define LEAP_ANSI_WARN   "\x1b[1;33m"
#define LEAP_ANSI_ERR    "\x1b[1;31m"
#define LEAP_ANSI_INFO   "\x1b[36m"
#define LEAP_ANSI_DIM    "\x1b[2m"
#else
#define LEAP_ANSI_RESET  ""
#define LEAP_ANSI_BANNER ""
#define LEAP_ANSI_OK     ""
#define LEAP_ANSI_WARN   ""
#define LEAP_ANSI_ERR    ""
#define LEAP_ANSI_INFO   ""
#define LEAP_ANSI_DIM    ""
#endif

#ifndef LEAP_TS_FMT
#define LEAP_TS_FMT "[%s] "
#endif

#endif /* LEAP_NB_CONFIG_H */
