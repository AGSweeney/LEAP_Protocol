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
#include "leap/leap_controller_session_hub.h"
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
    LeapControllerSessionHubConfig hub_config;
    rtems_status_code              sc;

    (void)ignored;

    leap_build_info_print(stdout, "LeapOS-Gateway");
    printf(
        LEAP_TS_FMT LEAP_ANSI_BANNER "*** LeapOS-Gateway (E/IP bridge) ***" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());

    leap_gateway_runtime_init();

    /*
     * Mount boot CF before libbsd — legacy pc386 IDE reads can fail once
     * the BSD stack has initialized on some ICH7 + CF-via-IDE boards.
     */
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
            "Gateway: no config volume — IDE could not mount /cf (Save disabled)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }

    sc = rtems_bsd_initialize();
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf("network init failed: %s\n", rtems_status_text(sc));
        rtems_task_suspend(RTEMS_SELF);
    }

    rtems_task_wake_after(2);
    sleep(2);
    (void)rtems_bsd_ifconfig_lo0();

    if (!leap_gateway_storage_ready() &&
        leap_gateway_storage_retry_after_pci() == 0)
    {
        (void)leap_gateway_config_load_file(
            &g_gateway.config,
            LEAP_GATEWAY_CONFIG_PATH);
        leap_eip_bridge_set_config(&g_gateway.bridge, &g_gateway.config.bridge);
        printf(
            LEAP_TS_FMT LEAP_ANSI_OK
            "Gateway: config loaded from %s after PCI storage retry" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            LEAP_GATEWAY_CONFIG_PATH);
    }

    if (leap_gateway_net_bring_up(&g_gateway) != 0)
    {
        rtems_task_suspend(RTEMS_SELF);
    }

    leap_gateway_controller_io_init(
        &g_gateway.controller_io,
        &g_gateway.transport);

    memset(&hub_config, 0, sizeof(hub_config));
    memcpy(
        hub_config.default_peer.mgmt.controller_mac,
        g_gateway.transport.local_mac,
        6);
    hub_config.default_peer.bootstrap_lease_us    = 5000000u;
    hub_config.default_peer.bootstrap_watchdog_us = 500000u;
    hub_config.default_peer.recv_timeout_ms       = 5000;
    hub_config.default_peer.pd.cycle_period_ms     = g_gateway.config.cyclic_ms;
    hub_config.default_peer.pd.use_exchange        = 1;
    hub_config.default_peer.pd.use_fixed_outputs   = 1;
    hub_config.default_peer.pd.fixed_digital_outputs = 0u;
    hub_config.default_peer.default_profile_id       = LEAP_PROFILE_DIGITAL_IO_8X8;
    hub_config.skip_foreign_owned_peers              = 1;
    leap_controller_session_hub_init(&g_gateway.session_hub, &hub_config);

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
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: LEAP scan/connect unavailable (session task failed)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str());
    }
    else
    {
        leap_gateway_leap_session_request_auto_connect(&g_gateway);
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

/* pc386 /dev/hda for CF on ICH7 SATA0 legacy — required for /cf dosfs mount. */
#define CONFIGURE_APPLICATION_NEEDS_IDE_DRIVER

#define CONFIGURE_FILESYSTEM_DOSFS

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 256
#define CONFIGURE_MAXIMUM_DRIVERS 32
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1
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
