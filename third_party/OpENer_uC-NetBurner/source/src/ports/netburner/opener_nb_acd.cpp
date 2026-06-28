/*
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * OpENer_uC-NetBurner — RFC 5227 Address Conflict Detection on NetBurner NNDK.
 *
 * Integration (see opener_nb_acd.h):
 *   OpenerNbAcdInit()  — blocking probe when select_acd enabled; installs pArpFunc hook
 *   OpenerNbAcdPoll()  — call every opener_process() tick
 *   OpenerNbAcdNotifyIoConnection() — from CheckIoConnectionEvent in sample app
 *   OpenerNbAcdNotifyDhcpBound()    — from OpenerNbPrepareNetworkStack on DHCP
 *
 * NV: HalStorage_UserParams offset 512 via opener_nb_nv.cpp.
 */

#include "opener_nb_acd.h"

#if OPENER_NB_ACD

#include "opener_nb_acd_config.h"
#include "opener_nb_fault.h"
#include "opener_nb_nv.h"
#include "opener_nb_nndk_io.h"

#include <arp.h>
#include <arpinternal.h>
#include <buffers.h>
#include <constants.h>
#include <dhcpclient.h>
#include <netinterface.h>
#include <nettimer.h>
#include <nbrtos.h>
#include <predef.h>
#include <stdio.h>
#include <string.h>

extern ProcessArpFunc *pArpFunc;

#define OPENER_NB_ACD_LOG_NOTE(...) \
    do { \
        if (OPENER_NB_ACD_LOG) { \
            iprintf(__VA_ARGS__); \
        } \
    } while (0)

#define OPENER_NB_ACD_LOG_VERBOSE_NOTE(...) \
    do { \
        if (OPENER_NB_ACD_LOG_VERBOSE) { \
            iprintf(__VA_ARGS__); \
        } \
    } while (0)

#define OPENER_NB_ACD_LOG_EVENT(...) \
    do { \
        if (OPENER_NB_ACD_LOG_EVENTS) { \
            iprintf(__VA_ARGS__); \
        } \
    } while (0)

enum opener_nb_acd_state {
    ACD_STATE_IDLE = 0,
    ACD_STATE_PROBE,
    ACD_STATE_PROBE_WAIT,
    ACD_STATE_ANNOUNCE,
    ACD_STATE_ONGOING,
    ACD_STATE_DEFEND,
    ACD_STATE_FAULT,
};

static struct {
    int ifnum;
    IPADDR4 monitored_ip;
    MACADR our_mac;
    bool select_acd;
    bool conflict_status;
    bool fault_status;
    bool io_active;
    bool in_announce;
    bool defend_pending;
    uint32_t defend_deadline_sec;
    uint8_t defend_hits;
    uint8_t probe_count;
    uint8_t announce_count;
    uint8_t activity_on_conflict;
    OpenerNbAcdLastConflict last_conflict;
    enum opener_nb_acd_state state;
  IPADDR4 last_dhcp_ip;
    int last_dhcp_state;
} g_acd;

static ProcessArpFunc *s_orig_arp = NULL;
static bool s_last_link_active = false;
static volatile bool s_acd_timer_due = false;
static volatile bool s_arp_conflict_pending = false;
static MACADR s_arp_conflict_mac;
static ARP s_arp_conflict_pdu;
static uint16_t s_io_refcount = 0;

static inline bool acd_fnptr_is_callable(uintptr_t fn)
{
    return fn != 0u && ((fn & 0x1u) != 0u);
}

static uint32_t acd_ms_to_ticks(uint32_t ms)
{
    return (ms * (uint32_t)TICKS_PER_SECOND + 999u) / 1000u;
}

