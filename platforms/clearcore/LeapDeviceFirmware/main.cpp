/*

 * LeapDeviceFirmware -- ClearCore LEAP device host

 *

 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

 * SPDX-License-Identifier: MIT

 */



#include "ClearCore.h"

#include "lwip/ip4_addr.h"

#include "lwip/netif.h"

#include "protocol/leap_device/clearcore_leap_host.h"



#include <stdio.h>



int main(void) {

    ConnectorUsb.PortOpen();

    Delay_ms(100);



    ConnectorUsb.SendLine("\r\n========================================");

    ConnectorUsb.SendLine("  LEAP Device Host -- ClearCore");

    ConnectorUsb.SendLine("========================================\r\n");

    ConnectorUsb.SendLine("Waiting for Ethernet link...");

    ConnectorUsb.Flush();



    const uint32_t link_timeout_ms = 5000U;

    const uint32_t link_wait_start = Milliseconds();

    while (!EthernetMgr.PhyLinkActive()) {

        if (Milliseconds() - link_wait_start > link_timeout_ms) {

            ConnectorUsb.SendLine("ERROR: Ethernet link timeout.");

            while (true) {

                Delay_ms(1000);

            }

        }

        Delay_ms(100);

    }



    ConnectorUsb.SendLine("Ethernet link detected.");

    ConnectorUsb.Flush();



    EthernetMgr.Setup();

    Delay_ms(100);



    struct netif *netif = EthernetMgr.MacInterface();

    if (netif == nullptr) {

        ConnectorUsb.SendLine("ERROR: Failed to get netif pointer.");

        while (true) {

            Delay_ms(1000);

        }

    }



    if (clearcore_leap_host_init(netif) != 0) {

        ConnectorUsb.SendLine("ERROR: LEAP device init failed.");

        while (true) {

            Delay_ms(1000);

        }

    }



    char ip_message[48];

    if (netif->ip_addr.addr != 0U) {

        snprintf(ip_message, sizeof(ip_message), "IP Address: %d.%d.%d.%d",

                 ip4_addr1(&netif->ip_addr),

                 ip4_addr2(&netif->ip_addr),

                 ip4_addr3(&netif->ip_addr),

                 ip4_addr4(&netif->ip_addr));

    } else {

        snprintf(ip_message, sizeof(ip_message), "IP Address: not assigned");

    }

    ConnectorUsb.SendLine(ip_message);

    ConnectorUsb.SendLine("Entering LEAP raw-frame loop.");

    ConnectorUsb.Flush();



    uint32_t last_stats_log_ms = Milliseconds();

    bool prevLinkUp = EthernetMgr.PhyLinkActive();



    while (true) {

        EthernetMgr.Refresh();

        clearcore_leap_host_cyclic();



        const bool linkUp = EthernetMgr.PhyLinkActive();

        if (linkUp && !prevLinkUp) {

            ConnectorUsb.SendLine("NET: Link UP");

        } else if (!linkUp && prevLinkUp) {

            ConnectorUsb.SendLine("NET: Link DOWN");

        }

        prevLinkUp = linkUp;



        const uint32_t now_ms = Milliseconds();

        if (now_ms - last_stats_log_ms >= 1000U) {

            const ClearcoreLeapHostStats *stats = clearcore_leap_host_stats();

            char stats_message[96];

            snprintf(stats_message, sizeof(stats_message),

                     "LEAP stats: rx_ok=%lu rx_drop=%lu tx_ok=%lu tx_drop=%lu",

                     static_cast<unsigned long>(stats->rx_ok),

                     static_cast<unsigned long>(stats->rx_drop),

                     static_cast<unsigned long>(stats->tx_ok),

                     static_cast<unsigned long>(stats->tx_drop));

            ConnectorUsb.SendLine(stats_message);

            last_stats_log_ms = now_ms;

        }



        /* M1a: no millisecond sleep on idle — poll continuously for sub-ms RTT.
         * See docs/LEAP_DEVICE_PERFORMANCE.md (LEAP_DEVICE_PERF_M1_NO_MAIN_LOOP_SLEEP). */
    }

}


