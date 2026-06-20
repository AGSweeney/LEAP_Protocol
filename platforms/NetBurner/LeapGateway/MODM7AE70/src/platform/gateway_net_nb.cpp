/*
 * gateway_net_nb.cpp - Bind LEAP transport on the configured interface.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gateway_net.h"

#include "gateway_config.h"
#include "leap_time.h"
#include "leap_transport.h"

#include "leap/leap_gateway_config.h"

#include <cstdio>
#include <cstring>

#include <netinterface.h>

static void ensure_interface_active_for_l2(int ifn)
{
    InterfaceBlock* ifblock = GetInterfaceBlock(ifn);
    if (ifblock == nullptr)
    {
        return;
    }

#ifdef AUTOIP
    if (!static_cast<bool>(ifblock->ip4.autoip))
    {
        ifblock->ip4.autoip = true;
    }

    if (InterfaceLinkActive(ifn) &&
        InterfaceIP(ifn).IsNull() &&
        InterfaceAutoIP(ifn).IsNull())
    {
        ifblock->AutoClient.restart();
    }
#endif
}

static int try_transport(LeapGatewayRuntime* gw, const char* ifname)
{
    int ifn = 0;

    if (leap_rtems_transport_init(&gw->transport, ifname, LEAP_GATEWAY_ETHERTYPE) != 0)
    {
        printf("%s Gateway: open '%s' failed\r\n", leap_rtems_uptime_str(), ifname);
        leap_rtems_transport_close(&gw->transport);
        return -1;
    }

    ifn = gw->transport.interface_number;
    snprintf(gw->bound_ifname, sizeof(gw->bound_ifname), "%d", ifn);
    ensure_interface_active_for_l2(ifn);
    InterfaceBlock* ifblock = GetInterfaceBlock(ifn);
    printf(
        "%s Gateway: LEAP bind candidate %s resolved if=%d name=%s mode=%s link=%s speed=%d full=%s ip=%hI autoip=%hI\r\n",
        leap_rtems_uptime_str(),
        ifname != nullptr ? ifname : "",
        ifn,
        ifblock != nullptr ? ifblock->GetInterfaceName() : "?",
        ifblock != nullptr ? static_cast<NBString>(ifblock->ip4.mode).c_str() : "?",
        InterfaceLinkActive(ifn) ? "up" : "down",
        InterfaceLinkSpeed(ifn),
        InterfaceLinkDuplex(ifn) ? "yes" : "no",
        InterfaceIP(ifn),
        InterfaceAutoIP(ifn));
    return 0;
}

int leap_gateway_net_bring_up(LeapGatewayRuntime* gw)
{
    const char* leap_if;

    if (gw == nullptr)
    {
        return -1;
    }

    gw->config.network.mode = LEAP_GATEWAY_NIC_DUAL;
    strncpy(gw->config.network.leap_ifname, "2", sizeof(gw->config.network.leap_ifname) - 1u);
    strncpy(gw->config.network.eip_ifname, "1", sizeof(gw->config.network.eip_ifname) - 1u);

    leap_if = leap_gateway_leap_ifname(&gw->config);
    if (try_transport(gw, leap_if) != 0)
    {
        static const char* const candidates[] = { "2", "1", nullptr };
        size_t i;
        for (i = 0; candidates[i] != nullptr; ++i)
        {
            if (try_transport(gw, candidates[i]) == 0)
            {
                strncpy(
                    gw->config.network.leap_ifname,
                    candidates[i],
                    sizeof(gw->config.network.leap_ifname) - 1u);
                break;
            }
        }
        if (gw->bound_ifname[0] == '\0')
        {
            printf("%s Gateway: no usable LEAP network interface\r\n", leap_rtems_uptime_str());
            return -1;
        }
    }

    printf(
        "%s Gateway: LEAP transport on interface %s (%02x:%02x:%02x:%02x:%02x:%02x)\r\n",
        leap_rtems_uptime_str(),
        gw->bound_ifname,
        gw->transport.local_mac[0],
        gw->transport.local_mac[1],
        gw->transport.local_mac[2],
        gw->transport.local_mac[3],
        gw->transport.local_mac[4],
        gw->transport.local_mac[5]);

    leap_eip_bridge_set_config(&gw->bridge, &gw->config.bridge);
    return 0;
}
