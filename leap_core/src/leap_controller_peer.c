/*
 * leap_controller_peer.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_controller_peer.h"

#include "leap/leap_disc_controller.h"
#include "leap/leap_frame.h"

#include <stdio.h>
#include <string.h>

#define LEAP_CTRL_PEER_RX_BUF       1600u
#define LEAP_CTRL_PEER_RECV_SLICE   100
#define LEAP_CTRL_PEER_HELLO_PERIOD 250000u

static const uint8_t k_bcast[6] = LEAP_CTRL_STACK_BROADCAST_MAC;

static uint64_t leap_ctrl_peer_now_us(const LeapControllerStackIo* io)
{
    if (io != NULL && io->monotonic_us != NULL)
    {
        return io->monotonic_us(io->user_ctx);
    }

    return 0u;
}

static LeapControllerPeerStatus leap_ctrl_peer_table_upsert(
    LeapControllerPeerTable* table,
    const uint8_t*           mac,
    const LeapHelloReply*    hello)
{
    unsigned i;

    if (table == NULL || mac == NULL || hello == NULL)
    {
        return LEAP_CTRL_PEER_INVALID_ARG;
    }

    for (i = 0u; i < table->count; i++)
    {
        if (memcmp(table->peers[i].mac, mac, 6) == 0)
        {
            table->peers[i].active_profile_id  = hello->active_profile_id;
            table->peers[i].default_profile_id = hello->default_profile_id;
            table->peers[i].device_state       = hello->current_state;
            memcpy(table->peers[i].active_owner_mac, hello->active_owner_mac, 6);
            table->peers[i].reachable          = 1;
            return LEAP_CTRL_PEER_OK;
        }
    }

    if (table->count >= LEAP_CTRL_MAX_PEERS)
    {
        return LEAP_CTRL_PEER_TABLE_FULL;
    }

    memcpy(table->peers[table->count].mac, mac, 6);
    table->peers[table->count].active_profile_id  = hello->active_profile_id;
    table->peers[table->count].default_profile_id = hello->default_profile_id;
    table->peers[table->count].device_state       = hello->current_state;
    memcpy(table->peers[table->count].active_owner_mac, hello->active_owner_mac, 6);
    table->peers[table->count].reachable          = 1;
    table->count++;
    return LEAP_CTRL_PEER_OK;
}

static LeapControllerPeerStatus leap_ctrl_peer_send_hello(
    const LeapControllerStackIo* io,
    const uint8_t*                 dst_mac,
    uint8_t                        flags)
{
    uint8_t payload[64];
    size_t  payload_length;

    if (io == NULL || dst_mac == NULL)
    {
        return LEAP_CTRL_PEER_INVALID_ARG;
    }

    payload_length = leap_disc_controller_build_hello(payload, sizeof(payload));
    if (payload_length == 0u)
    {
        return LEAP_CTRL_PEER_SEND_FAILED;
    }

    if (io->send_frame(
            io->user_ctx,
            dst_mac,
            flags,
            (uint16_t)LEAP_SERVICE_DISC,
            LEAP_DISC_HELLO,
            0u,
            1u,
            0u,
            payload,
            payload_length) != 0)
    {
        return LEAP_CTRL_PEER_SEND_FAILED;
    }

    return LEAP_CTRL_PEER_OK;
}

static int leap_ctrl_peer_ingest_hello_reply(
    LeapControllerPeerTable* table,
    const uint8_t*           src_mac,
    const uint8_t*           rx,
    size_t                   rx_length,
    unsigned                 min_peers)
{
    LeapFrameView  view;
    LeapHelloReply hello;

    if (table == NULL || src_mac == NULL || rx == NULL)
    {
        return 0;
    }

    if (leap_frame_parse(rx, rx_length, &view) != LEAP_FRAME_OK)
    {
        return 0;
    }

    if (view.header.service_id != (uint16_t)LEAP_SERVICE_DISC ||
        view.header.message_type != LEAP_DISC_HELLO_REPLY)
    {
        return 0;
    }

    if (leap_disc_controller_on_hello_reply(
            view.payload,
            view.payload_length,
            &hello) != LEAP_DISC_CTRL_OK)
    {
        return 0;
    }

    (void)leap_ctrl_peer_table_upsert(table, src_mac, &hello);

    return (min_peers > 0u && table->count >= min_peers) ? 1 : 0;
}

int leap_controller_peer_parse_mac(const char* text, uint8_t mac_out[6])
{
    unsigned values[6];
    int      matched;

    if (text == NULL || mac_out == NULL)
    {
        return 0;
    }

    matched = sscanf(
        text,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        &values[0],
        &values[1],
        &values[2],
        &values[3],
        &values[4],
        &values[5]);
    if (matched != 6)
    {
        matched = sscanf(
            text,
            "%02x-%02x-%02x-%02x-%02x-%02x",
            &values[0],
            &values[1],
            &values[2],
            &values[3],
            &values[4],
            &values[5]);
    }

    if (matched != 6)
    {
        return 0;
    }

    mac_out[0] = (uint8_t)values[0];
    mac_out[1] = (uint8_t)values[1];
    mac_out[2] = (uint8_t)values[2];
    mac_out[3] = (uint8_t)values[3];
    mac_out[4] = (uint8_t)values[4];
    mac_out[5] = (uint8_t)values[5];
    return 1;
}

void leap_controller_peer_table_init(LeapControllerPeerTable* table)
{
    if (table == NULL)
    {
        return;
    }

    memset(table, 0, sizeof(*table));
}

LeapControllerPeerStatus leap_controller_peer_table_probe_peer(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    const uint8_t*               peer_mac,
    int                          timeout_ms)
{
    uint8_t  rx[LEAP_CTRL_PEER_RX_BUF];
    uint8_t  src_mac[6];
    size_t   rx_length;
    LeapFrameView view;
    uint64_t start_us;
    uint64_t end_us;
    int      recv_timeout_ms;

    if (table == NULL || peer_mac == NULL)
    {
        return LEAP_CTRL_PEER_INVALID_ARG;
    }

    if (io == NULL || io->send_frame == NULL || io->recv_frame == NULL)
    {
        return LEAP_CTRL_PEER_IO_MISSING;
    }

    if (timeout_ms <= 0)
    {
        timeout_ms = LEAP_CTRL_PEER_PROBE_TIMEOUT_MS;
    }

    if (leap_ctrl_peer_send_hello(io, peer_mac, 0u) != LEAP_CTRL_PEER_OK)
    {
        return LEAP_CTRL_PEER_SEND_FAILED;
    }

    start_us = leap_ctrl_peer_now_us(io);
    if (start_us != 0u)
    {
        end_us = start_us + ((uint64_t)timeout_ms * 1000u);
    }
    else
    {
        end_us = 0u;
    }

    for (;;)
    {
        if (end_us != 0u)
        {
            uint64_t now_us = leap_ctrl_peer_now_us(io);

            if (now_us >= end_us)
            {
                break;
            }

            recv_timeout_ms = (int)((end_us - now_us) / 1000u);
            if (recv_timeout_ms <= 0)
            {
                recv_timeout_ms = 1;
            }
            if (recv_timeout_ms > LEAP_CTRL_PEER_RECV_SLICE)
            {
                recv_timeout_ms = LEAP_CTRL_PEER_RECV_SLICE;
            }
        }
        else
        {
            recv_timeout_ms = timeout_ms;
        }

        rx_length = 0u;
        if (io->recv_frame(
                io->user_ctx,
                src_mac,
                rx,
                sizeof(rx),
                &rx_length,
                &view,
                recv_timeout_ms) != 0)
        {
            if (end_us == 0u)
            {
                break;
            }
            continue;
        }

        if (memcmp(src_mac, peer_mac, 6) != 0)
        {
            continue;
        }

        if (leap_ctrl_peer_ingest_hello_reply(table, src_mac, rx, rx_length, 0u) != 0)
        {
            break;
        }

        if (leap_controller_peer_table_find(table, peer_mac) >= 0)
        {
            return LEAP_CTRL_PEER_OK;
        }
    }

    if (leap_controller_peer_table_find(table, peer_mac) < 0)
    {
        return LEAP_CTRL_PEER_NOT_FOUND;
    }

    return LEAP_CTRL_PEER_OK;
}

LeapControllerPeerStatus leap_controller_peer_table_discover_ex(
    LeapControllerPeerTable*                table,
    const LeapControllerStackIo*            io,
    const LeapControllerPeerDiscoverConfig* config)
{
    int                        scan_duration_ms;
    unsigned                   min_peers;
    uint8_t                    rx[LEAP_CTRL_PEER_RX_BUF];
    uint8_t                    src_mac[6];
    size_t                     rx_length;
    LeapFrameView              view;
    uint64_t                   start_us;
    uint64_t                   end_us;
    uint64_t                   last_hello_us;
    int                        recv_timeout_ms;

    if (table == NULL)
    {
        return LEAP_CTRL_PEER_INVALID_ARG;
    }

    if (io == NULL || io->send_frame == NULL || io->recv_frame == NULL)
    {
        return LEAP_CTRL_PEER_IO_MISSING;
    }

    scan_duration_ms = LEAP_CTRL_PEER_DISCOVER_DEFAULT_SCAN_MS;
    min_peers        = 0u;

    if (config != NULL)
    {
        scan_duration_ms = config->scan_duration_ms;
        min_peers        = config->min_peers;
    }

    if (scan_duration_ms < 0)
    {
        return LEAP_CTRL_PEER_OK;
    }

    if (scan_duration_ms == 0)
    {
        scan_duration_ms = LEAP_CTRL_PEER_DISCOVER_MIN_SCAN_MS;
    }

    if (leap_ctrl_peer_send_hello(io, k_bcast, LEAP_FLAG_BROADCAST) !=
        LEAP_CTRL_PEER_OK)
    {
        return LEAP_CTRL_PEER_SEND_FAILED;
    }

    start_us      = leap_ctrl_peer_now_us(io);
    last_hello_us = start_us;
    if (start_us != 0u)
    {
        end_us = start_us + ((uint64_t)scan_duration_ms * 1000u);
    }
    else
    {
        end_us = 0u;
    }

    while (1)
    {
        if (end_us != 0u)
        {
            uint64_t now_us = leap_ctrl_peer_now_us(io);

            if (now_us >= end_us)
            {
                break;
            }

            if (now_us != 0u && (now_us - last_hello_us) >= LEAP_CTRL_PEER_HELLO_PERIOD)
            {
                (void)leap_ctrl_peer_send_hello(io, k_bcast, LEAP_FLAG_BROADCAST);
                last_hello_us = now_us;
            }

            recv_timeout_ms = (int)((end_us - now_us) / 1000u);
            if (recv_timeout_ms <= 0)
            {
                recv_timeout_ms = 1;
            }
            if (recv_timeout_ms > LEAP_CTRL_PEER_RECV_SLICE)
            {
                recv_timeout_ms = LEAP_CTRL_PEER_RECV_SLICE;
            }
        }
        else
        {
            recv_timeout_ms = scan_duration_ms;
        }

        rx_length = 0u;
        if (io->recv_frame(
                io->user_ctx,
                src_mac,
                rx,
                sizeof(rx),
                &rx_length,
                &view,
                recv_timeout_ms) != 0)
        {
            if (end_us == 0u)
            {
                break;
            }
            continue;
        }

        if (leap_ctrl_peer_ingest_hello_reply(
                table,
                src_mac,
                rx,
                rx_length,
                min_peers) != 0)
        {
            break;
        }
    }

    return LEAP_CTRL_PEER_OK;
}

LeapControllerPeerStatus leap_controller_peer_table_discover(
    LeapControllerPeerTable*     table,
    const LeapControllerStackIo* io,
    int                          scan_duration_ms)
{
    LeapControllerPeerDiscoverConfig config;

    memset(&config, 0, sizeof(config));
    config.scan_duration_ms = scan_duration_ms;
    config.min_peers        = 0u;

    return leap_controller_peer_table_discover_ex(table, io, &config);
}

int leap_controller_peer_table_find(
    const LeapControllerPeerTable* table,
    const uint8_t*                 mac)
{
    unsigned i;

    if (table == NULL || mac == NULL)
    {
        return -1;
    }

    for (i = 0u; i < table->count; i++)
    {
        if (memcmp(table->peers[i].mac, mac, 6) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}

const LeapControllerPeerEntry* leap_controller_peer_table_get(
    const LeapControllerPeerTable* table,
    unsigned                       index)
{
    if (table == NULL || index >= table->count)
    {
        return NULL;
    }

    return &table->peers[index];
}

int leap_controller_peer_owned_by_other(
    const LeapControllerPeerEntry* entry,
    const uint8_t*                 controller_mac)
{
    static const uint8_t k_zero[6] = { 0u, 0u, 0u, 0u, 0u, 0u };

    if (entry == NULL)
    {
        return 0;
    }

    if (memcmp(entry->active_owner_mac, k_zero, 6) == 0)
    {
        return 0;
    }

    if (controller_mac == NULL)
    {
        return 1;
    }

    return (memcmp(entry->active_owner_mac, controller_mac, 6) != 0) ? 1 : 0;
}
