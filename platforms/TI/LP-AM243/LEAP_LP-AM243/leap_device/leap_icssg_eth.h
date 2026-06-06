/*
 * Raw LEAP Ethernet transport over ICSSG DMA.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_ICSSG_ETH_H
#define LEAP_ICSSG_ETH_H

#include <stddef.h>
#include <stdint.h>

struct EnetMp_PerCtxt_s;
struct EnetMp_Obj_s;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EnetMp_PerCtxt_s EnetMp_PerCtxt;
typedef struct EnetMp_Obj_s     EnetMp_Obj;

#define LEAP_ICSSG_ETH_MAX_FRAME 1600u

int  leap_icssg_eth_bind(EnetMp_PerCtxt *perCtxt, EnetMp_Obj *enetMp);
int  leap_icssg_eth_send(const uint8_t *dst_mac, const uint8_t *payload,
                         size_t payload_length);
int  leap_icssg_eth_send_on(EnetMp_PerCtxt *perCtxt, const uint8_t *dst_mac,
                            const uint8_t *payload, size_t payload_length);
int  leap_icssg_eth_accept_rx_frame(EnetMp_PerCtxt *perCtxt,
                                    const uint8_t *frame_bytes,
                                    uint32_t frame_len);
void leap_icssg_eth_poll_tx(void);
const uint8_t *leap_icssg_eth_mac(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_ICSSG_ETH_H */
