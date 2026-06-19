/* Revision: 3.5.7 */



/******************************************************************************

* Copyright 1998-2024 NetBurner, Inc.  ALL RIGHTS RESERVED

******************************************************************************/



/*

 * LEAP Gateway (NetBurner MOD5441X) - embedded LeapOS-Gateway.

 *

 * SPDX-License-Identifier: MIT

 */



#include <init.h>

#include <nbrtos.h>

#include <iosys.h>

#include <netinterface.h>

#include <config_server.h>

#include <config_obj.h>

#include <http.h>

#include <fdprintf.h>

#include <system.h>

#include <buffers.h>

#include <hal.h>

#include <ip.h>

#include <taskmon.h>

#include <sim.h>

#include <ethervars.h>

#include <cstdio>

#include <cstring>
#include <cerrno>



extern "C" {

#include "../opener/netburner_port/opener.h"

}

#include "../opener/netburner_port/netburner_ifconfig.cpp"

#include "../opener/netburner_port/nb_nvtcpip.cpp"

#include "../opener/netburner_port/nb_reboot.cpp"

#include "../opener/netburner_port/opener_nb_socket.cpp"



extern "C" {

#include "gateway_config.h"

#include "gateway_global.h"

#include "gateway_leap_session.h"

#include "gateway_net.h"

#include "gateway_rtems_io.h"

#include "gateway_storage.h"

#include "leap_time.h"

#include "leap/leap_build_info.h"

#include "leap/leap_controller_session_hub.h"

#include "leap/leap_controller_stack.h"

#include "leap/leap_gateway_config.h"

#include "leap/leap_log.h"

#include "leap/leap_protocol.h"

}

extern "C" __attribute__((weak)) int _open(const char* path, int flags, int mode)
{
    (void)path;
    (void)flags;
    (void)mode;
    errno = ENOSYS;
    return -1;
}


extern "C" void leap_gateway_eip_apply_output_assembly(const uint8_t* data, size_t length)
{
    leap_gateway_runtime_lock();
    (void)leap_eip_bridge_apply_output_assembly(&g_gateway.bridge, data, length);
    leap_gateway_runtime_unlock();
}


extern "C" void leap_gateway_eip_pack_input_assembly(
    uint8_t* data,
    size_t   capacity,
    size_t*  length)
{
    leap_gateway_runtime_lock();
    (void)leap_eip_bridge_pack_input_assembly(&g_gateway.bridge, data, capacity, length);
    leap_gateway_runtime_unlock();
}


extern "C" void leap_gateway_runtime_lock(void)
{
    USER_ENTER_CRITICAL();
}


extern "C" void leap_gateway_runtime_unlock(void)
{
    USER_EXIT_CRITICAL();
}



static void OpenerTask(void *pd)

{

    (void)pd;

    while (1)

    {

        if (!opener_get_status())

        {

            opener_cyclic();

        }

        OSTimeDly(1);

    }

}



#define LEAPGATEWAY_MAIN_TU 1

#include "core/core_state.cpp"

#include "core/mapping_state.cpp"

#include "http/http_handlers_core.cpp"

#include "http/leap_http_handlers.cpp"

#include "http/http_register.cpp"

#undef LEAPGATEWAY_MAIN_TU

extern volatile bool bDoneWaiting4Abort;

static void GatewayInitSystem()
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    ioctl(0, IOCTL_SET | IOCTL_ALL_OPTIONS);
    ioctl(1, IOCTL_SET | IOCTL_ALL_OPTIONS);
    ioctl(2, IOCTL_SET | IOCTL_ALL_OPTIONS);

    InitBuffers();
    ConfigInit();

    IrqStdio();
    InitializeStack();
    StartConfigServer(CONFIG_SERVER_PRIO);
    OSChangePrio(MAIN_PRIO);
    EnableTaskMonitor();

    while (!bDoneWaiting4Abort)
    {
        OSTimeDly(1);
    }
}



static void PrintNetworkInfo()

{

    int ifNumber = GetFirstInterface();

    if (!ifNumber)

    {

        iprintf("No network interfaces found.\r\n");

        return;

    }



    while (ifNumber)

    {

        InterfaceBlock *ifBlock = GetInterfaceBlock(ifNumber);

        const char *ifName = ifBlock ? ifBlock->GetInterfaceName() : "Unknown";

        const char *portLabel = (ifNumber == 1) ? "Plant" : (ifNumber == 2) ? "LEAP" : "Network";

        iprintf("Interface %d (%s) [%s]\r\n", ifNumber, ifName, portLabel);

        const IPADDR4 primaryIp = InterfaceIP(ifNumber);

        iprintf("  IP:      %hI\r\n", primaryIp);

#ifdef AUTOIP

        const IPADDR4 autoIp = InterfaceAutoIP(ifNumber);

        if (!autoIp.IsNull())

        {

            iprintf("  AutoIP:  %hI\r\n", autoIp);

        }

#endif

        if (primaryIp.IsNull() && !GetInterfaceIpv4Address(ifNumber).IsNull())

        {

            iprintf("  Use:     %hI (link-local; no DHCP)\r\n", GetInterfaceIpv4Address(ifNumber));

        }

        iprintf("  Mask:    %hI\r\n", InterfaceMASK(ifNumber));

        iprintf("  Gateway: %hI\r\n", InterfaceGate(ifNumber));

        iprintf("  DNS1:    %hI\r\n", InterfaceDNS(ifNumber));

        iprintf("  DNS2:    %hI\r\n", InterfaceDNS2(ifNumber));

        ifNumber = GetNextInterface(ifNumber);

    }

}



