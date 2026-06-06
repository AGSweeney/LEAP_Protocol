/*

 * clearcore_leap_host.h

 *

 * LEAP device stack integration for ClearCore.

 *

 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

 * SPDX-License-Identifier: MIT

 */



#ifndef CLEARCORE_LEAP_HOST_H_

#define CLEARCORE_LEAP_HOST_H_



#include <stddef.h>

#include <stdint.h>

#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

struct netif;



#define CLEARCORE_LEAP_HOST_MAX_FRAME 1600u

#define CLEARCORE_LEAP_HOST_RX_DEPTH  32u

/* PD EXCHANGE fast-path TX (header + LEAP_PD_DEVICE_MAX_REPLY). */
#define CLEARCORE_LEAP_PD_TX_BUF_MAX \
    (LEAP_HEADER_LENGTH_V1 + LEAP_PD_DEVICE_MAX_REPLY + 8u)



#ifdef __cplusplus

extern "C" {

#endif



typedef struct ClearcoreLeapHostStats

{

    uint32_t rx_queued;

    uint32_t rx_drop;

    uint32_t rx_ok;

    uint32_t tx_ok;

    uint32_t tx_drop;

} ClearcoreLeapHostStats;



int  clearcore_leap_host_init(struct netif *netif);

void clearcore_leap_host_cyclic(void);

int  clearcore_leap_host_rx_pending(void);

int clearcore_leap_host_queue_frame(

    struct netif *  netif,

    const uint8_t * src_mac,

    const uint8_t * payload,

    size_t          payload_length);



const ClearcoreLeapHostStats *clearcore_leap_host_stats(void);



#ifdef __cplusplus

}

#endif



#endif /* CLEARCORE_LEAP_HOST_H_ */

