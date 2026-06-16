/*

 * gateway_leap_session.c — Bootstrap and maintain LEAP owner sessions.

 *

 * LEAP bootstrap and cyclic PD run in a dedicated RTEMS task so blocking

 * L2 I/O never stalls the init-task poll loop (HTTP + EtherNet/IP).

 *

 * Each enabled mapping slot (0 .. mapping_count-1) maps to a session-hub

 * slot with independent MGMT/PD state so multiple peers run concurrently.

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

#define LEAP_GATEWAY_BOOTSTRAP_PROBE_MS   1500
#define LEAP_GATEWAY_RECONNECT_PROBE_MS   300
#define LEAP_GATEWAY_PD_MISS_LIMIT        3u
#define LEAP_GATEWAY_RECONNECT_TICKS      20u

static unsigned g_bootstrap_rr_index;

#include "leap/leap_controller_session_hub.h"

#include "leap/leap_eip_bridge.h"

#include "leap/leap_pd_controller.h"

#include "leap/leap_protocol.h"



#include "leap_transport.h"

#if defined(__rtems__)
#include <rtems.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include <stdio.h>

#include <string.h>



#if defined(__rtems__)

#define LEAP_GATEWAY_SESSION_TASK_STACK (64u * 1024u)

#define LEAP_GATEWAY_SESSION_TASK_PRIO  250u

static rtems_id         g_leap_session_task = RTEMS_INVALID_ID;

#else

static pthread_t        g_leap_session_thread;

static int              g_leap_session_thread_started = 0;

#endif

static volatile int     g_leap_session_stop = 0;

static void
gateway_session_sleep_ms(unsigned ms)
{
#if defined(__rtems__)
    rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(ms));
#else
    usleep(ms * 1000u);
#endif
}



static int

mapping_mac_is_zero(const uint8_t mac[6])

{

    static const uint8_t k_zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };



    return (memcmp(mac, k_zero, 6) == 0) ? 1 : 0;

}



static int

mapping_slot_enabled(const LeapGatewayRuntime* gw, unsigned slot)

{

    const LeapEipBridgeMapping* map;



    if (gw == NULL || slot >= gw->config.bridge.mapping_count)

    {

        return 0;

    }



    map = &gw->config.bridge.mappings[slot];

    return (map->enabled && !mapping_mac_is_zero(map->leap_mac)) ? 1 : 0;

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



static int

gateway_mac_is_zero(const uint8_t mac[6])

{

    static const uint8_t zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };



    return memcmp(mac, zero, 6) == 0;

}



static const char*

gateway_stack_phase_name(LeapControllerStackPhase phase)

{

    switch (phase)

    {

    case LEAP_CTRL_STACK_IDLE:

        return "IDLE";

    case LEAP_CTRL_STACK_DISCOVERING:

        return "DISCOVERING";

    case LEAP_CTRL_STACK_SELECT_PROFILE:

        return "SELECT_PROFILE";

    case LEAP_CTRL_STACK_OPEN_SESSION:

        return "OPEN_SESSION";

    case LEAP_CTRL_STACK_SET_STATE:

        return "SET_STATE";

    case LEAP_CTRL_STACK_OP:

        return "OP";

    case LEAP_CTRL_STACK_FAULT:

        return "FAULT";

    default:

        return "UNKNOWN";

    }

}



static const char*

gateway_stack_status_name(LeapControllerStackStatus status)

{

    switch (status)

    {

    case LEAP_CTRL_STACK_OK:

        return "OK";

    case LEAP_CTRL_STACK_INVALID_ARG:

        return "INVALID_ARG";

    case LEAP_CTRL_STACK_IO_MISSING:

        return "IO_MISSING";

    case LEAP_CTRL_STACK_SEND_FAILED:

        return "SEND_FAILED";

    case LEAP_CTRL_STACK_RECV_TIMEOUT:

        return "RECV_TIMEOUT";

    case LEAP_CTRL_STACK_UNEXPECTED_REPLY:

        return "UNEXPECTED_REPLY";

    case LEAP_CTRL_STACK_MGMT_ERROR:

        return "MGMT_ERROR";

    case LEAP_CTRL_STACK_DIR_ERROR:

        return "DIR_ERROR";

    case LEAP_CTRL_STACK_DISC_ERROR:

        return "DISC_ERROR";

    case LEAP_CTRL_STACK_ABORTED:

        return "ABORTED";

    default:

        return "ERROR";

    }

}



static void

gateway_drain_leap_rx(LeapRtemsTransport* transport)

{

    uint8_t scratch[256];

    uint8_t src_mac[6];

    size_t  payload_len;



    if (transport == NULL)

    {

        return;

    }



    while (leap_rtems_transport_recv(

               transport,

               src_mac,

               scratch,

               sizeof(scratch),

               &payload_len,

               0) == 0)

    {

        /* drop stale DISC/MGMT/PD frames before bootstrap */

    }

}



