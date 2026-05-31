/*
 * leap_raw_linux.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE

#include "leap/leap_raw_linux.h"

#if defined(__linux__)

#include "leap/leap_protocol.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <linux/if_ether.h>
#include <linux/if_packet.h>

static int g_leap_raw_linux_last_errno;

static void leap_raw_linux_set_errno(void)
{
    g_leap_raw_linux_last_errno = errno;
}

static void leap_raw_linux_clear_errno(void)
{
    g_leap_raw_linux_last_errno = 0;
}

int leap_raw_linux_last_errno(void)
{
    return g_leap_raw_linux_last_errno;
}

uint64_t leap_raw_linux_monotonic_us(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        leap_raw_linux_set_errno();
        return 0u;
    }

    return ((uint64_t)ts.tv_sec * 1000000u) + ((uint64_t)ts.tv_nsec / 1000u);
}

static int leap_raw_linux_ifindex(int fd, const char* ifname, uint8_t* mac_out)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    (void)snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        leap_raw_linux_set_errno();
        return -1;
    }

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
    {
        leap_raw_linux_set_errno();
        return -1;
    }

    if (mac_out != NULL)
    {
        (void)memcpy(mac_out, ifr.ifr_hwaddr.sa_data, LEAP_RAW_LINUX_MAC_LEN);
    }

    return ifr.ifr_ifindex;
}

static int leap_raw_linux_set_promiscuous(int fd, const char* ifname, int enable)
{
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    (void)snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
    {
        leap_raw_linux_set_errno();
        return -1;
    }

    if (enable != 0)
    {
        ifr.ifr_flags = (short)(ifr.ifr_flags | (short)IFF_PROMISC);
    }
    else
    {
        ifr.ifr_flags = (short)(ifr.ifr_flags & (short)(~IFF_PROMISC));
    }

    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0)
    {
        leap_raw_linux_set_errno();
        return -1;
    }

    return 0;
}

static int leap_raw_linux_mac_is_broadcast(const uint8_t* mac)
{
    static const uint8_t k_bcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

    return (memcmp(mac, k_bcast, 6) == 0);
}

static int leap_raw_linux_mac_is_multicast(const uint8_t* mac)
{
    return (mac != NULL && (mac[0] & 0x01u) != 0u);
}

static int leap_raw_linux_frame_dest_ok(
    const LeapRawLinuxSocket* sock,
    const uint8_t*            frame_dst_mac)
{
    if (sock == NULL || frame_dst_mac == NULL)
    {
        return 0;
    }

    if (sock->filter_dest_mac == 0)
    {
        return 1;
    }

    if (memcmp(frame_dst_mac, sock->local_mac, LEAP_RAW_LINUX_MAC_LEN) == 0)
    {
        return 1;
    }

    if (leap_raw_linux_mac_is_broadcast(frame_dst_mac))
    {
        return 1;
    }

    if (leap_raw_linux_mac_is_multicast(frame_dst_mac))
    {
        return 1;
    }

    return 0;
}

static ssize_t leap_raw_linux_send_all(LeapRawLinuxSocket* sock, const uint8_t* data, size_t length)
{
    size_t   offset = 0u;
    ssize_t  sent;
    int      chunks = 0;

    while (offset < length)
    {
        sent = send(sock->fd, data + offset, length - offset, 0);
        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            sock->stats.tx_errors++;
            leap_raw_linux_set_errno();
            return -1;
        }

        if (sent == 0)
        {
            sock->stats.tx_errors++;
            errno = EIO;
            leap_raw_linux_set_errno();
            return -1;
        }

        chunks++;
        if (chunks > 1)
        {
            sock->stats.tx_partial_chunks++;
        }

        offset += (size_t)sent;
    }

    sock->stats.tx_frames_ok++;
    sock->stats.tx_bytes += length;

    return (ssize_t)length;
}

void leap_raw_linux_get_stats(
    const LeapRawLinuxSocket* sock,
    LeapRawLinuxStats*        out)
{
    if (sock == NULL || out == NULL)
    {
        return;
    }

    *out = sock->stats;
}

void leap_raw_linux_reset_stats(LeapRawLinuxSocket* sock)
{
    if (sock == NULL)
    {
        return;
    }

    memset(&sock->stats, 0, sizeof(sock->stats));
}

int leap_raw_linux_open_ex(
    LeapRawLinuxSocket*             sock,
    const char*                     ifname,
    uint16_t                        ethertype,
    const LeapRawLinuxOpenOptions* options)
{
    struct sockaddr_ll addr;
    int                ifindex;
    int                promiscuous = 0;
    int                filter_dest = 1;

    if (sock == NULL || ifname == NULL)
    {
        return -1;
    }

    if (options != NULL)
    {
        promiscuous = options->promiscuous;
        filter_dest = options->filter_dest_mac;
    }

    leap_raw_linux_clear_errno();

    memset(sock, 0, sizeof(*sock));
    sock->fd               = -1;
    sock->filter_dest_mac  = (filter_dest != 0) ? 1 : 0;
    sock->cached_link_up   = -1;

    sock->fd = socket(AF_PACKET, SOCK_RAW, htons(ethertype));
    if (sock->fd < 0)
    {
        leap_raw_linux_set_errno();
        return -1;
    }

    ifindex = leap_raw_linux_ifindex(sock->fd, ifname, sock->local_mac);
    if (ifindex < 0)
    {
        close(sock->fd);
        sock->fd = -1;
        return -1;
    }

    if (promiscuous != 0)
    {
        if (leap_raw_linux_set_promiscuous(sock->fd, ifname, 1) != 0)
        {
            close(sock->fd);
            sock->fd = -1;
            return -1;
        }

        sock->promiscuous = 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sll_family   = AF_PACKET;
    addr.sll_protocol = htons(ethertype);
    addr.sll_ifindex  = ifindex;

    if (bind(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        if (errno == ENODEV)
        {
            /*
             * WSL2 and some virtualized hosts reject AF_PACKET bind even when
             * the interface name resolves. Preserve ENODEV for callers.
             */
        }

        leap_raw_linux_set_errno();
        if (sock->promiscuous != 0)
        {
            (void)leap_raw_linux_set_promiscuous(sock->fd, ifname, 0);
        }
        close(sock->fd);
        sock->fd = -1;
        return -1;
    }

    sock->ethertype = ethertype;
    (void)snprintf(sock->ifname, sizeof(sock->ifname), "%s", ifname);
    return 0;
}

