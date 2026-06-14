/*
 * gateway_main_linux.c — LeapOS-Gateway Linux entry point.
 *
 * Same structure as the RTEMS Init task: load config, bring up networking,
 * init session hub + HTTP, start the LEAP session thread, then poll HTTP
 * (and OpENer when enabled) every 10 ms forever.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_config.h"
#include "gateway_global.h"
#include "gateway_http.h"
#include "gateway_leap_session.h"
#include "gateway_net.h"
#include "gateway_rtems_io.h"
#include "gateway_storage.h"
#include "leap_time.h"
#include "leap_transport.h"

#if LEAP_GATEWAY_OPENER_ENABLE
#include "opener.h"
#endif

#include "leap/leap_build_info.h"
#include "leap/leap_controller_session_hub.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_gateway_config.h"
#include "leap/leap_protocol.h"

#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LEAP_GATEWAY_IF_WAIT_S 30

static void
gateway_wait_for_interface(const char* ifname)
{
    int waited;

    for (waited = 0; waited < LEAP_GATEWAY_IF_WAIT_S; ++waited)
    {
        if (if_nametoindex(ifname) != 0u)
        {
            return;
        }
        if (waited == 0)
        {
            printf(
                LEAP_TS_FMT LEAP_ANSI_INFO "Gateway: waiting for %s" LEAP_ANSI_RESET "\n",
                leap_rtems_uptime_str(),
                ifname);
        }
        sleep(1);
    }
}

int
main(int argc, char** argv)
{
    LeapControllerSessionHubConfig hub_config;
    const char*                    config_path = LEAP_GATEWAY_CONFIG_PATH;
    int                            i;
    int                            net_ready = 0;

    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            config_path = argv[++i];
        }
    }

    /* HTTP clients that disconnect mid-reply must not kill the daemon. */
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);

    leap_build_info_print(stdout, "LeapOS-Gateway");
    printf(
        LEAP_TS_FMT LEAP_ANSI_BANNER "*** LeapOS-Gateway (Linux, E/IP bridge) ***" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());

    leap_gateway_runtime_init();
    snprintf(
        g_gateway.config.config_path,
        sizeof(g_gateway.config.config_path),
        "%s",
        config_path);

    if (leap_gateway_storage_init() != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: %s not writable (Save disabled)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            leap_gateway_storage_mount_point());
    }

    if (access(config_path, R_OK) == 0 &&
        leap_gateway_config_load_file(&g_gateway.config, config_path) == 0)
    {
        snprintf(
            g_gateway.config.config_path,
            sizeof(g_gateway.config.config_path),
            "%s",
            config_path);
        leap_eip_bridge_set_config(&g_gateway.bridge, &g_gateway.config.bridge);
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: no config at %s — using defaults" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            config_path);
    }

    if (!g_gateway.config.network.auto_ifname)
    {
        gateway_wait_for_interface(leap_gateway_leap_ifname(&g_gateway.config));
    }

    if (leap_gateway_net_bring_up(&g_gateway) == 0)
    {
        net_ready = 1;
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: LEAP transport unavailable — Web UI starting in degraded mode" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

    if (net_ready)
    {
        leap_gateway_controller_io_init(
            &g_gateway.controller_io,
            &g_gateway.transport);
    }

    memset(&hub_config, 0, sizeof(hub_config));
    memcpy(
        hub_config.default_peer.mgmt.controller_mac,
        g_gateway.transport.local_mac,
        6);
    hub_config.default_peer.bootstrap_lease_us    = 5000000u;
    hub_config.default_peer.bootstrap_watchdog_us = 500000u;
    hub_config.default_peer.recv_timeout_ms       = 5000;
    hub_config.default_peer.pd.cycle_period_ms      = g_gateway.config.cyclic_ms;
    hub_config.default_peer.pd.use_exchange         = 1;
    hub_config.default_peer.pd.use_fixed_outputs    = 1;
    hub_config.default_peer.pd.fixed_digital_outputs = 0u;
    hub_config.default_peer.default_profile_id    = LEAP_PROFILE_DIGITAL_IO_8X8;
    hub_config.skip_foreign_owned_peers           = 1;
    leap_controller_session_hub_init(&g_gateway.session_hub, &hub_config);

    if (leap_gateway_http_init() != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: HTTP init failed" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

#if LEAP_GATEWAY_OPENER_ENABLE
    opener_init(g_gateway.bound_ifname[0] != '\0' ? g_gateway.bound_ifname : leap_gateway_eip_ifname(&g_gateway.config));
    if (opener_get_status() == 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_OK "Gateway: EtherNet/IP (OpENer) on %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            g_gateway.bound_ifname[0] != '\0' ? g_gateway.bound_ifname : leap_gateway_eip_ifname(&g_gateway.config));
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: OpENer init failed" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }
#endif

    if (!net_ready)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: LEAP scan/connect unavailable (transport not bound)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }
    else if (leap_gateway_leap_session_start_task() != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: LEAP scan/connect unavailable (session thread failed)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }
    else
    {
        leap_gateway_leap_session_request_auto_connect(&g_gateway);
    }

    for (;;)
    {
        leap_gateway_http_poll();

#if LEAP_GATEWAY_OPENER_ENABLE
        if (opener_get_status() == 0)
        {
            opener_cyclic();
        }
#endif

        usleep(10u * 1000u);
    }

    return 0;
}