static void

gateway_sync_hub_config(LeapGatewayRuntime* gw)

{

    LeapControllerStackConfig* def;



    if (gw == NULL)

    {

        return;

    }



    def = &gw->session_hub.config.default_peer;

    memcpy(def->mgmt.controller_mac, gw->transport.local_mac, 6);

    def->bootstrap_lease_us        = 5000000u;

    def->bootstrap_watchdog_us     = 500000u;

    def->recv_timeout_ms           = 5000;

    def->pd.cycle_period_ms        = gw->config.cyclic_ms;

    def->pd.use_exchange           = 1;

    def->pd.use_fixed_outputs      = 1;

    def->pd.fixed_digital_outputs  = 0u;

    def->single_peer_auto_select   = 0;

    if (def->default_profile_id == 0u)

    {

        def->default_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;

    }

}



static void

gateway_sync_bridge_from_pd(

    unsigned            mapping_index,

    LeapPdControllerIo* pd_io,

    int                 comm_ok)

{

    LeapControllerStack*       stack;

    const LeapPdControllerStats* stats;

    uint16_t                     outputs = 0u;



    stack = leap_controller_session_hub_stack(&g_gateway.session_hub, (int)mapping_index);

    if (stack == NULL)

    {

        return;

    }



    stats = leap_pd_controller_stats(&stack->pd);

    (void)leap_eip_bridge_peer_outputs(&g_gateway.bridge, mapping_index, &outputs);

    leap_eip_bridge_update_peer_io(

        &g_gateway.bridge,

        mapping_index,

        stats != NULL ? stats->last_digital_inputs : 0u,

        outputs,

        LEAP_DIO_STATUS_OK,

        comm_ok);



    (void)pd_io;

}



static void

gateway_sync_pd_outputs_from_bridge(LeapGatewayRuntime* gw, unsigned mapping_index)

{

    LeapControllerStack* stack;

    uint16_t             outputs = 0u;



    if (gw == NULL)

    {

        return;

    }



    stack = leap_controller_session_hub_stack(&gw->session_hub, (int)mapping_index);

    if (stack == NULL)

    {

        return;

    }



    if (leap_eip_bridge_peer_outputs(&gw->bridge, mapping_index, &outputs) != 0)

    {

        return;

    }



    stack->pd.config.use_fixed_outputs     = 1;

    stack->pd.config.fixed_digital_outputs = outputs;

}



static void

gateway_mark_mapping_down(

    LeapGatewayRuntime*          gw,

    unsigned                     mapping_index,

    const LeapPdControllerIo*    pd_io)

{

    const LeapEipBridgePeerIo* peer;

    uint16_t                   inputs = 0u;

    uint16_t                   outputs = 0u;

    uint16_t                   status = LEAP_DIO_STATUS_OK;



    if (gw == NULL || mapping_index >= LEAP_EIP_BRIDGE_MAX_MAPPINGS)

    {

        return;

    }



    peer = &gw->bridge.peer_io[mapping_index];

    inputs = peer->digital_inputs;

    outputs = peer->digital_outputs;

    status = peer->io_status;

    leap_eip_bridge_update_peer_io(

        &gw->bridge,

        mapping_index,

        inputs,

        outputs,

        status,

        0);



    if (leap_controller_session_hub_is_op(&gw->session_hub, (int)mapping_index) != 0 ||

        (mapping_index < LEAP_CTRL_MAX_PEERS &&

         gw->session_hub.slots[mapping_index].in_use != 0))

    {

        printf(

            LEAP_TS_FMT LEAP_ANSI_WARN

            "Gateway: LEAP peer mapping %u down, releasing slot for retry" LEAP_ANSI_RESET "\n",

            leap_rtems_uptime_str(),

            mapping_index);

        (void)leap_controller_session_hub_release(

            &gw->session_hub,

            (int)mapping_index,

            &gw->controller_io);

    }



    if (mapping_index < LEAP_CTRL_MAX_PEERS)

    {

        gw->leap_session.pd_miss_count[mapping_index] = 0u;

        gw->leap_session.reconnect_ticks[mapping_index] = LEAP_GATEWAY_RECONNECT_TICKS;

    }



    (void)pd_io;

}



