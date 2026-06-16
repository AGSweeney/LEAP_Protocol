/*
 * gateway_config.h — LeapOS-Gateway compile-time defaults.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_RTEMS_CONFIG_H
#define LEAP_GATEWAY_RTEMS_CONFIG_H

#include <stdint.h>

#include "leap/leap_protocol.h"

#define LEAP_GATEWAY_ETHERTYPE LEAP_ETHERTYPE_DEVELOPMENT

#define LEAP_GATEWAY_RX_BUF_SIZE      1600u
#define LEAP_GATEWAY_RECV_TIMEOUT_MS  5
#define LEAP_GATEWAY_HTTP_PORT        8080u
#define LEAP_GATEWAY_HTTP_PORT_ALT    80u
#define LEAP_GATEWAY_HTTP_STACK_SIZE  (32u * 1024u)
#define LEAP_GATEWAY_HTTP_PRIORITY    100u
#define LEAP_GATEWAY_DISCOVER_SCAN_MS 3000

#define LEAP_GATEWAY_CONFIG_PATH "/cf/config.txt"
#define LEAP_GATEWAY_STORAGE_MOUNT "/cf"

#ifndef LEAP_GATEWAY_OPENER_ENABLE
#define LEAP_GATEWAY_OPENER_ENABLE 1
#endif

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

#endif /* LEAP_GATEWAY_RTEMS_CONFIG_H */