static void GatewayInitLeapStack()

{

    LeapControllerSessionHubConfig hub_config;



    leap_gateway_runtime_init();

    leap_log_set_monotonic_us_fn(leap_rtems_monotonic_us);
    leap_log_set_stdout_enabled(0);
    leap_log_reset_origin();

    snprintf(g_gateway.config.config_path, sizeof(g_gateway.config.config_path), "%s", LEAP_GATEWAY_CONFIG_PATH);



    if (leap_gateway_storage_init() != 0)

    {

        iprintf("Gateway: %s unavailable (mapping persist disabled)\r\n", leap_gateway_storage_mount_point());

    }



    if (leap_gateway_storage_load_config(&g_gateway.config) == 0)

    {

        leap_eip_bridge_set_config(&g_gateway.bridge, &g_gateway.config.bridge);

        iprintf("Gateway: loaded config from %s (%u mapping(s))\r\n",

                g_gateway.config.config_path,

                g_gateway.config.bridge.mapping_count);

    }

    else

    {

        iprintf("Gateway: no config in %s - using defaults\r\n", leap_gateway_storage_mount_point());

    }



    if (leap_gateway_net_bring_up(&g_gateway) == 0)

    {

        leap_gateway_controller_io_init(&g_gateway.controller_io, &g_gateway.transport);



        memset(&hub_config, 0, sizeof(hub_config));

        memcpy(hub_config.default_peer.mgmt.controller_mac, g_gateway.transport.local_mac, 6);

        hub_config.default_peer.bootstrap_lease_us = 5000000u;

        hub_config.default_peer.bootstrap_watchdog_us = 500000u;

        hub_config.default_peer.recv_timeout_ms = 5000;

        hub_config.default_peer.pd.cycle_period_ms = g_gateway.config.cyclic_ms;

        hub_config.default_peer.pd.use_exchange = 1;

        hub_config.default_peer.pd.use_fixed_outputs = 1;

        hub_config.default_peer.pd.fixed_digital_outputs = 0u;

        hub_config.default_peer.default_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;

        hub_config.skip_foreign_owned_peers = 1;

        leap_controller_session_hub_init(&g_gateway.session_hub, &hub_config);



        if (leap_gateway_leap_session_start_task() == 0)

        {

            leap_gateway_leap_session_request_auto_connect(&g_gateway);

        }

    }

    else

    {

        iprintf("Gateway: LEAP transport unavailable - mappings saved but sessions disabled\r\n");

    }

}



void UserMain(void *pd)
{
    GatewayInitSystem();
    EnableSystemDiagnostics();
    EnableConfigMirror();
    EnsureGatewayPortTopology();
    StartHttp();

    const bool networkUp = WaitForNetworkWithAutoIpFallback();

    leap_build_info_print(stdout, "LEAP-Gateway-Embedded");

    iprintf("*** LEAP Gateway (NetBurner / LeapOS-Gateway embedded) ***\r\n");

    iprintf("Web Application: %s\r\nNNDK Revision: %s\r\n", AppName, GetReleaseTag());



    GatewayInitLeapStack();



    if (networkUp)

    {

        iprintf("Network is active. Device IP information:\r\n");

        PrintNetworkInfo();

        iprintf("Web UI: /network.html, /mapping.html\r\n");



        char opener_ifname[16];

        const int plantIfNumber = GetFirstInterface();

        snprintf(opener_ifname, sizeof(opener_ifname), "%d", plantIfNumber);

        opener_init(opener_ifname);

        if (!opener_get_status())

        {

            iprintf("OpENer EtherNet/IP listening on Port 1 (interface %s, %hI, TCP/UDP port 44818)\r\n",

                    opener_ifname,

                    InterfaceIP(plantIfNumber));

            iprintf("Assemblies: Input=100 (32B), Output=150 (32B), Config=151 (10B)\r\n");

            iprintf("LEAP Master on Port 2 (interface %s)\r\n", g_gateway.bound_ifname[0] ? g_gateway.bound_ifname : "2");

            OSSimpleTaskCreatewName(OpenerTask, MAIN_PRIO - 2, "OpENer");

        }

        else

        {

            iprintf("Warning: OpENer EtherNet/IP stack failed to start on interface %s\r\n", opener_ifname);

        }

    }

    else

    {

        iprintf("Warning: network did not become active (no DHCP or AutoIP address).\r\n");

    }



    while (1)

    {

        OSTimeDly(TICKS_PER_SECOND);

    }

}