static void

gateway_push_dirty_outputs(LeapGatewayRuntime* gw, const LeapPdControllerIo* pd_io)

{

    unsigned i;



    if (gw == NULL || !gw->bridge.outputs_dirty)

    {

        return;

    }



    for (i = 0u; i < gw->config.bridge.mapping_count; ++i)

    {

        LeapControllerStack* stack;

        uint16_t             outputs = 0u;



        if (!mapping_slot_enabled(gw, i))

        {

            continue;

        }



        if (leap_controller_session_hub_is_op(&gw->session_hub, (int)i) == 0)

        {

            continue;

        }



        stack = leap_controller_session_hub_stack(&gw->session_hub, (int)i);

        if (stack == NULL)

        {

            continue;

        }



        if (leap_eip_bridge_peer_outputs(&gw->bridge, i, &outputs) != 0)

        {

            continue;

        }



        gateway_sync_pd_outputs_from_bridge(gw, i);

        (void)leap_controller_stack_pd_single_write(stack, pd_io, outputs);

    }



    gw->bridge.outputs_dirty = 0;

}



static LeapControllerStackStatus

gateway_bootstrap_mapping_slot(

    LeapGatewayRuntime*          gw,

    unsigned                     mapping_index,

    const LeapEipBridgeMapping*  map,

    int                          probe_ms)