int leap_raw_linux_query_link(
    const LeapRawLinuxSocket* sock,
    LeapRawLinuxLinkState*    state_out)
{
    struct ifreq ifr;
    short        flags;

    if (sock == NULL || state_out == NULL || sock->fd < 0 ||
        sock->ifname[0] == '\0')
    {
        return -1;
    }

    memset(state_out, 0, sizeof(*state_out));

    memset(&ifr, 0, sizeof(ifr));
    (void)snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", sock->ifname);

    if (ioctl(sock->fd, SIOCGIFFLAGS, &ifr) < 0)
    {
        leap_raw_linux_set_errno();
        return -1;
    }

    flags = ifr.ifr_flags;
    state_out->interface_up = ((flags & IFF_UP) != 0) ? 1 : 0;
    state_out->carrier_up   = ((flags & IFF_RUNNING) != 0) ? 1 : 0;
    state_out->link_up =
        (state_out->interface_up != 0 && state_out->carrier_up != 0) ? 1 : 0;

    leap_raw_linux_clear_errno();
    return 0;
}

int leap_raw_linux_poll_link(
    LeapRawLinuxSocket*    sock,
    int*                   changed_out,
    LeapRawLinuxLinkState* state_out)
{
    LeapRawLinuxLinkState state;
    int                   prev;
    int                   changed = 0;

    if (sock == NULL)
    {
        return -1;
    }

    if (changed_out != NULL)
    {
        *changed_out = 0;
    }

    if (leap_raw_linux_query_link(sock, &state) != 0)
    {
        return -1;
    }

    if (state_out != NULL)
    {
        *state_out = state;
    }

    prev = sock->cached_link_up;
    if (prev >= 0 && state.link_up != prev)
    {
        changed = 1;
        sock->stats.link_transitions++;
    }

    sock->cached_link_up = state.link_up;

    if (changed_out != NULL)
    {
        *changed_out = changed;
    }

    return 0;
}

int leap_raw_linux_open(LeapRawLinuxSocket* sock, const char* ifname, uint16_t ethertype)
{
    return leap_raw_linux_open_ex(sock, ifname, ethertype, NULL);
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
    LeapRawLinuxSocket* sock,
    const uint8_t*      dst_mac,
    const uint8_t*      payload,
    size_t              payload_length)
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
    /* Ethernet II type field is big-endian on the wire. */
    frame[12] = (uint8_t)((sock->ethertype >> 8) & 0xFFu);
    frame[13] = (uint8_t)(sock->ethertype & 0xFFu);
    memcpy(frame + 14, payload, payload_length);

    sent = leap_raw_linux_send_all(sock, frame, total);
    if (sent != (ssize_t)total)
    {
        return -1;
    }

    leap_raw_linux_clear_errno();
    return 0;
}

