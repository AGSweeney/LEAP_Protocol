/*
 * gateway_net_linux.c — Interface bring-up (Linux replacement for gateway_net.c).
 *
 * Binds the LEAP AF_PACKET transport and assigns the static IPv4 used by the
 * Web UI / EtherNet/IP side. Linux equivalent of the RTEMS/libbsd SIOCAIFADDR
 * path: SIOCSIFADDR + SIOCSIFNETMASK + SIOCSIFFLAGS(IFF_UP).
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
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int
iface_set_ipv4(const char* ifname, const char* addr, const char* mask)
{
    struct ifreq        ifr;
    struct sockaddr_in* sin;
    int                 s;
    int                 rc = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    sin = (struct sockaddr_in*)&ifr.ifr_addr;
    sin->sin_family = AF_INET;
    if (inet_pton(AF_INET, addr, &sin->sin_addr) != 1 ||
        ioctl(s, SIOCSIFADDR, &ifr) != 0)
    {
        rc = -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    sin = (struct sockaddr_in*)&ifr.ifr_netmask;
    sin->sin_family = AF_INET;
    if (rc == 0 &&
        (inet_pton(AF_INET, mask, &sin->sin_addr) != 1 ||
         ioctl(s, SIOCSIFNETMASK, &ifr) != 0))
    {
        rc = -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
    {
        ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
        (void)ioctl(s, SIOCSIFFLAGS, &ifr);
    }

    close(s);
    return rc;
}

static int
try_transport(LeapGatewayRuntime* gw, const char* ifname)
{
    if (leap_rtems_transport_init(
            &gw->transport,
            ifname,
            LEAP_GATEWAY_ETHERTYPE) != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN "Gateway: open '%s' failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            ifname,
            strerror(leap_raw_linux_last_errno()));
        leap_rtems_transport_close(&gw->transport);
        return -1;
    }

    snprintf(gw->bound_ifname, sizeof(gw->bound_ifname), "%s", ifname);
    return 0;
}

static int
bind_leap_transport(LeapGatewayRuntime* gw)
{
    static const char* const candidates[] = {
        "eth0", "end0", "en0", "eth1", "end1", "enp1s0", "enp2s0", NULL
    };
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
            snprintf(
                gw->config.network.ifname,
                sizeof(gw->config.network.ifname),
                "%s",
                candidates[i]);
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

    eip_if = (gw->bound_ifname[0] != '\0') ? gw->bound_ifname : leap_gateway_eip_ifname(&gw->config);
    if (!gw->config.network.dhcp)
    {
        if (iface_set_ipv4(
                eip_if,
                gw->config.network.ipv4_addr,
                gw->config.network.ipv4_mask) != 0)
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