{

    LeapHelloReply                 hello;

    const LeapControllerPeerEntry* entry;

    LeapControllerStackStatus      status;

    int                            peer_index;

    uint32_t                       profile_id;



    if (gw == NULL || map == NULL || mapping_index >= LEAP_EIP_BRIDGE_MAX_MAPPINGS)

    {

        return LEAP_CTRL_STACK_INVALID_ARG;

    }



    if (leap_controller_session_hub_is_op(&gw->session_hub, (int)mapping_index) != 0 &&

        gw->session_hub.slots[mapping_index].in_use != 0 &&

        memcmp(gw->session_hub.slots[mapping_index].peer_mac, map->leap_mac, 6) == 0)

    {

        return LEAP_CTRL_STACK_OK;

    }



    if (gw->session_hub.slots[mapping_index].in_use != 0)

    {

        (void)leap_controller_session_hub_release(

            &gw->session_hub,

            (int)mapping_index,

            &gw->controller_io);

    }



    profile_id = map->profile_id;

    if (profile_id == 0u)

    {

        profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;

    }



    gateway_sync_hub_config(gw);

    gw->session_hub.config.default_peer.default_profile_id = profile_id;

    memcpy(gw->session_hub.config.default_peer.target_peer_mac, map->leap_mac, 6);



    {

        LeapControllerPeerStatus probe_status;

        int                      use_live_bootstrap = 0;



        peer_index = leap_controller_peer_table_find(&gw->peer_table, map->leap_mac);

        if (peer_index >= 0)

        {

            gw->peer_table.peers[peer_index].reachable = 0;

        }



        probe_status = leap_controller_peer_table_probe_peer(

            &gw->peer_table,

            &gw->controller_io,

            map->leap_mac,

            (probe_ms > 0) ? probe_ms : LEAP_GATEWAY_BOOTSTRAP_PROBE_MS);



        entry = NULL;

        peer_index = leap_controller_peer_table_find(&gw->peer_table, map->leap_mac);

        if (probe_status != LEAP_CTRL_PEER_OK ||

            peer_index < 0)

        {

            use_live_bootstrap = 1;

        }

        else

        {

            entry = leap_controller_peer_table_get(

                &gw->peer_table,

                (unsigned)peer_index);

            if (entry == NULL || entry->reachable == 0)

            {

                use_live_bootstrap = 1;

            }

            else if (leap_controller_peer_owned_by_other(

                         entry,

                         gw->session_hub.config.default_peer.mgmt.controller_mac) != 0)

            {

                printf(

                    LEAP_TS_FMT LEAP_ANSI_WARN

                    "Gateway: mapping %u skipped — foreign owner on %02x:%02x:%02x:%02x:%02x:%02x"

                    LEAP_ANSI_RESET "\n",

                    leap_rtems_uptime_str(),

                    mapping_index,

                    map->leap_mac[0],

                    map->leap_mac[1],

                    map->leap_mac[2],

                    map->leap_mac[3],

                    map->leap_mac[4],

                    map->leap_mac[5]);

                return LEAP_CTRL_STACK_MGMT_ERROR;

            }

        }



        if (use_live_bootstrap)

        {

            printf(

                LEAP_TS_FMT LEAP_ANSI_WARN

                "Gateway: mapping %u live DISC bootstrap for %02x:%02x:%02x:%02x:%02x:%02x"

                LEAP_ANSI_RESET "\n",

                leap_rtems_uptime_str(),

                mapping_index,

                map->leap_mac[0],

                map->leap_mac[1],

                map->leap_mac[2],

                map->leap_mac[3],

                map->leap_mac[4],

                map->leap_mac[5]);



            status = leap_controller_session_hub_bootstrap_peer_live_at_slot(

                &gw->session_hub,

                &gw->controller_io,

                map->leap_mac,

                (int)mapping_index);

        }

        else

        {

            gateway_build_hello_from_entry(entry, profile_id, &hello);



            status = leap_controller_session_hub_bootstrap_peer_at_slot(

                &gw->session_hub,

                &gw->controller_io,

                map->leap_mac,

                &hello,

                (int)mapping_index);

        }

    }



    if (status != LEAP_CTRL_STACK_OK ||

        leap_controller_session_hub_is_op(&gw->session_hub, (int)mapping_index) == 0)

    {

        LeapControllerStack*     failed_stack;
        LeapControllerStackPhase phase = LEAP_CTRL_STACK_IDLE;

        failed_stack = leap_controller_session_hub_stack(
            &gw->session_hub,
            (int)mapping_index);
        if (failed_stack != NULL)
        {
            phase = leap_controller_stack_get_phase(failed_stack);
        }

        printf(

            LEAP_TS_FMT LEAP_ANSI_WARN

            "Gateway: LEAP bootstrap failed mapping %u (%s, phase=%s)" LEAP_ANSI_RESET "\n",

            leap_rtems_uptime_str(),

            mapping_index,

            gateway_stack_status_name(status),

            gateway_stack_phase_name(phase));

        if (entry != NULL)

        {

            if (gateway_mac_is_zero(entry->active_owner_mac))

            {

                printf(

                    LEAP_TS_FMT "  peer HELLO: state=0x%04X profile=0x%08X owner=none"

                    LEAP_ANSI_RESET "\n",

                    leap_rtems_uptime_str(),

                    (unsigned)entry->device_state,

                    (unsigned)entry->active_profile_id);

            }

            else

            {

                printf(

                    LEAP_TS_FMT

                    "  peer HELLO: state=0x%04X profile=0x%08X owner=%02x:%02x:%02x:%02x:%02x:%02x"

                    LEAP_ANSI_RESET "\n",

                    leap_rtems_uptime_str(),

                    (unsigned)entry->device_state,

                    (unsigned)entry->active_profile_id,

                    entry->active_owner_mac[0],

                    entry->active_owner_mac[1],

                    entry->active_owner_mac[2],

                    entry->active_owner_mac[3],

                    entry->active_owner_mac[4],

                    entry->active_owner_mac[5]);

            }

        }

    }



    return status;

}



static void

gateway_retry_down_mappings(LeapGatewayRuntime* gw)