static uint32_t acd_probe_interval_ms(void)
{
#if OPENER_NB_ACD_PROBE_INTERVAL_MIN_MS >= OPENER_NB_ACD_PROBE_INTERVAL_MAX_MS
    return OPENER_NB_ACD_PROBE_INTERVAL_MIN_MS;
#else
    const uint32_t span = OPENER_NB_ACD_PROBE_INTERVAL_MAX_MS - OPENER_NB_ACD_PROBE_INTERVAL_MIN_MS + 1u;
    const uint32_t r = (uint32_t)TimeTick ^ ((uint32_t)g_acd.probe_count << 8) ^
                       (uint32_t)g_acd.monitored_ip;
    return OPENER_NB_ACD_PROBE_INTERVAL_MIN_MS + (r % span);
#endif
}

class ScipAcdTimer : public TimeOutElement
{
   public:
    void arm_ticks(uint32_t ticks)
    {
        NetTimeOutManager.RegisterTriggerAt(*this, TimeTick + ticks);
    }

    void disarm()
    {
        NetTimeOutManager.DeRegister(*this);
    }

   protected:
    virtual void TimeElementEvent()
    {
        /* Enet#26 must not run ACD state machine / iprintf / ARP sends here. */
        s_acd_timer_due = true;
    }
};

static ScipAcdTimer g_acd_timer;

static bool mac_equal_local(const MACADR *a, const MACADR *b)
{
    return MacEqual(*a, *b);
}

static void copy_arp_pdu(uint8_t *dst, const ARP *arp)
{
    if (dst == NULL || arp == NULL) {
        return;
    }
    memcpy(dst, &arp->hard_Type, 28);
}

static void persist_last_conflict(void)
{
    OpenerNbAcdNvSetLastConflict(&g_acd.last_conflict);
    OpenerNbAcdNvSave();
}

static void record_conflict(uint8_t activity, const MACADR *remote_mac, const ARP *arp)
{
    g_acd.last_conflict.acd_activity = activity;
    if (remote_mac != NULL) {
        memcpy(g_acd.last_conflict.remote_mac, remote_mac->phywadr, 6);
    } else {
        memset(g_acd.last_conflict.remote_mac, 0, 6);
    }
    if (arp != NULL) {
        copy_arp_pdu(g_acd.last_conflict.arp_pdu, arp);
    } else {
        memset(g_acd.last_conflict.arp_pdu, 0, 28);
    }
    g_acd.conflict_status = true;
    persist_last_conflict();
}

static void zero_interface_ip(void)
{
    InterfaceBlock *block = GetInterfaceBlock(g_acd.ifnum);
    if (block == NULL) {
        return;
    }
    block->ip4.cur_addr = IPADDR4::NullIP();
}

static void enter_fault(uint8_t activity, const MACADR *remote_mac, const ARP *arp)
{
    if (g_acd.state == ACD_STATE_FAULT) {
        return;
    }

    record_conflict(activity, remote_mac, arp);
    g_acd.fault_status = true;
    g_acd.state = ACD_STATE_FAULT;
    g_acd_timer.disarm();

    OPENER_NB_ACD_LOG_EVENT("ACD: fault activity=%u\r\n", (unsigned)activity);

    OpenerNbAcdOnFault();
    OpenerNbAcdClearIoActive();
    zero_interface_ip();
}

static bool is_conflicting_arp(const ARP *arp)
{
    if (arp == NULL || g_acd.monitored_ip.IsNull()) {
        return false;
    }
    if ((uint32_t)arp->sender_Ip != (uint32_t)g_acd.monitored_ip) {
        return false;
    }
    if (mac_equal_local(&arp->sender_phy, &g_acd.our_mac)) {
        return false;
    }
    return true;
}

static void send_arp_probe(void)
{
    ArpRecord *pa = FindAddArp(g_acd.monitored_ip);
    if (pa != NULL) {
        RawSendArp(pa, IPADDR4::NullIP(), g_acd.ifnum);
    }
}

static void send_gratuitous(void)
{
    sendGratuitousArp(g_acd.ifnum, g_acd.monitored_ip);
}

static bool probe_response_conflict(void)
{
    ArpRecord *pa = FindArp(g_acd.monitored_ip);
    if (pa == NULL || IsNull(pa)) {
        return false;
    }
    return !mac_equal_local(&pa->mac, &g_acd.our_mac);
}

