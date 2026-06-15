/*
 * LwIP hook for LEAP raw Ethernet frames.
 *
 * Included from lwipopts.h while opt.h is still being assembled — do not
 * pull in LwIP headers here (pbuf/netif need options defined first).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_STM746_LWIP_HOOKS_H
#define LEAP_STM746_LWIP_HOOKS_H

struct pbuf;
struct netif;

/* Matches LwIP err_t (signed char) without including LwIP headers here. */
signed char leap_lwip_unknown_eth_protocol_hook(struct pbuf *packet, struct netif *netif);

#define LWIP_HOOK_UNKNOWN_ETH_PROTOCOL(p, netif) \
    leap_lwip_unknown_eth_protocol_hook((p), (netif))

#endif /* LEAP_STM746_LWIP_HOOKS_H */
