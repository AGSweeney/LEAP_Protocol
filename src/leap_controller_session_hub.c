/*
 * leap_controller_session_hub.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_controller_session_hub.h"

#include <string.h>

static int leap_ctrl_hub_mac_is_zero(const uint8_t* mac)
{
    static const uint8_t k_zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };

    if (mac == NULL)
    {
        return 1;
    }

    return (memcmp(mac, k_zero, 6) == 0) ? 1 : 0;
}

static int leap_ctrl_hub_alloc_slot(LeapControllerSessionHub* hub)
{
    unsigned i;

    if (hub == NULL)
    {
        return -1;
    }

    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        if (hub->slots[i].in_use == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

static void leap_ctrl_hub_free_slot(LeapControllerSessionHub* hub, int slot)
{
    if (hub == NULL || slot < 0 || (unsigned)slot >= LEAP_CTRL_MAX_PEERS)
    {
        return;
    }

    if (hub->slots[slot].in_use == 0)
    {
        return;
    }

    memset(&hub->slots[slot], 0, sizeof(hub->slots[slot]));
    if (hub->active_count > 0u)
    {
        hub->active_count--;
    }
}

void leap_controller_session_hub_init(
    LeapControllerSessionHub*             hub,
    const LeapControllerSessionHubConfig* config)
{
    if (hub == NULL)
    {
        return;
    }

    memset(hub, 0, sizeof(*hub));

    if (config != NULL)
    {
        hub->config = *config;
    }
    else
    {
        hub->config.default_peer.frame_sequence.enforce_session_match = 1;
        hub->config.default_peer.pd.validate_exchange_reply         = 1;
        hub->config.default_peer.pd.enforce_reply_frame_age         = 1;
        hub->config.skip_foreign_owned_peers                        = 1;
    }
}

void leap_controller_session_hub_reset(LeapControllerSessionHub* hub)
{
    LeapControllerSessionHubConfig saved;

    if (hub == NULL)
    {
        return;
    }

    saved = hub->config;
    leap_controller_session_hub_init(hub, &saved);
}

unsigned leap_controller_session_hub_active_count(
    const LeapControllerSessionHub* hub)
{
    if (hub == NULL)
    {
        return 0u;
    }

    return hub->active_count;
}

int leap_controller_session_hub_find(
    const LeapControllerSessionHub* hub,
    const uint8_t*                  peer_mac)
{
    unsigned i;

    if (hub == NULL || peer_mac == NULL || leap_ctrl_hub_mac_is_zero(peer_mac))
    {
        return -1;
    }

    for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
    {
        if (hub->slots[i].in_use != 0 &&
            memcmp(hub->slots[i].peer_mac, peer_mac, 6) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

LeapControllerStack* leap_controller_session_hub_stack(
    LeapControllerSessionHub* hub,
    int                       slot)
{
    if (hub == NULL || slot < 0 || (unsigned)slot >= LEAP_CTRL_MAX_PEERS ||
        hub->slots[slot].in_use == 0)
    {
        return NULL;
    }

    return &hub->slots[slot].stack;
}

const uint8_t* leap_controller_session_hub_peer_mac(
    const LeapControllerSessionHub* hub,
    int                             slot)
{
    if (hub == NULL || slot < 0 || (unsigned)slot >= LEAP_CTRL_MAX_PEERS ||
        hub->slots[slot].in_use == 0)
    {
        return NULL;
    }

    return hub->slots[slot].peer_mac;
}

int leap_controller_session_hub_is_op(
    const LeapControllerSessionHub* hub,
    int                             slot)
{
    const LeapControllerStack* stack;

    stack = leap_controller_session_hub_stack((LeapControllerSessionHub*)hub, slot);
    if (stack == NULL)
    {
        return 0;
    }

    return (leap_controller_stack_get_phase(stack) == LEAP_CTRL_STACK_OP) ? 1 : 0;
}

LeapControllerStackStatus leap_controller_session_hub_bootstrap_peer(
    LeapControllerSessionHub*     hub,
    const LeapControllerStackIo*  io,
    const uint8_t*                peer_mac,
    const LeapHelloReply*         hello_reply,
    int*                          slot_out)
{
    LeapControllerStackStatus status;
    int                       slot;

    if (hub == NULL || io == NULL || peer_mac == NULL ||
        leap_ctrl_hub_mac_is_zero(peer_mac))
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    if (leap_controller_session_hub_find(hub, peer_mac) >= 0)
    {
        return LEAP_CTRL_STACK_UNEXPECTED_REPLY;
    }

    slot = leap_ctrl_hub_alloc_slot(hub);
    if (slot < 0)
    {
        return LEAP_CTRL_STACK_ABORTED;
    }

    leap_controller_stack_init(
        &hub->slots[slot].stack,
        &hub->config.default_peer);
    memcpy(hub->slots[slot].peer_mac, peer_mac, 6);
    hub->slots[slot].in_use = 1;
    hub->active_count++;

    status = leap_controller_stack_bootstrap_peer(
        &hub->slots[slot].stack,
        io,
        peer_mac,
        hello_reply);
    if (status != LEAP_CTRL_STACK_OK)
    {
        leap_ctrl_hub_free_slot(hub, slot);
        return status;
    }

    if (slot_out != NULL)
    {
        *slot_out = slot;
    }

    return LEAP_CTRL_STACK_OK;
}

LeapControllerSessionHubStatus leap_controller_session_hub_bootstrap_table(
    LeapControllerSessionHub*      hub,
    const LeapControllerStackIo*   io,
    const LeapControllerPeerTable* table,
    unsigned*                      bootstrapped_count)
{
    unsigned i;
    unsigned ok_count = 0u;

    if (hub == NULL || io == NULL || table == NULL)
    {
        return LEAP_CTRL_HUB_INVALID_ARG;
    }

    for (i = 0u; i < table->count; i++)
    {
        const LeapControllerPeerEntry* entry =
            leap_controller_peer_table_get(table, i);
        LeapHelloReply                   hello;
        int                              slot;

        if (entry == NULL || entry->reachable == 0)
        {
            continue;
        }

        if (hub->config.skip_foreign_owned_peers != 0 &&
            leap_controller_peer_owned_by_other(
                entry,
                hub->config.default_peer.mgmt.controller_mac) != 0)
        {
            continue;
        }

        if (leap_controller_session_hub_find(hub, entry->mac) >= 0)
        {
            ok_count++;
            continue;
        }

        memset(&hello, 0, sizeof(hello));
        hello.current_state      = entry->device_state;
        hello.active_profile_id  = entry->active_profile_id;
        hello.default_profile_id = entry->default_profile_id;

        if (leap_controller_session_hub_bootstrap_peer(
                hub,
                io,
                entry->mac,
                &hello,
                &slot) == LEAP_CTRL_STACK_OK)
        {
            ok_count++;
        }
    }

    if (bootstrapped_count != NULL)
    {
        *bootstrapped_count = ok_count;
    }

    return (ok_count > 0u) ? LEAP_CTRL_HUB_OK : LEAP_CTRL_HUB_NO_ACTIVE_PEERS;
}

LeapControllerStackStatus leap_controller_session_hub_on_frame(
    LeapControllerSessionHub*    hub,
    const uint8_t*               src_mac,
    const LeapFrameView*         view,
    LeapControllerStackEvent*    event,
    int*                         slot_out)
{
    int                       slot;
    LeapControllerStackStatus status;

    if (hub == NULL || src_mac == NULL || view == NULL)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    slot = leap_controller_session_hub_find(hub, src_mac);
    if (slot < 0)
    {
        if (event != NULL)
        {
            event->status = LEAP_CTRL_STACK_IGNORED;
        }
        if (slot_out != NULL)
        {
            *slot_out = -1;
        }
        return LEAP_CTRL_STACK_IGNORED;
    }

    status = leap_controller_stack_on_frame(
        &hub->slots[slot].stack,
        src_mac,
        view,
        event);

    if (slot_out != NULL)
    {
        *slot_out = slot;
    }

    return status;
}

LeapControllerStackStatus leap_controller_session_hub_release(
    LeapControllerSessionHub*    hub,
    int                          slot,
    const LeapControllerStackIo* io)
{
    LeapControllerStackStatus status;

    if (hub == NULL || slot < 0 || (unsigned)slot >= LEAP_CTRL_MAX_PEERS ||
        hub->slots[slot].in_use == 0)
    {
        return LEAP_CTRL_STACK_INVALID_ARG;
    }

    status = leap_controller_stack_release(
        &hub->slots[slot].stack,
        io);
    leap_ctrl_hub_free_slot(hub, slot);
    return status;
}

void leap_controller_session_hub_release_all(
    LeapControllerSessionHub*    hub,
    const LeapControllerStackIo* io)
{
    int slot;

    if (hub == NULL)
    {
        return;
    }

    for (slot = (int)LEAP_CTRL_MAX_PEERS - 1; slot >= 0; slot--)
    {
        if (hub->slots[slot].in_use != 0)
        {
            (void)leap_controller_session_hub_release(hub, slot, io);
        }
    }
}

LeapPdControllerStatus leap_controller_session_hub_run_one_cycle(
    LeapControllerSessionHub* hub,
    int                       slot,
    const LeapPdControllerIo* io,
    volatile int*             stop_flag)
{
    LeapControllerStack* stack;

    if (hub == NULL || io == NULL || stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    stack = leap_controller_session_hub_stack(hub, slot);
    if (stack == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (leap_controller_stack_get_phase(stack) != LEAP_CTRL_STACK_OP)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    return leap_pd_controller_run_one_cycle(
        &stack->pd,
        &stack->mgmt,
        io,
        hub->slots[slot].peer_mac,
        stop_flag,
        0);
}

LeapPdControllerStatus leap_controller_session_hub_run_round_robin(
    LeapControllerSessionHub* hub,
    const LeapPdControllerIo* io,
    volatile int*             stop_flag)
{
    LeapPdControllerStatus status;
    unsigned                 i;
    int                      ran_any;

    if (hub == NULL || io == NULL || stop_flag == NULL)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    if (hub->active_count == 0u)
    {
        return LEAP_PD_CTRL_INVALID_ARG;
    }

    while (*stop_flag == 0)
    {
        ran_any = 0;

        for (i = 0u; i < LEAP_CTRL_MAX_PEERS; i++)
        {
            if (hub->slots[i].in_use == 0)
            {
                continue;
            }

            if (leap_controller_stack_get_phase(&hub->slots[i].stack) !=
                LEAP_CTRL_STACK_OP)
            {
                continue;
            }

            status = leap_controller_session_hub_run_one_cycle(
                hub,
                (int)i,
                io,
                stop_flag);
            if (status == LEAP_PD_CTRL_STOPPED)
            {
                return LEAP_PD_CTRL_OK;
            }
            if (status != LEAP_PD_CTRL_OK)
            {
                return status;
            }

            ran_any = 1;
        }

        if (ran_any == 0)
        {
            return LEAP_PD_CTRL_INVALID_ARG;
        }
    }

    return LEAP_PD_CTRL_OK;
}
