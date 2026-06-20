/*
 * leap_transport_nb.cpp - LeapRtemsTransport over NetBurner SetCustomNetDoRX.
 *
 * SPDX-License-Identifier: MIT
 */

#include "leap_transport.h"

#include "leap_config.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#define ALLOW_CUSTOM_NET_DO_RX
#include <ethervars.h>
#include <hal.h>
#include <init.h>
#include <netinterface.h>
#include <netrx.h>
#include <nbrtos.h>
#include <predef.h>

struct LeapNbRxFrame
{
    uint8_t  src_mac[6];
    int32_t  interface_number;
    uint16_t ethertype;
    uint16_t payload_len;
    uint8_t  payload[LEAP_MAX_FRAME_BYTES];
};

static constexpr uint8_t kRxQueueDepth = 8U;
static LeapNbRxFrame g_rx_queue[kRxQueueDepth];
static volatile uint8_t g_rx_head = 0U;
static volatile uint8_t g_rx_tail = 0U;
static volatile uint32_t g_rx_drop_count = 0U;
static volatile uint32_t g_rx_callback_count = 0U;
static volatile uint32_t g_rx_match_count = 0U;
static volatile uint32_t g_rx_nonmatch_count = 0U;
static volatile uint32_t g_rx_wrong_interface_count = 0U;
static volatile uint32_t g_tx_frame_count = 0U;
static volatile uint32_t g_tx_failure_count = 0U;
static volatile int32_t g_rx_last_interface = 0;
static volatile int32_t g_rx_last_wrong_interface = 0;
static volatile uint16_t g_rx_last_ethertype = 0U;
static volatile uint16_t g_rx_last_nonmatch_ethertype = 0U;
static int32_t g_bound_interface = 0;
static uint16_t g_ethertype_filter = LEAP_RTEMS_ETHERTYPE;
static volatile int g_hook_registered = 0;
static OS_SEM g_rx_sem(0);

static uint64_t leap_nb_monotonic_us()
{
    uint32_t ticks1;
    uint32_t ticks2;
    uint16_t fraction;
    uint64_t us_per_tick;
    uint64_t fraction_us;

    do
    {
        ticks1 = static_cast<uint32_t>(TimeTick);
        fraction = HalGetTickFraction();
        ticks2 = static_cast<uint32_t>(TimeTick);
    } while (ticks1 != ticks2);

    us_per_tick = 1000000ULL / static_cast<uint64_t>(TICKS_PER_SECOND);
    fraction_us = static_cast<uint64_t>(fraction) * us_per_tick;
    if (HalTickMaxCount > 0U)
    {
        fraction_us /= static_cast<uint64_t>(HalTickMaxCount);
    }
    return static_cast<uint64_t>(ticks1) * us_per_tick + fraction_us;
}

static int leap_custom_net_rx(PoolPtr pp, uint16_t ocount, int if_num)
{
    const uint8_t* frame;
    uint16_t ethertype;
    uint16_t leap_length;
    uint16_t payload_offset = LEAP_ETH_HEADER_BYTES;
    uint8_t next_head;
    LeapNbRxFrame* slot;

    if (pp == nullptr || ocount < LEAP_ETH_HEADER_BYTES)
    {
        return 0;
    }

    g_rx_callback_count++;
    g_rx_last_interface = if_num;

    frame = pp->pData;
    ethertype = static_cast<uint16_t>((static_cast<uint16_t>(frame[12]) << 8) | frame[13]);
    g_rx_last_ethertype = ethertype;

    if (g_bound_interface > 0 && if_num != g_bound_interface)
    {
        g_rx_wrong_interface_count++;
        g_rx_last_wrong_interface = if_num;
    }

    if (ethertype != g_ethertype_filter && ethertype != LEAP_ETHERTYPE_ALT)
    {
        g_rx_nonmatch_count++;
        g_rx_last_nonmatch_ethertype = ethertype;
        return 0;
    }

    USER_ENTER_CRITICAL();
    next_head = static_cast<uint8_t>((g_rx_head + 1U) % kRxQueueDepth);
    if (next_head == g_rx_tail)
    {
        g_rx_drop_count++;
        USER_EXIT_CRITICAL();
        return 1;
    }
    slot = &g_rx_queue[g_rx_head];
    USER_EXIT_CRITICAL();

    slot->interface_number = if_num;
    slot->ethertype = ethertype;
    memcpy(slot->src_mac, &frame[6], 6);

    leap_length = static_cast<uint16_t>(ocount - payload_offset);
    if (leap_length > LEAP_MAX_FRAME_BYTES)
    {
        leap_length = LEAP_MAX_FRAME_BYTES;
    }
    slot->payload_len = leap_length;
    memcpy(slot->payload, frame + payload_offset, leap_length);

    USER_ENTER_CRITICAL();
    g_rx_head = next_head;
    g_rx_match_count++;
    USER_EXIT_CRITICAL();
    (void)g_rx_sem.Post();
    return 1;
}

