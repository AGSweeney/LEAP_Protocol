/*
 * LEAP device host for LP-AM243 (ICSSG raw Ethernet).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_AM243_HOST_H
#define LEAP_AM243_HOST_H

#include <stddef.h>
#include <stdint.h>

struct EnetMp_PerCtxt_s;

#define LEAP_HOST_MAX_FRAME 1600u
#define LEAP_HOST_RX_DEPTH  16u

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

int  leap_host_init(const uint8_t mac[6]);
void leap_host_bind_task_handle(void *task_handle);
void leap_host_cyclic(void);
int  leap_host_rx_pending(void);
int  leap_host_queue_frame(struct EnetMp_PerCtxt_s *port,
                           const uint8_t *src_mac,
                           const uint8_t *payload,
                           size_t payload_length);
/*
 * Hot path: handle PD EXCHANGE in the Enet RX task when the stack mutex is
 * free. Returns 1 if consumed (no queue), 0 to fall through to queue_frame.
 */
int  leap_host_try_inline_pd_exchange(struct EnetMp_PerCtxt_s *port,
                                      const uint8_t *src_mac,
                                      const uint8_t *payload,
                                      size_t payload_length);
const LeapHostStats *leap_host_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_AM243_HOST_H */
