/*
 * LEAP raw Ethernet TX on LwIP netif.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_STM746_ETH_H
#define LEAP_STM746_ETH_H

#include <stddef.h>
#include <stdint.h>

struct netif;

int leap_eth_send(struct netif *netif, const uint8_t *dst_mac,
                  const uint8_t *payload, size_t payload_length);

#endif /* LEAP_STM746_ETH_H */
