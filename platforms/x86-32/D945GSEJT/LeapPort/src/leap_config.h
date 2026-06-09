/*
 * leap_config.h — LEAP transport and profile defaults for D945GSEJT port.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_CONFIG_H
#define LEAP_RTEMS_CONFIG_H

#include <stdint.h>

#include "leap/leap_protocol.h"

/* Onboard RTL8111D/DL via libbsd re(4) → re0. */
#ifndef LEAP_RTEMS_IFNAME
#define LEAP_RTEMS_IFNAME "re0"
#endif

/* LEAP development EtherType (matches leap_protocol.h). */
#define LEAP_RTEMS_ETHERTYPE LEAP_ETHERTYPE_DEVELOPMENT

/* Initial digital profile for lab bring-up. */
#define LEAP_RTEMS_PROFILE_ID LEAP_PROFILE_DIGITAL_IO_8X8
#define LEAP_RTEMS_DIGITAL_OUTPUTS 8u
#define LEAP_RTEMS_DIGITAL_INPUTS  8u

/* RX buffer and poll timeout. */
#define LEAP_RTEMS_RX_BUF_SIZE   1600u
#define LEAP_RTEMS_RECV_TIMEOUT_MS 5
#define LEAP_RTEMS_TICK_PERIOD_US 500u

/* Max serialized LEAP frame (header + payload). */
#define LEAP_MAX_FRAME_BYTES (LEAP_HEADER_LENGTH_V1 + LEAP_MAX_PAYLOAD_V1)

/*
 * ANSI color for the serial boot log. Set to 0 for plain text if the terminal
 * does not interpret escape sequences.
 */
#ifndef LEAP_RTEMS_ANSI_COLOR
#define LEAP_RTEMS_ANSI_COLOR 1
#endif

#if LEAP_RTEMS_ANSI_COLOR
#define LEAP_ANSI_RESET  "\x1b[0m"
#define LEAP_ANSI_BANNER "\x1b[1;36m" /* bold cyan */
#define LEAP_ANSI_OK     "\x1b[1;32m" /* bold green */
#define LEAP_ANSI_WARN   "\x1b[1;33m" /* bold yellow */
#define LEAP_ANSI_ERR    "\x1b[1;31m" /* bold red */
#define LEAP_ANSI_INFO   "\x1b[36m"   /* cyan */
#define LEAP_ANSI_DIM    "\x1b[2m"    /* dim */
#else
#define LEAP_ANSI_RESET  ""
#define LEAP_ANSI_BANNER ""
#define LEAP_ANSI_OK     ""
#define LEAP_ANSI_WARN   ""
#define LEAP_ANSI_ERR    ""
#define LEAP_ANSI_INFO   ""
#define LEAP_ANSI_DIM    ""
#endif

/*
 * Dim uptime prefix for log lines. Pair with leap_rtems_uptime_str():
 *   printf(LEAP_TS_FMT LEAP_ANSI_OK "msg" LEAP_ANSI_RESET "\n",
 *          leap_rtems_uptime_str());
 */
#define LEAP_TS_FMT LEAP_ANSI_DIM "[%s]" LEAP_ANSI_RESET " "

#endif /* LEAP_RTEMS_CONFIG_H */