static void begin_probe_sequence(void)
{
    if (!g_acd.select_acd || g_acd.monitored_ip.IsNull()) {
        g_acd.state = ACD_STATE_ONGOING;
        return;
    }

    g_acd.probe_count = 0;
    g_acd.announce_count = 0;
    g_acd.in_announce = false;
    g_acd.defend_pending = false;
    g_acd.defend_hits = 0;
    g_acd.state = ACD_STATE_PROBE;
    g_acd.activity_on_conflict = OPENER_NB_ACD_ACTIVITY_PROBE;
    OPENER_NB_ACD_LOG_VERBOSE_NOTE("ACD: probe start for %u.%u.%u.%u\r\n",
                      (unsigned)((uint32_t)g_acd.monitored_ip >> 24) & 0xFFu,
                      (unsigned)((uint32_t)g_acd.monitored_ip >> 16) & 0xFFu,
                      (unsigned)((uint32_t)g_acd.monitored_ip >> 8) & 0xFFu,
                      (unsigned)((uint32_t)g_acd.monitored_ip & 0xFFu));

#if OPENER_NB_ACD_INITIAL_DELAY_MS > 0
    g_acd.state = ACD_STATE_PROBE_WAIT;
    g_acd_timer.arm_ticks(acd_ms_to_ticks(OPENER_NB_ACD_INITIAL_DELAY_MS));
#endif
}

static void handle_defend_conflict(const MACADR *remote_mac, const ARP *arp)
{
    const uint32_t now = Secs;

    if (!g_acd.defend_pending) {
        g_acd.defend_pending = true;
        g_acd.defend_deadline_sec = now + (uint32_t)OPENER_NB_ACD_DEFEND_INTERVAL_SEC;
        g_acd.defend_hits = 1;
        g_acd.state = ACD_STATE_DEFEND;
        send_gratuitous();
        record_conflict(OPENER_NB_ACD_ACTIVITY_ONGOING, remote_mac, arp);
        OPENER_NB_ACD_LOG_EVENT("ACD: defend (first conflict)\r\n");
        return;
    }

    if (now <= g_acd.defend_deadline_sec) {
        ++g_acd.defend_hits;
        OPENER_NB_ACD_LOG_EVENT("ACD: defend failed (%u hits)\r\n", (unsigned)g_acd.defend_hits);
        enter_fault(OPENER_NB_ACD_ACTIVITY_ONGOING, remote_mac, arp);
        return;
    }

    g_acd.defend_pending = true;
    g_acd.defend_deadline_sec = now + (uint32_t)OPENER_NB_ACD_DEFEND_INTERVAL_SEC;
    g_acd.defend_hits = 1;
    send_gratuitous();
    record_conflict(OPENER_NB_ACD_ACTIVITY_ONGOING, remote_mac, arp);
}

static void handle_incoming_conflict(const MACADR *remote_mac, const ARP *arp)
{
    if (g_acd.state == ACD_STATE_FAULT || !g_acd.select_acd) {
        return;
    }

    if (g_acd.in_announce) {
        send_gratuitous();
        return;
    }

    if (g_acd.state == ACD_STATE_PROBE || g_acd.state == ACD_STATE_PROBE_WAIT) {
        enter_fault(OPENER_NB_ACD_ACTIVITY_PROBE, remote_mac, arp);
        return;
    }

    if (g_acd.state == ACD_STATE_ONGOING || g_acd.state == ACD_STATE_DEFEND) {
        handle_defend_conflict(remote_mac, arp);
        return;
    }
}

static void opener_nb_acd_arp_hook(PoolPtr p, PEFRAME pf)
{
    if (p != NULL && pf != NULL && pf->pData != NULL) {
        const PARP arp = (PARP)(pf->pData);
        if (is_conflicting_arp(arp)) {
            s_arp_conflict_mac = arp->sender_phy;
            copy_arp_pdu((uint8_t *)&s_arp_conflict_pdu, arp);
            s_arp_conflict_pending = true;
        }
    }
    if (s_orig_arp != NULL && acd_fnptr_is_callable((uintptr_t)s_orig_arp)) {
        s_orig_arp(p, pf);
    }
}