{

    unsigned mapping_count;

    unsigned n;



    if (gw == NULL)

    {

        return;

    }



    if (gw->leap_session.connect_suppressed != 0)

    {

        return;

    }



    mapping_count = gw->config.bridge.mapping_count;

    if (mapping_count == 0u || mapping_count > LEAP_CTRL_MAX_PEERS)

    {

        return;

    }



    for (n = 0u; n < mapping_count; ++n)

    {

        unsigned                  i;

        LeapControllerStackStatus status;



        i = (g_bootstrap_rr_index + n) % mapping_count;



        if (!mapping_slot_enabled(gw, i))

        {

            gw->leap_session.reconnect_ticks[i] = 0u;

            gw->leap_session.pd_miss_count[i] = 0u;

            continue;

        }



        if (leap_controller_session_hub_is_op(&gw->session_hub, (int)i) != 0)

        {

            continue;

        }



        if (gw->leap_session.reconnect_ticks[i] > 0u)

        {

            gw->leap_session.reconnect_ticks[i]--;

            continue;

        }



        g_bootstrap_rr_index = (i + 1u) % mapping_count;



        status = gateway_bootstrap_mapping_slot(

            gw,

            i,

            &gw->config.bridge.mappings[i],

            LEAP_GATEWAY_RECONNECT_PROBE_MS);

        if (status == LEAP_CTRL_STACK_OK &&

            leap_controller_session_hub_is_op(&gw->session_hub, (int)i) != 0)

        {

            gateway_sync_pd_outputs_from_bridge(gw, i);

            gw->leap_session.pd_miss_count[i] = 0u;

            gw->leap_session.reconnect_ticks[i] = 0u;

            printf(

                LEAP_TS_FMT LEAP_ANSI_OK

                "Gateway: LEAP peer mapping %u recovered" LEAP_ANSI_RESET "\n",

                leap_rtems_uptime_str(),

                i);

        }

        else

        {

            gw->leap_session.reconnect_ticks[i] = LEAP_GATEWAY_RECONNECT_TICKS;

        }



        return;

    }

}



static void

gateway_release_unused_mapping_slots(LeapGatewayRuntime* gw)

{

    unsigned slot;



    if (gw == NULL)

    {

        return;

    }



    for (slot = 0u; slot < LEAP_CTRL_MAX_PEERS; ++slot)

    {

        if (gw->session_hub.slots[slot].in_use == 0)

        {

            continue;

        }



        if (!mapping_slot_enabled(gw, slot))

        {

            (void)leap_controller_session_hub_release(

                &gw->session_hub,

                (int)slot,

                &gw->controller_io);

        }

    }

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

    if (leap_gateway_leap_session_active(&g_gateway))

    {

        g_gateway.discover_active = 0;

        printf(

            LEAP_TS_FMT LEAP_ANSI_WARN

            "Gateway: LEAP discover skipped while owner sessions are active" LEAP_ANSI_RESET "\n",

            leap_rtems_uptime_str());

        return;

    }

    g_gateway.discover_active = 1;

    (void)leap_controller_peer_table_discover(

        &g_gateway.peer_table,

        &g_gateway.controller_io,

        scan_ms);

    printf(

        LEAP_TS_FMT LEAP_ANSI_INFO

        "Gateway: LEAP discover finished (%u peer(s))" LEAP_ANSI_RESET "\n",

        leap_rtems_uptime_str(),

        g_gateway.peer_table.count);
    g_gateway.discover_active = 0;

}



int

leap_gateway_leap_session_active(const LeapGatewayRuntime* gw)

{

    if (gw == NULL)

    {

        return 0;

    }



    return (leap_controller_session_hub_count_op_peers(&gw->session_hub) > 0u) ? 1 : 0;

}



void

leap_gateway_leap_session_disconnect(LeapGatewayRuntime* gw)

{

    if (gw == NULL)

    {

        return;

    }



    leap_controller_session_hub_release_all(&gw->session_hub, &gw->controller_io);

    gw->leap_session.op_peer_count   = 0u;

    gw->leap_session.connect_pending = 0;

    gw->leap_session.last_status     = LEAP_CTRL_STACK_OK;

    gw->leap_session.retry_ticks     = 0u;

    memset(gw->leap_session.pd_miss_count, 0, sizeof(gw->leap_session.pd_miss_count));

    memset(gw->leap_session.reconnect_ticks, 0, sizeof(gw->leap_session.reconnect_ticks));

    gw->bridge.leap_comm_ok          = 0;

}



void

leap_gateway_leap_session_request_disconnect(LeapGatewayRuntime* gw)

