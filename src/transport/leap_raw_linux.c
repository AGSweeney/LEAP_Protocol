/*
 * leap_raw_linux.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_raw_linux.h"

#if defined(__linux__)

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>

static int leap_raw_linux_ifindex(int fd, const char* ifname, uint8_t* mac_out)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    (void)snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        return -1;
    }

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        return -1;
    }

    if (mac_out != NULL)
    {
        (void)memcpy(mac_out, ifr.ifr_hwaddr.sa_data, LEAP_RAW_LINUX_MAC_LEN);
    }

    return ifr.ifr_ifindex;
}

int leap_raw_linux_open(LeapRawLinuxSocket* sock, const char* ifname, uint16_t ethertype)
{
    struct sockaddr_ll addr;
    int                ifindex;

    if (sock == NULL || ifname == NULL)
    {
        return -1;
    }

    memset(sock, 0, sizeof(*sock));
    sock->fd = socket(AF_PACKET, SOCK_RAW, htons(ethertype));
    if (sock->fd < 0)
    {
        return -1;
    }

    ifindex = leap_raw_linux_ifindex(sock->fd, ifname, sock->local_mac);
    if (ifindex < 0)
    {
        close(sock->fd);
        sock->fd = -1;
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(ethertype);
    addr.sll_ifindex  = ifindex;

    if (bind(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        close(sock->fd);
        sock->fd = -1;
        return -1;
    }

    sock->ethertype = ethertype;
    return 0;
}

void leap_raw_linux_close(LeapRawLinuxSocket* sock)
{
    if (sock == NULL || sock->fd < 0)
    {
        return;
    }

    close(sock->fd);
    sock->fd = -1;
}

int leap_raw_linux_send(
    const LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    const uint8_t*            payload,
    size_t                    payload_length)
{
    uint8_t  frame[LEAP_MAX_ETHERNET_PAYLOAD + 14u];
    size_t   total;
    ssize_t  sent;

    if (sock == NULL || sock->fd < 0 || dst_mac == NULL || payload == NULL)
    {
        return -1;
    }

    if (payload_length > LEAP_MAX_ETHERNET_PAYLOAD)
    {
        return -1;
    }

    total = 14u + payload_length;
    if (total > sizeof(frame))
    {
        return -1;
    }

    memcpy(frame, dst_mac, 6);
    memcpy(frame + 6, sock->local_mac, 6);
    frame[12] = (uint8_t)(sock->ethertype & 0xFFu);
    frame[13] = (uint8_t)((sock->ethertype >> 8) & 0xFFu);
    memcpy(frame + 14, payload, payload_length);

    sent = send(sock->fd, frame, total, 0);
    return (sent == (ssize_t)total) ? 0 : -1;
}

int leap_raw_linux_recv(
    const LeapRawLinuxSocket* sock,
    uint8_t*                  src_mac,
    uint8_t*                  payload,
    size_t                    payload_capacity,
    size_t*                   payload_length,
    int                       timeout_ms)
{
    uint8_t     frame[LEAP_MAX_ETHERNET_PAYLOAD + 14u];
    struct pollfd pfd;
    ssize_t     received;
    size_t      leap_len;

    if (sock == NULL || sock->fd < 0 || payload == NULL || payload_length == NULL)
    {
        return -1;
    }

    *payload_length = 0u;

    pfd.fd     = sock->fd;
    pfd.events = POLLIN;

    if (poll(&pfd, 1, timeout_ms) <= 0)
    {
        return -1;
    }

    received = recv(sock->fd, frame, sizeof(frame), 0);
    if (received < 14)
    {
        return -1;
    }

    if (src_mac != NULL)
    {
        memcpy(src_mac, frame + 6, 6);
    }

    leap_len = (size_t)received - 14u;
    if (leap_len > payload_capacity)
    {
        return -1;
    }

    memcpy(payload, frame + 14, leap_len);
    *payload_length = leap_len;
    return 0;
}

#else

int leap_raw_linux_open(LeapRawLinuxSocket* sock, const char* ifname, uint16_t ethertype)
{
    (void)sock;
    (void)ifname;
    (void)ethertype;
    return -1;
}

void leap_raw_linux_close(LeapRawLinuxSocket* sock)
{
    (void)sock;
}

int leap_raw_linux_send(
    const LeapRawLinuxSocket* sock,
    const uint8_t*            dst_mac,
    const uint8_t*            payload,
    size_t                    payload_length)
{
    (void)sock;
    (void)dst_mac;
    (void)payload;
    (void)payload_length;
    return -1;
}

int leap_raw_linux_recv(
    const LeapRawLinuxSocket* sock,
    uint8_t*                  src_mac,
    uint8_t*                  payload,
    size_t                    payload_capacity,
    size_t*                   payload_length,
    int                       timeout_ms)
{
    (void)sock;
    (void)src_mac;
    (void)payload;
    (void)payload_capacity;
    (void)payload_length;
    (void)timeout_ms;
    return -1;
}

#endif /* __linux__ */