static void OpenerNbAcdPoll_pending_arp(void)
{
    if (!s_arp_conflict_pending) {
        return;
    }
    s_arp_conflict_pending = false;
    handle_incoming_conflict(&s_arp_conflict_mac, &s_arp_conflict_pdu);
}

static void refresh_monitored_ip(void)
{
    g_acd.monitored_ip = InterfaceIP(g_acd.ifnum);
    g_acd.our_mac = InterfaceMAC(g_acd.ifnum);
}

static void poll_dhcp(void)
{
    InterfaceBlock *block = GetInterfaceBlock(g_acd.ifnum);
    if (block == NULL) {
        return;
    }

    const int dhcp_state = block->dhcpClient.GetDHCPState();
    const IPADDR4 cur_ip = InterfaceIP(g_acd.ifnum);

    if (dhcp_state == SDHCP_CMPL || dhcp_state == SDHCP_RENEW || dhcp_state == SDHCP_REBIND) {
        if (cur_ip.NotNull() &&
            ((uint32_t)cur_ip != (uint32_t)g_acd.last_dhcp_ip ||
             dhcp_state != g_acd.last_dhcp_state)) {
            g_acd.last_dhcp_ip = cur_ip;
            g_acd.last_dhcp_state = dhcp_state;
            refresh_monitored_ip();
            if (g_acd.state != ACD_STATE_FAULT) {
                begin_probe_sequence();
            }
        }
    }

    g_acd.last_dhcp_state = dhcp_state;
    if (cur_ip.NotNull()) {
        g_acd.last_dhcp_ip = cur_ip;
    }
}

static void poll_semi_active(void)
{
#if OPENER_NB_ACD_SEMI_ACTIVE_ENABLE
    static uint32_t last_semi_sec = 0;

    if (!g_acd.io_active || g_acd.state != ACD_STATE_ONGOING || g_acd.monitored_ip.IsNull()) {
        return;
    }
    if ((Secs - last_semi_sec) < (uint32_t)OPENER_NB_ACD_SEMI_ACTIVE_INTERVAL_SEC) {
        return;
    }
    last_semi_sec = Secs;
    g_acd.activity_on_conflict = OPENER_NB_ACD_ACTIVITY_SEMI_ACTIVE;
    g_acd.probe_count = 0;
    g_acd.state = ACD_STATE_PROBE;
    OPENER_NB_ACD_LOG_VERBOSE_NOTE("ACD: semi-active probe\r\n");
#endif
}

static void poll_link_restore(void)
{
    const bool link = InterfaceLinkActive(g_acd.ifnum);

    if (!link) {
        s_last_link_active = false;
        return;
    }

    if (!s_last_link_active && g_acd.select_acd && g_acd.state != ACD_STATE_FAULT) {
        /* Commit rising edge before nested poll — prevents begin_probe_sequence loop. */
        s_last_link_active = true;
        refresh_monitored_ip();
        if (g_acd.monitored_ip.NotNull()) {
            begin_probe_sequence();
        }
    }
}