static int parse_interface_number(const char* ifname)
{
    if (ifname == nullptr || ifname[0] == '\0')
    {
        return 0;
    }

    char* end = nullptr;
    const long value = strtol(ifname, &end, 10);
    if (end == ifname || value <= 0L || value > 8L)
    {
        return 0;
    }
    return static_cast<int>(value);
}

static int interface_exists(int ifn)
{
    return (ifn > 0 && GetInterfaceBlock(ifn) != nullptr) ? 1 : 0;
}

static int resolve_leap_interface(const char* ifname)
{
    int ifn = parse_interface_number(ifname);
    if (ifn > 0 && interface_exists(ifn) != 0)
    {
        return ifn;
    }

    int probe = GetFirstInterface();
    while (probe > 0)
    {
        if (probe > 1 && interface_exists(probe) != 0)
        {
            return probe;
        }
        probe = GetNextInterface(probe);
    }
    probe = GetFirstInterface();
    if (interface_exists(probe) != 0)
    {
        return probe;
    }
    return 0;
}

int leap_rtems_transport_init(LeapRtemsTransport* transport, const char* ifname, uint16_t ethertype)
{
    int ifn;
    MACADR mac;

    if (transport == nullptr)
    {
        return -1;
    }

    memset(transport, 0, sizeof(*transport));
    ifn = resolve_leap_interface(ifname);
    if (ifn <= 0)
    {
        return -1;
    }

    mac = InterfaceMAC(ifn);
    for (uint8_t i = 0U; i < LEAP_RTEMS_MAC_LEN; ++i)
    {
        transport->local_mac[i] = mac.GetByte(i);
    }

    transport->interface_number = ifn;
    transport->ethertype = ethertype;
    transport->link_up = 1;
    snprintf(transport->ifname, sizeof(transport->ifname), "%d", ifn);

    g_bound_interface = ifn;
    g_ethertype_filter = ethertype;
    g_rx_head = 0U;
    g_rx_tail = 0U;
    while (g_rx_sem.Pend(1) == OS_NO_ERR)
    {
    }
    g_rx_drop_count = 0U;
    g_rx_callback_count = 0U;
    g_rx_match_count = 0U;
    g_rx_nonmatch_count = 0U;
    g_rx_wrong_interface_count = 0U;
    g_rx_last_interface = 0;
    g_rx_last_wrong_interface = 0;
    g_rx_last_ethertype = 0U;
    g_rx_last_nonmatch_ethertype = 0U;
    g_tx_frame_count = 0U;
    g_tx_failure_count = 0U;

    if (g_hook_registered == 0)
    {
        SetCustomNetDoRX(leap_custom_net_rx);
        g_hook_registered = 1;
    }

    return 0;
}

int leap_rtems_transport_init_auto(LeapRtemsTransport* transport, uint16_t ethertype)
{
    static const char* const candidates[] = { "2", "1", nullptr };
    for (size_t i = 0; candidates[i] != nullptr; ++i)
    {
        if (leap_rtems_transport_init(transport, candidates[i], ethertype) == 0)
        {
            return 0;
        }
    }
    return -1;
}

void leap_rtems_transport_close(LeapRtemsTransport* transport)
{
    if (transport == nullptr)
    {
        return;
    }
    transport->link_up = 0;
}

int leap_rtems_transport_recv(
    LeapRtemsTransport* transport,
    uint8_t*            src_mac_out,
    uint8_t*            payload_out,
    size_t              payload_capacity,
    size_t*             payload_len_out,
    int                 timeout_ms)
{
    const uint64_t start_us = leap_nb_monotonic_us();
    const uint64_t timeout_us =
        (timeout_ms > 0) ? (static_cast<uint64_t>(timeout_ms) * 1000ULL) : 0ULL;
    const uint64_t deadline_us = start_us + timeout_us;
    uint16_t wait_ticks;

    (void)transport;

    if (payload_len_out == nullptr)
    {
        return -1;
    }

    for (;;)
    {
        uint8_t slot_index;

        USER_ENTER_CRITICAL();
        if (g_rx_tail != g_rx_head)
        {
            slot_index = g_rx_tail;
            USER_EXIT_CRITICAL();

            const LeapNbRxFrame* slot = &g_rx_queue[slot_index];
            if (slot->payload_len > payload_capacity)
            {
                (void)g_rx_sem.PendNoWait();
                USER_ENTER_CRITICAL();
                if (g_rx_tail == slot_index)
                {
                    g_rx_tail = static_cast<uint8_t>((g_rx_tail + 1U) % kRxQueueDepth);
                }
                g_rx_drop_count++;
                USER_EXIT_CRITICAL();
                continue;
            }

            (void)g_rx_sem.PendNoWait();
            if (src_mac_out != nullptr)
            {
                memcpy(src_mac_out, slot->src_mac, 6);
            }
            if (payload_out != nullptr && slot->payload_len > 0U)
            {
                memcpy(payload_out, slot->payload, slot->payload_len);
            }
            *payload_len_out = slot->payload_len;
            USER_ENTER_CRITICAL();
            if (g_rx_tail == slot_index)
            {
                g_rx_tail = static_cast<uint8_t>((g_rx_tail + 1U) % kRxQueueDepth);
            }
            USER_EXIT_CRITICAL();
            return 0;
        }
        USER_EXIT_CRITICAL();

        if (timeout_ms <= 0)
        {
            return -1;
        }

        if (leap_nb_monotonic_us() >= deadline_us)
        {
            return -1;
        }

        wait_ticks = 1U;
        if (timeout_ms > 0)
        {
            const uint64_t now_us = leap_nb_monotonic_us();
            uint64_t remaining_us = (deadline_us > now_us) ? (deadline_us - now_us) : 0ULL;
            uint64_t ticks = (remaining_us * static_cast<uint64_t>(TICKS_PER_SECOND) +
                              999999ULL) /
                             1000000ULL;
            if (ticks == 0ULL)
            {
                ticks = 1ULL;
            }
            if (ticks > 0xFFFFULL)
            {
                ticks = 0xFFFFULL;
            }
            wait_ticks = static_cast<uint16_t>(ticks);
        }

        (void)g_rx_sem.Pend(wait_ticks);
    }
}

