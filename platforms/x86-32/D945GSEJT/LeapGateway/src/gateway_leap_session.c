/*
 * gateway_leap_session.c — Bootstrap and maintain LEAP owner sessions.
 *
 * LEAP bootstrap and cyclic PD run in a dedicated RTEMS task so blocking
 * L2 I/O never stalls the init-task poll loop (HTTP + EtherNet/IP).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "gateway_leap_session.h"

#include "gateway_config.h"
#include "gateway_global.h"
#include "gateway_pd_io.h"
#include "leap_time.h"

#include "leap/leap_controller_peer.h"
#include "leap/leap_eip_bridge.h"
#include "leap/leap_pd_controller.h"
#include "leap/leap_protocol.h"

#include <rtems.h>
#include <stdio.h>
#include <string.h>

#define LEAP_GATEWAY_SESSION_TASK_STACK (64u * 1024u)
#define LEAP_GATEWAY_SESSION_TASK_PRIO  250u

static rtems_id g_leap_session_task = RTEMS_INVALID_ID;
static volatile int g_leap_session_stop = 0;

static int
mapping_mac_is_zero(const uint8_t mac[6])
{
    static const uint8_t k_zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };

    return (memcmp(mac, k_zero, 6) == 0) ? 1 : 0;
}

static int
gateway_find_enabled_mapping(
    const LeapGatewayRuntime* gw,
    unsigned*                 mapping_index_out,
    uint8_t                   peer_mac_out[6],
    uint32_t*                 profile_id_out)
{
    unsigned i;

    if (gw == NULL || mapping_index_out == NULL || peer_mac_out == NULL)
    {
        return 0;
    }

    for (i = 0u; i < gw->config.bridge.mapping_count; ++i)
    {
        const LeapEipBridgeMapping* map = &gw->config.bridge.mappings[i];

        if (!map->enabled || mapping_mac_is_zero(map->leap_mac))
        {
            continue;
        }

        *mapping_index_out = i;
        memcpy(peer_mac_out, map->leap_mac, 6);
        if (profile_id_out != NULL)
        {
            *profile_id_out = map->profile_id;
        }
        return 1;
    }

    return 0;
}

static void
gateway_build_hello_from_entry(
    const LeapControllerPeerEntry* entry,
    uint32_t                       profile_id,
    LeapHelloReply*                hello_out)
{
    uint32_t profile = profile_id;

    if (profile == 0u)
    {
        profile = LEAP_PROFILE_DIGITAL_IO_8X8;
    }

    memset(hello_out, 0, sizeof(*hello_out));
    if (entry == NULL)
    {
        hello_out->current_state      = (uint16_t)LEAP_STATE_CONFIGURED;
        hello_out->active_profile_id  = profile;
        hello_out->default_profile_id = profile;
        return;
    }

    hello_out->current_state      = entry->device_state;
    hello_out->active_profile_id  = entry->active_profile_id;
    hello_out->default_profile_id = entry->default_profile_id;
    memcpy(hello_out->active_owner_mac, entry->active_owner_mac, 6);
}

static void
gateway_sync_stack_config(
    LeapGatewayRuntime* gw,
    const uint8_t       peer_mac[6],
    uint32_t            profile_id)
{
    LeapControllerStackConfig* cfg;

    if (gw == NULL || peer_mac == NULL)
    {
        return;
    }

    cfg = &gw->controller.config;
    memcpy(cfg->mgmt.controller_mac, gw->transport.local_mac, 6);
    memcpy(cfg->target_peer_mac, peer_mac, 6);
    cfg->single_peer_auto_select = 0;
    cfg->bootstrap_lease_us      = 5000000u;
    cfg->bootstrap_watchdog_us     = 500000u;
    cfg->recv_timeout_ms           = 500;
    cfg->pd.cycle_period_ms        = gw->config.cyclic_ms;
    cfg->pd.use_exchange           = 1;
    cfg->default_profile_id =
        (profile_id != 0u) ? profile_id : LEAP_PROFILE_DIGITAL_IO_8X8;
}

static void
gateway_sync_bridge_from_pd(
    unsigned            mapping_index,
    LeapPdControllerIo* pd_io,
    int                 comm_ok)
{
    const LeapPdControllerStats* stats;
    uint16_t                     outputs = 0u;

    if (g_gateway.bridge.outputs_dirty)
    {
        (void)leap_eip_bridge_peer_outputs(
            &g_gateway.bridge,
            mapping_index,
            &outputs);
        (void)leap_controller_stack_pd_single_write(
            &g_gateway.controller,
            pd_io,
            outputs);
        g_gateway.bridge.outputs_dirty = 0;
    }

    stats = leap_pd_controller_stats(&g_gateway.controller.pd);
    leap_eip_bridge_update_peer_io(
        &g_gateway.bridge,
        mapping_index,
        stats != NULL ? stats->last_digital_inputs : 0u,
        outputs,
        LEAP_DIO_STATUS_OK,
        comm_ok);
}

static void
gateway_run_pending_discover(void)
{
    int scan_ms;

    scan_ms = g_gateway.discover_pending_ms;
    if (scan_ms <= 0)
    {
        return;
    }

    g_gateway.discover_pending_ms = 0;
    (void)leap_controller_peer_table_discover(
        &g_gateway.peer_table,
        &g_gateway.controller_io,
        scan_ms);
}

int
leap_gateway_leap_session_active(const LeapGatewayRuntime* gw)
{
    if (gw == NULL)
    {
        return 0;
    }

    return (gw->leap_session.active != 0 &&
            leap_controller_stack_get_phase(&gw->controller) == LEAP_CTRL_STACK_OP)
               ? 1
               : 0;
}

void
leap_gateway_leap_session_disconnect(LeapGatewayRuntime* gw)
{
    if (gw == NULL)
    {
        return;
    }

    if (gw->controller.peer_bound != 0 ||
        leap_controller_stack_get_phase(&gw->controller) != LEAP_CTRL_STACK_IDLE)
    {
        (void)leap_controller_stack_release(&gw->controller, &gw->controller_io);
    }

    gw->leap_session.active          = 0;
    gw->leap_session.mapping_index   = -1;
    gw->leap_session.connect_pending = 0;
    gw->leap_session.last_status     = LEAP_CTRL_STACK_OK;
    gw->leap_session.retry_ticks     = 0u;
    memset(gw->leap_session.peer_mac, 0, sizeof(gw->leap_session.peer_mac));
}

void
leap_gateway_leap_session_request_connect(LeapGatewayRuntime* gw)
{
    if (gw == NULL)
    {
        return;
    }

    gw->leap_session.connect_pending = 1;
    gw->leap_session.retry_ticks     = 0u;
}

int
leap_gateway_leap_session_connect(LeapGatewayRuntime* gw)
{
    unsigned                  mapping_index;
    uint8_t                   peer_mac[6];
    uint32_t                  profile_id = 0u;
    LeapHelloReply            hello;
    const LeapControllerPeerEntry* entry;
    LeapControllerStackStatus status;
    int                       peer_index;

    if (gw == NULL)
    {
        return 0;
    }

    if (!gateway_find_enabled_mapping(
            gw,
            &mapping_index,
            peer_mac,
            &profile_id))
    {
        leap_gateway_leap_session_disconnect(gw);
        return 0;
    }

    if (leap_gateway_leap_session_active(gw) &&
        memcmp(gw->leap_session.peer_mac, peer_mac, 6) == 0 &&
        gw->leap_session.mapping_index == (int)mapping_index)
    {
        return 1;
    }

    if (gw->leap_session.active != 0 ||
        gw->controller.peer_bound != 0 ||
        leap_controller_stack_get_phase(&gw->controller) != LEAP_CTRL_STACK_IDLE)
    {
        leap_gateway_leap_session_disconnect(gw);
    }

    gateway_sync_stack_config(gw, peer_mac, profile_id);

    (void)leap_controller_peer_table_probe_peer(
        &gw->peer_table,
        &gw->controller_io,
        peer_mac,
        LEAP_CTRL_PEER_PROBE_TIMEOUT_MS);

    entry = NULL;
    peer_index = leap_controller_peer_table_find(&gw->peer_table, peer_mac);
    if (peer_index >= 0)
    {
        entry = leap_controller_peer_table_get(
            &gw->peer_table,
            (unsigned)peer_index);
    }

    gateway_build_hello_from_entry(entry, profile_id, &hello);

    status = leap_controller_stack_bootstrap_peer(
        &gw->controller,
        &gw->controller_io,
        peer_mac,
        &hello);
    gw->leap_session.last_status = status;

    if (status != LEAP_CTRL_STACK_OK ||
        leap_controller_stack_get_phase(&gw->controller) != LEAP_CTRL_STACK_OP)
    {
        gw->leap_session.active        = 0;
        gw->leap_session.mapping_index = -1;
        printf(
            LEAP_TS_FMT LEAP_ANSI_WARN
            "Gateway: LEAP bootstrap failed for mapped peer (status=%d phase=%u)" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            (int)status,
            (unsigned)leap_controller_stack_get_phase(&gw->controller));
        return 0;
    }

    memcpy(gw->leap_session.peer_mac, peer_mac, 6);
    gw->leap_session.mapping_index = (int)mapping_index;
    gw->leap_session.active        = 1;
    gw->leap_session.retry_ticks   = 0u;
    printf(
        LEAP_TS_FMT LEAP_ANSI_OK
        "Gateway: LEAP owner session OP with mapped peer" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());
    return 1;
}

static void
leap_gateway_leap_session_task(rtems_task_argument ignored)
{
    LeapPdControllerIo pd_io;

    (void)ignored;

    leap_gateway_pd_io_init(&pd_io, &g_gateway.transport);

    for (;;)
    {
        rtems_interval delay_ticks;

        if (g_leap_session_stop != 0)
        {
            break;
        }

        gateway_run_pending_discover();

        if (g_gateway.leap_session.connect_pending != 0)
        {
            g_gateway.leap_session.connect_pending = 0;
            (void)leap_gateway_leap_session_connect(&g_gateway);
        }

        if (leap_gateway_leap_session_active(&g_gateway))
        {
            LeapPdControllerStatus pd_status;
            unsigned               mapping_index;

            mapping_index = (unsigned)g_gateway.leap_session.mapping_index;

            pd_status = leap_pd_controller_run_one_cycle(
                &g_gateway.controller.pd,
                &g_gateway.controller.mgmt,
                &pd_io,
                g_gateway.leap_session.peer_mac,
                (volatile int*)&g_leap_session_stop,
                0);

            gateway_sync_bridge_from_pd(
                mapping_index,
                &pd_io,
                pd_status == LEAP_PD_CTRL_OK);

            delay_ticks =
                RTEMS_MILLISECONDS_TO_TICKS(g_gateway.config.cyclic_ms);
        }
        else
        {
            delay_ticks = RTEMS_MILLISECONDS_TO_TICKS(50);
        }

        rtems_task_wake_after(delay_ticks);
    }

    rtems_task_exit();
}

int
leap_gateway_leap_session_start_task(void)
{
    rtems_status_code sc;

    if (g_leap_session_task != RTEMS_INVALID_ID)
    {
        return 0;
    }

    sc = rtems_task_create(
        rtems_build_name('L', 'E', 'A', 'P'),
        LEAP_GATEWAY_SESSION_TASK_PRIO,
        LEAP_GATEWAY_SESSION_TASK_STACK,
        RTEMS_DEFAULT_MODES,
        RTEMS_DEFAULT_ATTRIBUTES,
        &g_leap_session_task);
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR
            "Gateway: LEAP session task create failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            rtems_status_text(sc));
        return -1;
    }

    sc = rtems_task_start(g_leap_session_task, leap_gateway_leap_session_task, 0);
    if (sc != RTEMS_SUCCESSFUL)
    {
        printf(
            LEAP_TS_FMT LEAP_ANSI_ERR
            "Gateway: LEAP session task start failed: %s" LEAP_ANSI_RESET "\n",
            leap_rtems_uptime_str(),
            rtems_status_text(sc));
        (void)rtems_task_delete(g_leap_session_task);
        g_leap_session_task = RTEMS_INVALID_ID;
        return -1;
    }

    printf(
        LEAP_TS_FMT LEAP_ANSI_OK "Gateway: LEAP session task started" LEAP_ANSI_RESET "\n",
        leap_rtems_uptime_str());
    return 0;
}