void OpenerNbAcdPoll(void)
{
    poll_link_restore();
    OpenerNbAcdPoll_pending_arp();

    if (!g_acd.select_acd || g_acd.state == ACD_STATE_FAULT) {
        poll_dhcp();
        return;
    }

    poll_dhcp();
    poll_semi_active();

    const bool timer_due = s_acd_timer_due;
    if (timer_due) {
        s_acd_timer_due = false;
    }

    switch (g_acd.state) {
    case ACD_STATE_PROBE:
        if (g_acd.probe_count >= OPENER_NB_ACD_PROBE_COUNT) {
#if OPENER_NB_ACD_ANNOUNCE_WAIT_SEC > 0
            g_acd.state = ACD_STATE_PROBE_WAIT;
            g_acd_timer.arm_ticks((uint32_t)OPENER_NB_ACD_ANNOUNCE_WAIT_SEC * TICKS_PER_SECOND);
            break;
#else
            g_acd.state = ACD_STATE_ANNOUNCE;
            g_acd.announce_count = 0;
            g_acd.in_announce = true;
            /* Send first announce immediately; further announces wait on timer. */
#endif
        } else {
            send_arp_probe();
            ++g_acd.probe_count;
            g_acd.state = ACD_STATE_PROBE_WAIT;
            g_acd_timer.arm_ticks(acd_ms_to_ticks(acd_probe_interval_ms()));
            break;
        }
#if OPENER_NB_ACD_ANNOUNCE_WAIT_SEC == 0
        /* fall through when probe sequence completes */
        [[fallthrough]];
#endif
    case ACD_STATE_ANNOUNCE:
        if (g_acd.announce_count > 0u && !timer_due) {
            break;
        }
        if (g_acd.announce_count >= OPENER_NB_ACD_ANNOUNCE_COUNT) {
            g_acd.in_announce = false;
            g_acd.state = ACD_STATE_ONGOING;
            OPENER_NB_ACD_LOG_VERBOSE_NOTE("ACD: ongoing detection\r\n");
            g_acd_timer.arm_ticks(TICKS_PER_SECOND);
            break;
        }
        send_gratuitous();
        ++g_acd.announce_count;
        g_acd_timer.arm_ticks((uint32_t)OPENER_NB_ACD_ANNOUNCE_INTERVAL_SEC * TICKS_PER_SECOND);
        break;

    case ACD_STATE_PROBE_WAIT:
        if (!timer_due) {
            break;
        }
        if (g_acd.probe_count >= OPENER_NB_ACD_PROBE_COUNT) {
            g_acd.state = ACD_STATE_ANNOUNCE;
            g_acd.announce_count = 0;
            g_acd.in_announce = true;
            OpenerNbAcdPoll();
            break;
        }
        if (probe_response_conflict()) {
            ArpRecord *pa = FindArp(g_acd.monitored_ip);
            MACADR remote = pa != NULL ? pa->mac : ENET_ZERO;
            enter_fault(g_acd.activity_on_conflict, &remote, NULL);
            return;
        }
        g_acd.state = ACD_STATE_PROBE;
        OpenerNbAcdPoll();
        break;

    case ACD_STATE_DEFEND:
        if (g_acd.defend_pending && Secs > g_acd.defend_deadline_sec) {
            g_acd.defend_pending = false;
            g_acd.defend_hits = 0;
            g_acd.state = ACD_STATE_ONGOING;
            OPENER_NB_ACD_LOG_EVENT("ACD: defend succeeded\r\n");
        }
        g_acd_timer.arm_ticks(TICKS_PER_SECOND);
        break;

    case ACD_STATE_ONGOING:
        g_acd_timer.arm_ticks(TICKS_PER_SECOND);
        break;

    default:
        break;
    }
}

