/*
 * gateway_init.c — LeapOS-Gateway RTEMS application entry.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include <rtems.h>
#include <rtems/bsd/bsd.h>
#include <rtems/bspIo.h>

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
#include "leap/leap_controller_stack.h"
#include "leap/leap_gateway_config.h"
#include "leap/leap_protocol.h"

#include <machine/rtems-bsd-commands.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

rtems_task
Init(rtems_task_argument ignored)
{
    LeapControllerStackConfig stack_config;
    rtems_status_code         sc;

    (void)ignored;

    leap_build_info_print(stdout, "LeapOS-Gateway");
    printf(
        LEAP_TS_FMT LEAP_ANSI_BANNER "*** LeapOS-Gateway (E/IP bridge) ***" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());

    leap_gateway_runtime_init();
    if (leap_gateway_storage_init() == 0)
    {
        (void)leap_gateway_config_load_file(
            &g_gateway.config,
            LEAP_GATEWAY_CONFIG_PATH);
        leap_eip_bridge_set_config(&g_gateway.bridge, &g_gateway.config.bridge);
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: no config volume - Web UI Apply works; Save needs CF/IDE boot media" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

    /* Emergency recovery profile: always boot on known network settings. */
    g_gateway.config.network.mode = LEAP_GATEWAY_NIC_SINGLE;
    g_gateway.config.network.dhcp = 0;
    g_gateway.config.network.auto_ifname = 1;
    strncpy(g_gateway.config.network.ifname, "re0", sizeof(g_gateway.config.network.ifname) - 1u);
    g_gateway.config.network.ifname[sizeof(g_gateway.config.network.ifname) - 1u] = '\0';
    strncpy(g_gateway.config.network.ipv4_addr, "192.168.1.2", sizeof(g_gateway.config.network.ipv4_addr) - 1u);
    g_gateway.config.network.ipv4_addr[sizeof(g_gateway.config.network.ipv4_addr) - 1u] = '\0';
    strncpy(g_gateway.config.network.ipv4_mask, "255.255.255.0", sizeof(g_gateway.config.network.ipv4_mask) - 1u);
    g_gateway.config.network.ipv4_mask[sizeof(g_gateway.config.network.ipv4_mask) - 1u] = '\0';

    sc = rtems_bsd_initialize();
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf("network init failed: %s\n", rtems_status_text(sc));
        rtems_task_suspend(RTEMS_SELF);
    }

    rtems_task_wake_after(2);
    (void)rtems_bsd_ifconfig_lo0();

    if (leap_gateway_net_bring_up(&g_gateway) != 0)
    {
        rtems_task_suspend(RTEMS_SELF);
    }

    leap_gateway_controller_io_init(
        &g_gateway.controller_io,
        &g_gateway.transport);

    memset(&stack_config, 0, sizeof(stack_config));
    memcpy(
        stack_config.mgmt.controller_mac,
        g_gateway.transport.local_mac,
        6);
    stack_config.bootstrap_lease_us = 5000000u;
    stack_config.pd.cycle_period_ms = g_gateway.config.cyclic_ms;
    stack_config.pd.use_exchange = 1;
    stack_config.default_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
    leap_controller_stack_init(&g_gateway.controller, &stack_config);

#if LEAP_GATEWAY_OPENER_ENABLE
    opener_init(leap_gateway_eip_ifname(&g_gateway.config));
    if (opener_get_status() == 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_OK "Gateway: EtherNet/IP (OpENer) on %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            leap_gateway_eip_ifname(&g_gateway.config));
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: OpENer init failed" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }
#endif

    if (leap_gateway_http_init() != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: HTTP init failed" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

    if (leap_gateway_leap_session_start_task() != 0)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR "Gateway: LEAP session task failed" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }
    else
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_INFO
            "Gateway: LEAP autoconnect disabled - use Web UI Connect LEAP when ready" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

    for (;;)
    {
#if LEAP_GATEWAY_OPENER_ENABLE
        if (opener_get_status() == 0)
        {
            opener_cyclic();
        }
#endif

        leap_gateway_http_poll();

        rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(10));
    }
}

#define RTEMS_BSD_CONFIG_DOMAIN_PAGE_MBUFS_SIZE (128 * 1024 * 1024)
#define RTEMS_BSD_CONFIG_DOMAIN_BIO_SIZE (16 * 1024 * 1024)
#define RTEMS_BSD_CONFIG_BSP_CONFIG
#define RTEMS_BSD_CONFIG_INIT

#include <machine/rtems-bsd-config.h>

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK

#ifdef RTEMS_BSP_HAS_IDE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_IDE_DRIVER
#endif

#define CONFIGURE_FILESYSTEM_DOSFS

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 256
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1
#define CONFIGURE_MAXIMUM_TASKS 12
#define CONFIGURE_UNLIMITED_ALLOCATION_SIZE 32
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE (64 * 1024)
#define CONFIGURE_BDBUF_MAX_READ_AHEAD_BLOCKS 4
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (1 * 1024 * 1024)

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT_TASK_PRIORITY 248u
#define CONFIGURE_INIT_TASK_STACK_SIZE (192 * 1024)
#define CONFIGURE_INIT_TASK_INITIAL_MODES RTEMS_DEFAULT_MODES
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_DEFAULT_ATTRIBUTES
#define CONFIGURE_INIT

#include <bsp.h>
#include <rtems/confdefs.h>