int leap_raw_linux_recv(
    LeapRawLinuxSocket* sock,
    uint8_t*            src_mac,
    uint8_t*            payload,
    size_t              payload_capacity,
    size_t*             payload_length,
    int                 timeout_ms)
{
    uint8_t       frame[LEAP_MAX_ETHERNET_PAYLOAD + 14u];
    struct pollfd pfd;
    ssize_t       received;
    size_t        leap_len;
    LeapRawLinuxSocket* mutable_sock = sock;

    if (sock == NULL || sock->fd < 0 || payload == NULL || payload_length == NULL)
    {
        return -1;
    }

    *payload_length = 0u;

    for (;;)
    {
        for (;;)
        {
            pfd.fd     = sock->fd;
            pfd.events = POLLIN;

            {
                int poll_result = poll(&pfd, 1, timeout_ms);

                if (poll_result == 0)
                {
                    mutable_sock->stats.rx_timeouts++;
                    leap_raw_linux_clear_errno();
                    return -1;
                }

                if (poll_result < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }

                    mutable_sock->stats.rx_errors++;
                    leap_raw_linux_set_errno();
                    return -1;
                }
            }

            received = recv(sock->fd, frame, sizeof(frame), 0);
            if (received < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                mutable_sock->stats.rx_errors++;
                leap_raw_linux_set_errno();
                return -1;
            }

            break;
        }

        if (received < 14)
        {
            mutable_sock->stats.rx_short_frames++;
            continue;
        }

        if (!leap_raw_linux_frame_dest_ok(sock, frame))
        {
            mutable_sock->stats.rx_filtered++;
            continue;
        }

        break;
    }

    if (src_mac != NULL)
    {
        memcpy(src_mac, frame + 6, 6);
    }

    leap_len = (size_t)received - 14u;
    if (leap_len > payload_capacity)
    {
        mutable_sock->stats.rx_errors++;
        errno = EMSGSIZE;
        leap_raw_linux_set_errno();
        return -1;
    }

    memcpy(payload, frame + 14, leap_len);
    *payload_length = leap_len;
    mutable_sock->stats.rx_frames_ok++;
    mutable_sock->stats.rx_bytes += leap_len;
    leap_raw_linux_clear_errno();
    return 0;
}

#else

static int g_leap_raw_linux_last_errno;

int leap_raw_linux_last_errno(void)
{
    return g_leap_raw_linux_last_errno;
}

uint64_t leap_raw_linux_monotonic_us(void)
{
    return 0u;
}

int leap_raw_linux_open_ex(
    LeapRawLinuxSocket*             sock,
    const char*                     ifname,
    uint16_t                        ethertype,
    const LeapRawLinuxOpenOptions* options)
{
    (void)sock;
    (void)ifname;
    (void)ethertype;
    (void)options;
    return -1;
}

int leap_raw_linux_open(LeapRawLinuxSocket* sock, const char* ifname, uint16_t ethertype)
{
    return leap_raw_linux_open_ex(sock, ifname, ethertype, NULL);
}

void leap_raw_linux_close(LeapRawLinuxSocket* sock)
{
    (void)sock;
}

int leap_raw_linux_send(
    LeapRawLinuxSocket* sock,
    const uint8_t*      dst_mac,
    const uint8_t*      payload,
    size_t              payload_length)
{
    (void)sock;
    (void)dst_mac;
    (void)payload;
    (void)payload_length;
    return -1;
}

void leap_raw_linux_get_stats(
    const LeapRawLinuxSocket* sock,
    LeapRawLinuxStats*        out)
{
    (void)sock;
    if (out != NULL)
    {
        memset(out, 0, sizeof(*out));
    }
}

void leap_raw_linux_reset_stats(LeapRawLinuxSocket* sock)
{
    (void)sock;
}

int leap_raw_linux_recv(
    LeapRawLinuxSocket* sock,
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

int leap_raw_linux_query_link(
    const LeapRawLinuxSocket* sock,
    LeapRawLinuxLinkState*    state_out)
{
    (void)sock;
    if (state_out != NULL)
    {
        memset(state_out, 0, sizeof(*state_out));
    }
    return -1;
}

int leap_raw_linux_poll_link(
    LeapRawLinuxSocket*    sock,
    int*                   changed_out,
    LeapRawLinuxLinkState* state_out)
{
    (void)sock;
    if (changed_out != NULL)
    {
        *changed_out = 0;
    }
    if (state_out != NULL)
    {
        memset(state_out, 0, sizeof(*state_out));
    }
    return -1;
}

#endif /* __linux__ */
