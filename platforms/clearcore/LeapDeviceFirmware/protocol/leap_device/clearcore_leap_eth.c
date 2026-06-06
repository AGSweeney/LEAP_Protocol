/*
 * clearcore_leap_eth.c
 *
 * lwIP unknown-EtherType hook and LEAP frame egress for ClearCore.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "clearcore_leap_eth.h"
#include "clearcore_leap_host.h"

#include "leap/leap_protocol.h"

#include "lwip/def.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "netif/ethernet.h"

#include <string.h>

#define LEAP_ETHERTYPE_ALT_HOST LEAP_ETHERTYPE_EXPERIMENTAL_ALT
#define LEAP_ETHERTYPE_DEV_HOST LEAP_ETHERTYPE_DEVELOPMENT

static int clearcore_leap_ethertype_match(uint16_t net_type)
{
    return (net_type == PP_HTONS(LEAP_ETHERTYPE_DEV_HOST) ||
            net_type == PP_HTONS(LEAP_ETHERTYPE_ALT_HOST)) ? 1 : 0;
}

int clearcore_leap_eth_init(void)
{
    return 0;
}

int clearcore_leap_eth_send(
    struct netif *  netif,
    const uint8_t * dst_mac,
    const uint8_t * payload,
    size_t          payload_length)
{
    struct pbuf *   packet;
    struct eth_addr source_mac;
    struct eth_addr destination_mac;
    err_t           copy_result;
    err_t           send_result;

    if (netif == NULL || dst_mac == NULL || payload == NULL || payload_length == 0u)
    {
        return -1;
    }

    /*
     * Per-send pbuf alloc/free: lwIP may free the pbuf inside ethernet_output.
     * Reusing pooled pbufs broke DISC HELLO_REPLY (discovery peers=0).
     * See docs/LEAP_DEVICE_PERFORMANCE.md M1d — pool needs driver ownership rules.
     */
    packet = pbuf_alloc(PBUF_LINK, (u16_t)payload_length, PBUF_RAM);
    if (packet == NULL)
    {
        return -1;
    }

    copy_result = pbuf_take(packet, payload, (u16_t)payload_length);
    if (copy_result != ERR_OK)
    {
        pbuf_free(packet);
        return -1;
    }

    memcpy(source_mac.addr, netif->hwaddr, ETH_HWADDR_LEN);
    memcpy(destination_mac.addr, dst_mac, ETH_HWADDR_LEN);

    send_result = ethernet_output(
        netif,
        packet,
        &source_mac,
        &destination_mac,
        (u16_t)LEAP_ETHERTYPE_DEV_HOST);
    pbuf_free(packet);

    return (send_result == ERR_OK) ? 0 : -1;
}

err_t LeapDevice_LwipUnknownEthProtocolHook(struct pbuf *packet, struct netif *netif)
{
    struct eth_hdr header;
    u16_t          copied;
    u16_t          payload_length;
    uint8_t        payload[CLEARCORE_LEAP_HOST_MAX_FRAME];
    u16_t          payload_copied;
    int            status;

    if (packet == NULL || netif == NULL)
    {
        return ERR_ARG;
    }

    copied = pbuf_copy_partial(packet, &header, SIZEOF_ETH_HDR, 0U);
    if (copied != SIZEOF_ETH_HDR)
    {
        return ERR_VAL;
    }

    if (clearcore_leap_ethertype_match(header.type) == 0)
    {
        return ERR_VAL;
    }

    if (packet->tot_len <= SIZEOF_ETH_HDR)
    {
        pbuf_free(packet);
        return ERR_OK;
    }

    payload_length = (u16_t)(packet->tot_len - SIZEOF_ETH_HDR);
    if (payload_length > CLEARCORE_LEAP_HOST_MAX_FRAME)
    {
        pbuf_free(packet);
        return ERR_VAL;
    }

    payload_copied = pbuf_copy_partial(packet, payload, payload_length, SIZEOF_ETH_HDR);
    if (payload_copied != payload_length)
    {
        pbuf_free(packet);
        return ERR_VAL;
    }

    status = clearcore_leap_host_queue_frame(
        netif,
        header.src.addr,
        payload,
        payload_length);
    pbuf_free(packet);
    return (status == 0) ? ERR_OK : ERR_VAL;
}
