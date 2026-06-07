// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include <stddef.h>
#include <stdint.h>

struct netif;

#define LEAP_HOST_MAX_FRAME 1600u
#define LEAP_HOST_RX_DEPTH  8u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeapHostStats
{
    uint32_t rx_queued;
    uint32_t rx_drop;
    uint32_t rx_ok;
    uint32_t tx_ok;
    uint32_t tx_drop;
} LeapHostStats;

int  leap_host_init(struct netif *netif);
void leap_host_bind_task_handle(void *task_handle);
void leap_host_cyclic(void);
int  leap_host_rx_pending(void);

int leap_host_queue_frame(struct netif *netif, const uint8_t *src_mac,
                          const uint8_t *payload, size_t payload_length);

const LeapHostStats *leap_host_stats(void);

#ifdef __cplusplus
}
#endif
