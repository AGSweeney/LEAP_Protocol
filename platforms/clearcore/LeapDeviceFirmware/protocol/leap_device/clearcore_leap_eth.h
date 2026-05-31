/*
 * clearcore_leap_eth.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CLEARCORE_LEAP_ETH_H_
#define CLEARCORE_LEAP_ETH_H_

#include <stddef.h>
#include <stdint.h>

struct netif;
struct pbuf;

#ifdef __cplusplus
extern "C" {
#endif

int clearcore_leap_eth_send(
    struct netif *    netif,
    const uint8_t *   dst_mac,
    const uint8_t *   payload,
    size_t            payload_length);

#ifdef __cplusplus
}
#endif

#endif /* CLEARCORE_LEAP_ETH_H_ */
