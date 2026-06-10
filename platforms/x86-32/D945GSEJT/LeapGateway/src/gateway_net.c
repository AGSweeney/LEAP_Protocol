/*
 * gateway_net.c — Interface bring-up (single-NIC default).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_net.h"

#include "gateway_config.h"
#include "leap_time.h"
#include "leap_transport.h"

#include "leap/leap_gateway_config.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int
iface_set_ipv4(const char* ifname, const char* addr, const char* mask)
{
    struct in_aliasreq ifra;
    struct in_addr in_addr;
    struct in_addr in_mask;
    int s;
    int error;

    if (inet_pton(AF_INET, addr, &in_addr) != 1 ||
        inet_pton(AF_INET, mask, &in_mask) != 1)
    {
        return -1;
    }

    memset(&ifra, 0, sizeof(ifra));
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    ifra.ifra_addr.sin_len = sizeof(ifra.ifra_addr);
    ifra.ifra_addr.sin_family = AF_INET;
    ifra.ifra_addr.sin_addr = in_addr;
    ifra.ifra_mask.sin_len = sizeof(ifra.ifra_mask);
    ifra.ifra_mask.sin_family = AF_INET;
    ifra.ifra_mask.sin_addr = in_mask;
    ifra.ifra_broadaddr.sin_len = sizeof(ifra.ifra_broadaddr);
    ifra.ifra_broadaddr.sin_family = AF_INET;
    ifra.ifra_broadaddr.sin_addr.s_addr =
        in_addr.s_addr | ~in_mask.s_addr;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return -1;
    }

    error = ioctl(s, SIOCAIFADDR, &ifra);
    close(s);
    return error;
}

static int
iface_set_up(const char* ifname)
{
    struct ifreq  ifr;
    unsigned int  flags;
    int           s;

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return -1;
    }

    if (ioctl(s, SIOCGIFFLAGS, &ifr) != 0)
    {
        close(s);
        return -1;
    }

    flags = (unsigned int)(ifr.ifr_flags & 0xffffU) |
            ((unsigned int)ifr.ifr_flagshigh << 16);
    flags |= IFF_UP;

    ifr.ifr_flags    = (short)(flags & 0xffffU);
    ifr.ifr_flagshigh = (short)((flags >> 16) & 0xffffU);
    if (ioctl(s, SIOCSIFFLAGS, &ifr) != 0)
    {
        close(s);
        return -1;
    }

    close(s);
    return 0;
}

static int
try_transport(LeapGatewayRuntime* gw, const char* ifname)
{
    if (leap_rtems_transport_init(
            &gw->transport,
            ifname,
            LEAP_GATEWAY_ETHERTYPE) != 0)
    {
        leap_rtems_transport_close(&gw->transport);
        return -1;
    }

    strlcpy(gw->bound_ifname, ifname, sizeof(gw->bound_ifname));
    return 0;
}

static int
bind_leap_transport(LeapGatewayRuntime* gw)
{
    static const char* const candidates[] = { "re0", "em0", "em1", NULL };
    size_t i;

    if (!gw->config.network.auto_ifname &&
        gw->config.network.ifname[0] != '\0')
    {
        return try_transport(gw, leap_gateway_leap_ifname(&gw->config));
    }

    for (i = 0; candidates[i] != NULL; ++i)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_INFO "Gateway: trying %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            candidates[i]);
        if (try_transport(gw, candidates[i]) == 0)
        {
            strlcpy(
                gw->config.network.ifname,
                candidates[i],
                sizeof(gw->config.network.ifname));
            return 0;
        }
    }

    return -1;
}

int
leap_gateway_net_bring_up(LeapGatewayRuntime* gw)
{
    const char* eip_if;

    if (gw == NULL)
    {
        return -1;
    }

    if (bind_leap_transport(gw) != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: no usable network interface" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
        return -1;
    }

    eip_if = leap_gateway_eip_ifname(&gw->config);
    if (iface_set_up(eip_if) != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN "Gateway: failed to set %s UP (%s)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            eip_if,
            strerror(errno));
    }

    if (!gw->config.network.dhcp)
    {
        int ipv4_ok = -1;
        int attempt;

        for (attempt = 0; attempt < 5; ++attempt)
        {
            if (iface_set_ipv4(
                    eip_if,
                    gw->config.network.ipv4_addr,
                    gw->config.network.ipv4_mask) == 0)
            {
                ipv4_ok = 0;
                break;
            }
            sleep(1);
        }

        if (ipv4_ok != 0)
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_WARN "Gateway: IPv4 assign failed on %s: %s" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                eip_if,
                strerror(errno));
        }
        else
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_OK "Gateway: %s IPv4 %s (Web UI :%u / :%u)" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                eip_if,
                gw->config.network.ipv4_addr,
                (unsigned)LEAP_GATEWAY_HTTP_PORT,
                (unsigned)LEAP_GATEWAY_HTTP_PORT_ALT);
        }
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: DHCP requested but not implemented — set network.dhcp=0" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

    leap_eip_bridge_set_config(&gw->bridge, &gw->config.bridge);
    return 0;
}
