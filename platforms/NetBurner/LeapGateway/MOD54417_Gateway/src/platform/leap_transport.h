/*
 * leap_transport.h - NetBurner NNDK raw L2 transport (LeapRtemsTransport API).
 *
 * Drop-in replacement for LeapPort/leap_transport.h so shared LeapGateway
 * sources compile unchanged on NetBurner.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_TRANSPORT_H
#define LEAP_RTEMS_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEAP_RTEMS_MAC_LEN 6u

typedef struct LeapRtemsTransport
{
    int      link_up;
    int      interface_number;
    uint16_t ethertype;
    uint8_t  local_mac[LEAP_RTEMS_MAC_LEN];
    char     ifname[16];
} LeapRtemsTransport;

typedef struct LeapRtemsTransportStats
{
    uint32_t rx_callbacks;
    uint32_t rx_matches;
    uint32_t rx_nonmatches;
    uint32_t rx_wrong_interface;
    uint32_t rx_drops;
    uint32_t tx_frames;
    uint32_t tx_failures;
    int32_t  bound_interface;
    int32_t  last_rx_interface;
    int32_t  last_wrong_interface;
    uint16_t last_rx_ethertype;
    uint16_t last_nonmatch_ethertype;
} LeapRtemsTransportStats;

int  leap_rtems_transport_init(LeapRtemsTransport* transport, const char* ifname, uint16_t ethertype);
int  leap_rtems_transport_init_auto(LeapRtemsTransport* transport, uint16_t ethertype);
void leap_rtems_transport_close(LeapRtemsTransport* transport);

int leap_rtems_transport_recv(
    LeapRtemsTransport* transport,
    uint8_t*            src_mac_out,
    uint8_t*            payload_out,
    size_t              payload_capacity,
    size_t*             payload_len_out,
    int                 timeout_ms);

int leap_rtems_transport_send_leap(
    LeapRtemsTransport*  transport,
    const uint8_t*       dst_mac,
    uint8_t              flags,
    uint16_t             service_id,
    uint16_t             message_type,
    uint32_t             session_id,
    uint32_t             sequence,
    uint32_t             ack_sequence,
    const uint8_t*       payload,
    size_t               payload_length);

int leap_rtems_transport_poll_link(LeapRtemsTransport* transport);
void leap_rtems_transport_get_stats(LeapRtemsTransportStats* stats_out);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_RTEMS_TRANSPORT_H */
