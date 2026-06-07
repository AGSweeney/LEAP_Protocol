// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "leap_eth.h"
#include "leap_host.h"

#include "leap/leap_protocol.h"

#include "lwip/def.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"

#include <string.h>

static uint8_t s_hook_rx_frame[LEAP_HOST_MAX_FRAME];

static int leap_ethertype_match(uint16_t net_type)
{
    return (net_type == PP_HTONS(LEAP_ETHERTYPE_DEVELOPMENT) ||
            net_type == PP_HTONS(LEAP_ETHERTYPE_EXPERIMENTAL_ALT)) ? 1 : 0;
}

int leap_eth_send(struct netif *netif, const uint8_t *dst_mac,
                  const uint8_t *payload, size_t payload_length)
{
    struct pbuf *packet;
    struct eth_addr source_mac;
    struct eth_addr destination_mac;
    err_t copy_result;
    err_t send_result;

    if (netif == NULL || dst_mac == NULL || payload == NULL || payload_length == 0u) {
        return -1;
    }

    packet = pbuf_alloc(PBUF_LINK, (u16_t)payload_length, PBUF_RAM);
    if (packet == NULL) {
        return -1;
    }

    copy_result = pbuf_take(packet, payload, (u16_t)payload_length);
    if (copy_result != ERR_OK) {
        pbuf_free(packet);
        return -1;
    }

    memcpy(source_mac.addr, netif->hwaddr, ETH_HWADDR_LEN);
    memcpy(destination_mac.addr, dst_mac, ETH_HWADDR_LEN);

    LOCK_TCPIP_CORE();
    send_result = ethernet_output(
        netif,
        packet,
        &source_mac,
        &destination_mac,
        (u16_t)LEAP_ETHERTYPE_DEVELOPMENT);
    UNLOCK_TCPIP_CORE();
    pbuf_free(packet);

    return (send_result == ERR_OK) ? 0 : -1;
}

err_t leap_lwip_unknown_eth_protocol_hook(struct pbuf *packet, struct netif *netif)
{
    struct eth_hdr header;
    u16_t copied;
    u16_t payload_length;
    u16_t payload_copied;
    int status;

    if (packet == NULL || netif == NULL) {
        return ERR_ARG;
    }

    copied = pbuf_copy_partial(packet, &header, SIZEOF_ETH_HDR, 0U);
    if (copied != SIZEOF_ETH_HDR) {
        return ERR_VAL;
    }

    if (leap_ethertype_match(header.type) == 0) {
        return ERR_VAL;
    }

    if (packet->tot_len <= SIZEOF_ETH_HDR) {
        pbuf_free(packet);
        return ERR_OK;
    }

    payload_length = (u16_t)(packet->tot_len - SIZEOF_ETH_HDR);
    if (payload_length > LEAP_HOST_MAX_FRAME) {
        return ERR_VAL;
    }

    payload_copied = pbuf_copy_partial(packet, s_hook_rx_frame, payload_length, SIZEOF_ETH_HDR);
    if (payload_copied != payload_length) {
        return ERR_VAL;
    }

    status = leap_host_queue_frame(netif, header.src.addr, s_hook_rx_frame, payload_length);
    if (status == 0) {
        pbuf_free(packet);
        return ERR_OK;
    }

    return ERR_VAL;
}