{

    if (gw == NULL)

    {

        return;

    }



    leap_gateway_leap_session_disconnect(gw);

    gw->leap_session.connect_suppressed = 1;

}



void

leap_gateway_leap_session_request_connect(LeapGatewayRuntime* gw)

{

    if (gw == NULL)

    {

        return;

    }



    gw->leap_session.connect_suppressed = 0;

    gw->leap_session.connect_pending = 1;

    gw->leap_session.retry_ticks     = 0u;

    memset(gw->leap_session.reconnect_ticks, 0, sizeof(gw->leap_session.reconnect_ticks));

}



void

leap_gateway_leap_session_request_auto_connect(LeapGatewayRuntime* gw)

{

    unsigned i;



    if (gw == NULL)

    {

        return;

    }



    gw->leap_session.connect_suppressed = 0;

    for (i = 0u; i < gw->config.bridge.mapping_count; ++i)

    {

        if (mapping_slot_enabled(gw, i))

        {

            leap_gateway_leap_session_request_connect(gw);

            return;

        }

    }

}



int

leap_gateway_leap_session_connect(LeapGatewayRuntime* gw)

{

    unsigned                     i;

    unsigned                     op_count = 0u;

    LeapControllerStackStatus    last_status = LEAP_CTRL_STACK_OK;

    int                          any_enabled = 0;



    if (gw == NULL)

    {

        return 0;

    }



    gateway_drain_leap_rx(&gw->transport);

    gateway_release_unused_mapping_slots(gw);



    for (i = 0u; i < gw->config.bridge.mapping_count; ++i)

    {

        const LeapEipBridgeMapping* map = &gw->config.bridge.mappings[i];

        LeapControllerStackStatus   status;



        if (!mapping_slot_enabled(gw, i))

        {

            continue;

        }



        any_enabled = 1;

        status = gateway_bootstrap_mapping_slot(

            gw, i, map, LEAP_GATEWAY_BOOTSTRAP_PROBE_MS);

        last_status = status;



        if (status == LEAP_CTRL_STACK_OK &&

            leap_controller_session_hub_is_op(&gw->session_hub, (int)i) != 0)

        {

            gateway_sync_pd_outputs_from_bridge(gw, i);

            if (i < LEAP_CTRL_MAX_PEERS)

            {

                gw->leap_session.pd_miss_count[i] = 0u;

                gw->leap_session.reconnect_ticks[i] = 0u;

            }

            op_count++;

        }

        else if (i < LEAP_CTRL_MAX_PEERS)

        {

            gw->leap_session.reconnect_ticks[i] = LEAP_GATEWAY_RECONNECT_TICKS;

        }

    }



    gw->leap_session.last_status   = last_status;

    gw->leap_session.op_peer_count = op_count;



    if (op_count > 0u)

    {

        printf(

            LEAP_TS_FMT LEAP_ANSI_OK

            "Gateway: LEAP owner session OP with %u mapped peer(s)" LEAP_ANSI_RESET "\n",

            leap_rtems_uptime_str(),

            op_count);

        return 1;

    }



    if (any_enabled)

    {

        gw->bridge.leap_comm_ok = 0;

    }

    else

    {

        leap_gateway_leap_session_disconnect(gw);

    }



    return 0;

}



static void

gateway_leap_session_loop(void)

