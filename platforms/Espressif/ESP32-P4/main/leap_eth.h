// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include <stddef.h>
#include <stdint.h>

struct netif;

#ifdef __cplusplus
extern "C" {
#endif

int leap_eth_send(struct netif *netif, const uint8_t *dst_mac,
                  const uint8_t *payload, size_t payload_length);

#ifdef __cplusplus
}
#endif
