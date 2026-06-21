/*
 * leap_config_mcc_dio24.h — LEAP defaults for MCC PCI-DIO-24H on D945GSEJT.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_CONFIG_H
#define LEAP_RTEMS_CONFIG_H

#include <stdint.h>

#include "leap/leap_protocol.h"

#ifndef LEAP_RTEMS_IFNAME
#define LEAP_RTEMS_IFNAME "eth0"
#endif

#ifndef LEAP_RTEMS_IFNAME_AUTO
#define LEAP_RTEMS_IFNAME_AUTO 1
#endif

#define LEAP_RTEMS_ETHERTYPE LEAP_ETHERTYPE_DEVELOPMENT

#define LEAP_RTEMS_PROFILE_ID      LEAP_PROFILE_DIGITAL_IO_16X16
#define LEAP_RTEMS_DIGITAL_OUTPUTS 16u
#define LEAP_RTEMS_DIGITAL_INPUTS  16u

#define LEAP_RTEMS_RX_BUF_SIZE       1600u
#define LEAP_RTEMS_RECV_TIMEOUT_MS   5
#define LEAP_RTEMS_TICK_PERIOD_US    500u

#define LEAP_MAX_FRAME_BYTES (LEAP_HEADER_LENGTH_V1 + LEAP_MAX_PAYLOAD_V1)

#ifndef LEAP_RTEMS_ANSI_COLOR
#define LEAP_RTEMS_ANSI_COLOR 1
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

#define LEAP_TS_FMT LEAP_ANSI_DIM "[%s]" LEAP_ANSI_RESET " "

#endif /* LEAP_RTEMS_CONFIG_H */
