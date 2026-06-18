/*
 * gateway_config.h - LeapOS-Gateway compile-time defaults (NetBurner embedded).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_RTEMS_CONFIG_H
#define LEAP_GATEWAY_RTEMS_CONFIG_H

#include <stdint.h>

#include "leap/leap_protocol.h"

#define LEAP_GATEWAY_ETHERTYPE LEAP_ETHERTYPE_DEVELOPMENT

#define LEAP_GATEWAY_RX_BUF_SIZE      1600u
#define LEAP_GATEWAY_RECV_TIMEOUT_MS  5
#define LEAP_GATEWAY_HTTP_PORT        80u
#define LEAP_GATEWAY_HTTP_PORT_ALT    8080u
#define LEAP_GATEWAY_DISCOVER_SCAN_MS 3000

#define LEAP_GATEWAY_CONFIG_PATH "nndk:appdata.leap_gateway_config"
#define LEAP_GATEWAY_STORAGE_MOUNT "NNDK config"

#ifndef LEAP_GATEWAY_OPENER_ENABLE
#define LEAP_GATEWAY_OPENER_ENABLE 1
#endif

#endif /* LEAP_GATEWAY_RTEMS_CONFIG_H */