{

    LeapPdControllerIo pd_io;



    leap_gateway_pd_io_init(&pd_io, &g_gateway.transport);



    for (;;)

    {

        unsigned       delay_ms;

        unsigned       i;

        int            any_comm_ok = 0;



        if (g_leap_session_stop != 0)

        {

            break;

        }



        if (g_gateway.config_dirty != 0)

        {

            g_gateway.config_dirty = 0;

            leap_gateway_leap_session_disconnect(&g_gateway);

            gateway_sync_hub_config(&g_gateway);

            g_gateway.leap_session.connect_pending = 1;

        }



        gateway_run_pending_discover();



        if (g_gateway.leap_session.connect_pending != 0)

        {

            g_gateway.leap_session.connect_pending = 0;

            (void)leap_gateway_leap_session_connect(&g_gateway);

        }



        gateway_retry_down_mappings(&g_gateway);



        gateway_push_dirty_outputs(&g_gateway, &pd_io);



        if (leap_gateway_leap_session_active(&g_gateway))

        {

            for (i = 0u; i < g_gateway.config.bridge.mapping_count; ++i)

            {

                LeapPdControllerStatus       pd_status;

                LeapControllerStack*         stack;

                const LeapPdControllerStats* stats_before;

                uint64_t                     replies_before;



                if (!mapping_slot_enabled(&g_gateway, i))

                {

                    continue;

                }



                if (leap_controller_session_hub_is_op(&g_gateway.session_hub, (int)i) == 0)

                {

                    continue;

                }



                stack = leap_controller_session_hub_stack(&g_gateway.session_hub, (int)i);

                stats_before = (stack != NULL) ? leap_pd_controller_stats(&stack->pd) : NULL;

                replies_before = (stats_before != NULL) ? stats_before->exchange_replies : 0u;



                gateway_sync_pd_outputs_from_bridge(&g_gateway, i);



                pd_status = leap_controller_session_hub_run_one_cycle(

                    &g_gateway.session_hub,

                    (int)i,

                    &pd_io,

                    (volatile int*)&g_leap_session_stop);



                if (stack != NULL &&

                    leap_controller_stack_get_phase(stack) != LEAP_CTRL_STACK_OP)

                {

                    gateway_mark_mapping_down(&g_gateway, i, &pd_io);

                    continue;

                }



                {

                    const LeapPdControllerStats* stats_after;

                    uint64_t                     replies_after;

                    int                          got_exchange_reply;



                    stats_after = (stack != NULL) ? leap_pd_controller_stats(&stack->pd) : NULL;

                    replies_after = (stats_after != NULL) ? stats_after->exchange_replies : replies_before;

                    got_exchange_reply =

                        (pd_status == LEAP_PD_CTRL_OK) && (replies_after > replies_before);



                    gateway_sync_bridge_from_pd(

                        i,

                        &pd_io,

                        got_exchange_reply);



                    if (got_exchange_reply)

                    {

                        if (i < LEAP_CTRL_MAX_PEERS)

                        {

                            g_gateway.leap_session.pd_miss_count[i] = 0u;

                            g_gateway.leap_session.reconnect_ticks[i] = 0u;

                        }

                        any_comm_ok = 1;

                    }

                    else if (i < LEAP_CTRL_MAX_PEERS)

                    {

                        g_gateway.leap_session.pd_miss_count[i]++;

                        if (g_gateway.leap_session.pd_miss_count[i] >= LEAP_GATEWAY_PD_MISS_LIMIT)

                        {

                            gateway_mark_mapping_down(

                                &g_gateway,

                                i,

                                &pd_io);

                        }

                    }

                }

            }



            g_gateway.bridge.leap_comm_ok = any_comm_ok;



            g_gateway.leap_session.op_peer_count =

                leap_controller_session_hub_count_op_peers(&g_gateway.session_hub);



            delay_ms = g_gateway.config.cyclic_ms;

        }

        else

        {

            delay_ms = 50u;

        }



        gateway_session_sleep_ms(delay_ms);

    }

}

#if defined(__rtems__)

static void

leap_gateway_leap_session_task(rtems_task_argument ignored)

{

    (void)ignored;

    gateway_leap_session_loop();

    rtems_task_exit();

}

#else

static void*

leap_gateway_leap_session_thread(void* arg)

{

    (void)arg;

    gateway_leap_session_loop();

    return NULL;

}

#endif



int

leap_gateway_leap_session_start_task(void)

{

#if !defined(__rtems__)

    if (g_leap_session_thread_started)

    {

        return 0;

    }



    if (pthread_create(

            &g_leap_session_thread,

            NULL,

            leap_gateway_leap_session_thread,

            NULL) != 0)

    {

        printf(

            LEAP_TS_FMT LEAP_ANSI_ERR

            "Gateway: LEAP session thread create failed" LEAP_ANSI_RESET "\n",

            leap_rtems_uptime_str());

        return -1;

    }



    g_leap_session_thread_started = 1;

    printf(

        LEAP_TS_FMT LEAP_ANSI_OK "Gateway: LEAP session task started" LEAP_ANSI_RESET "\n",

        leap_rtems_uptime_str());

    return 0;

#else

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

#endif /* __rtems__ */

}