bool OpenerNbAcdInit(int ifnum)
{
    g_acd = {};
    g_acd.ifnum = ifnum;
    g_acd.last_dhcp_state = SDHCP_NOTSTARTED;

    OpenerNbAcdNvLoad();
    g_acd.select_acd = OpenerNbAcdNvGetSelectAcd();
    OpenerNbAcdNvGetLastConflict(&g_acd.last_conflict);
    g_acd.conflict_status = (g_acd.last_conflict.acd_activity != OPENER_NB_ACD_ACTIVITY_NONE);
    g_acd.fault_status = false;
    s_last_link_active = InterfaceLinkActive(ifnum);

    if (!g_acd.select_acd) {
        g_acd.state = ACD_STATE_IDLE;
        return true;
    }

    refresh_monitored_ip();
    if (g_acd.monitored_ip.IsNull()) {
        InterfaceBlock *block = GetInterfaceBlock(ifnum);
        if (block != NULL) {
            for (int i = 0; i < 30 && g_acd.monitored_ip.IsNull(); ++i) {
                const int st = block->dhcpClient.GetDHCPState();
                if (st == SDHCP_CMPL || st == SDHCP_RENEW || st == SDHCP_REBIND) {
                    refresh_monitored_ip();
                    break;
                }
                OSTimeDly(TICKS_PER_SECOND);
                refresh_monitored_ip();
            }
        }
    }

    if (g_acd.monitored_ip.IsNull()) {
        OPENER_NB_ACD_LOG_VERBOSE_NOTE("ACD: no IPv4 yet\r\n");
        g_acd.state = ACD_STATE_IDLE;
    }

    if (s_orig_arp == NULL) {
        s_orig_arp = pArpFunc;
        pArpFunc = opener_nb_acd_arp_hook;
    }

    if (g_acd.monitored_ip.NotNull()) {
        begin_probe_sequence();
        while (g_acd.state == ACD_STATE_PROBE || g_acd.state == ACD_STATE_PROBE_WAIT ||
               g_acd.state == ACD_STATE_ANNOUNCE) {
            OSTimeDly(TICKS_PER_SECOND / 10);
            OpenerNbAcdPoll();
        }
    } else {
        g_acd_timer.arm_ticks(TICKS_PER_SECOND);
    }

    return !g_acd.fault_status;
}

void OpenerNbAcdShutdown(void)
{
    g_acd_timer.disarm();
    if (s_orig_arp != NULL) {
        pArpFunc = s_orig_arp;
        s_orig_arp = NULL;
    }
}

void OpenerNbAcdSetAdapter(void *adapter)
{
    (void)adapter;
}

bool OpenerNbAcdIsCapable(void)
{
    return OPENER_NB_ACD != 0;
}

bool OpenerNbAcdSelectEnabled(void)
{
    return g_acd.select_acd;
}

bool OpenerNbAcdStatusConflict(void)
{
    return g_acd.conflict_status;
}

bool OpenerNbAcdStatusFault(void)
{
    return g_acd.fault_status;
}

bool OpenerNbAcdGetSelectAcd(void)
{
    return g_acd.select_acd;
}

void OpenerNbAcdGetLastConflict(OpenerNbAcdLastConflict *out)
{
    if (out == NULL) {
        return;
    }
    *out = g_acd.last_conflict;
}

opener_nb_status_t OpenerNbAcdSetSelectAcd(bool enable, bool *needs_reset_out)
{
    OpenerNbAcdNvSetSelectAcd(enable);
    OpenerNbAcdNvSave();
    if (needs_reset_out != NULL) {
        *needs_reset_out = true;
    }
    return OPENER_NB_OK;
}

opener_nb_status_t OpenerNbAcdClearLastConflict(void)
{
    memset(&g_acd.last_conflict, 0, sizeof(g_acd.last_conflict));
    g_acd.conflict_status = false;
    if (!g_acd.fault_status) {
        /* Keep fault latched until reboot when IP was zeroed. */
    }
    OpenerNbAcdNvSetLastConflict(&g_acd.last_conflict);
    OpenerNbAcdNvSave();
    return OPENER_NB_OK;
}

void opener_nb_acd_on_arp_rx(void *pool_ptr, void *eth_frame_ptr)
{
    opener_nb_acd_arp_hook((PoolPtr)pool_ptr, (PEFRAME)eth_frame_ptr);
}

void OpenerNbAcdNotifyIoConnection(bool active)
{
    if (active) {
        ++s_io_refcount;
    } else if (s_io_refcount > 0u) {
        --s_io_refcount;
    }
    g_acd.io_active = (s_io_refcount > 0u);
}

void OpenerNbAcdClearIoActive(void)
{
    s_io_refcount = 0;
    g_acd.io_active = false;
}

void OpenerNbAcdNotifyDhcpBound(int ifnum)
{
    if (ifnum == g_acd.ifnum && g_acd.select_acd && g_acd.state != ACD_STATE_FAULT) {
        refresh_monitored_ip();
        begin_probe_sequence();
    }
}

#endif /* OPENER_NB_ACD */