int leap_rtems_transport_send_leap(
    LeapRtemsTransport*  transport,
    const uint8_t*       dst_mac,
    uint8_t              flags,
    uint16_t             service_id,
    uint16_t             message_type,
    uint32_t             session_id,
    uint32_t             sequence,
    uint32_t             ack_sequence,
    const uint8_t*       payload,
    size_t               payload_length)
{
    uint8_t tx[LEAP_MAX_FRAME_BYTES];
    size_t tx_len = 0U;
    PoolPtr pp;
    MACADR my_mac;
    uint16_t wire_payload_len;
    uint16_t frame_len;
    int32_t ifn;

    if (transport == nullptr || dst_mac == nullptr || transport->link_up == 0)
    {
        g_tx_failure_count++;
        return -1;
    }

    if (leap_frame_write(
            tx,
            sizeof(tx),
            &tx_len,
            flags,
            service_id,
            message_type,
            session_id,
            sequence,
            ack_sequence,
            payload,
            payload_length) != LEAP_FRAME_OK)
    {
        g_tx_failure_count++;
        return -1;
    }

    ifn = transport->interface_number;
    if (ifn <= 0)
    {
        ifn = g_bound_interface;
    }
    if (ifn <= 0)
    {
        g_tx_failure_count++;
        return -1;
    }

    pp = GetBuffer();
    if (pp == nullptr)
    {
        g_tx_failure_count++;
        return -1;
    }

    my_mac = InterfaceMAC(ifn);
    memcpy(pp->pData, dst_mac, 6);
    for (uint8_t i = 0; i < 6U; ++i)
    {
        pp->pData[6 + i] = my_mac.GetByte(i);
    }
    pp->pData[12] = static_cast<uint8_t>((transport->ethertype >> 8) & 0xFFU);
    pp->pData[13] = static_cast<uint8_t>(transport->ethertype & 0xFFU);

    if (tx_len > LEAP_MAX_FRAME_BYTES)
    {
        FreeBuffer(pp);
        g_tx_failure_count++;
        return -1;
    }
    memcpy(pp->pData + LEAP_ETH_HEADER_BYTES, tx, tx_len);

    wire_payload_len = static_cast<uint16_t>(tx_len);
    if (wire_payload_len < LEAP_MIN_TX_ETH_PAYLOAD)
    {
        memset(pp->pData + LEAP_ETH_HEADER_BYTES + wire_payload_len, 0,
               static_cast<size_t>(LEAP_MIN_TX_ETH_PAYLOAD - wire_payload_len));
        wire_payload_len = LEAP_MIN_TX_ETH_PAYLOAD;
    }

    frame_len = static_cast<uint16_t>(LEAP_ETH_HEADER_BYTES + wire_payload_len);
    pp->usedsize = frame_len;
    TransmitBuffer(pp, ifn);
    g_tx_frame_count++;
    return 0;
}

int leap_rtems_transport_poll_link(LeapRtemsTransport* transport)
{
    if (transport == nullptr)
    {
        return -1;
    }
    return transport->link_up ? 0 : -1;
}

void leap_rtems_transport_get_stats(LeapRtemsTransportStats* stats_out)
{
    if (stats_out == nullptr)
    {
        return;
    }

    stats_out->rx_callbacks = g_rx_callback_count;
    stats_out->rx_matches = g_rx_match_count;
    stats_out->rx_nonmatches = g_rx_nonmatch_count;
    stats_out->rx_wrong_interface = g_rx_wrong_interface_count;
    stats_out->rx_drops = g_rx_drop_count;
    stats_out->tx_frames = g_tx_frame_count;
    stats_out->tx_failures = g_tx_failure_count;
    stats_out->bound_interface = g_bound_interface;
    stats_out->last_rx_interface = g_rx_last_interface;
    stats_out->last_wrong_interface = g_rx_last_wrong_interface;
    stats_out->last_rx_ethertype = g_rx_last_ethertype;
    stats_out->last_nonmatch_ethertype = g_rx_last_nonmatch_ethertype;
}
