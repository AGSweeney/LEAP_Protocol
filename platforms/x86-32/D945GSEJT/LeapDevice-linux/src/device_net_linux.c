/*
 * device_net_linux.c — Bring NIC up for raw L2 (no IPv4 required).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "device_net.h"

#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

int
device_net_bring_up(const char* ifname)
{
    struct ifreq ifr;
    int          s;

    if (ifname == NULL || ifname[0] == '\0')
    {
        return -1;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    (void)snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);

    if (ioctl(s, SIOCGIFFLAGS, &ifr) != 0)
    {
        close(s);
        return -1;
    }

    ifr.ifr_flags = (short)(ifr.ifr_flags | (short)IFF_UP);

    if (ioctl(s, SIOCSIFFLAGS, &ifr) != 0)
    {
        close(s);
        return -1;
    }

    close(s);
    return 0;
}

int
device_net_wait_for_iface(const char* ifname, int timeout_s)
{
    char path[64];
    int  waited;

    if (ifname == NULL || ifname[0] == '\0')
    {
        return -1;
    }

    (void)snprintf(path, sizeof(path), "/sys/class/net/%s", ifname);

    for (waited = 0; waited < timeout_s; ++waited)
    {
        if (access(path, F_OK) == 0)
        {
            return 0;
        }

        sleep(1);
    }

    return -1;
}
