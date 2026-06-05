// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t leap_lwip_unknown_eth_protocol_hook(struct pbuf *packet, struct netif *netif);

#ifdef __cplusplus
}
#endif

#define LWIP_HOOK_UNKNOWN_ETH_PROTOCOL(p, netif) \
    leap_lwip_unknown_eth_protocol_hook((p), (netif))
