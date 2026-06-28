/*
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * OpENer_uC-NetBurner — IEEE 802.1AB LLDP TX/RX + CIP 0x109/0x10A data plane.
 *
 * Integration (see opener_nb_lldp.h):
 *   OpenerNbLldpInit() / OpenerNbLldpPoll() — from opener.c
 *   OpenerNbLldpBuild* / OpenerNbLldpSetMgmtAttr — from opener_nb_lldp_cip.c
 *
 * Requires makefile link of nndk_overload/lldp.o and ALLOW_CUSTOM_NET_DO_RX=1.
 * Public API functions are in the extern "C" block near file end.
 */

#include "opener_nb_lldp.h"

#if OPENER_NB_LLDP

#include "opener_nb_identity.h"
#include "opener_nb_le.h"
#include "opener_nb_nndk_io.h"

#include "lldp.h"
#include "opener_nb_hal_storage.h"

#include <buffers.h>
#include <constants.h>
#include <ethernet.h>
#include <netinterface.h>
#include <nettypes.h>
#include <nbrtos.h>
#include <predef.h>
#include <stdio.h>
#include <string.h>

#if OPENER_NB_LLDP_RX_ENABLE
#include <netrx.h>
#endif

ProcessLLDPptr pProcessLLDP = NULL;

#ifdef ALLOW_CUSTOM_NET_DO_RX
static netDoRXFunc s_prev_custom_net_do_rx = NULL;
#endif

#define OPENER_NB_LLDP_LOG_NOTE(...) \
    do { \
        if (OPENER_NB_LLDP_LOG) { \
            iprintf(__VA_ARGS__); \
        } \
    } while (0)

#define OPENER_NB_LLDP_LOG_VERBOSE_NOTE(...) \
    do { \
        if (OPENER_NB_LLDP_LOG_VERBOSE) { \
            iprintf(__VA_ARGS__); \
        } \
    } while (0)

#define OPENER_NB_LLDP_LOG_NEIGHBOR(...) \
    do { \
        if (OPENER_NB_LLDP_LOG_NEIGHBORS) { \
            iprintf(__VA_ARGS__); \
        } \
    } while (0)

#define OPENER_NB_LLDP_ODVA_OUI 0x00120Fu
#define OPENER_NB_LLDP_ETHERTYPE 0x88CCu

#define OPENER_NB_CIP_ERR_INVALID_ATTR_VALUE 0x09u
#define OPENER_NB_CIP_ERR_ATTR_NOT_SETTABLE  0x14u
#define OPENER_NB_CIP_ERR_INVALID_PARAM      0x20u

#define OPENER_NB_LLDP_NV_MAGIC   0x53434C50u /* 'SCLP' */
#define OPENER_NB_LLDP_NV_OFFSET  576

#define OPENER_NB_LLDP_GLOBAL_BIT 0x01u
#define OPENER_NB_LLDP_PORT1_BIT  0x02u
#define OPENER_NB_LLDP_ETH_LINK_MAX_INSTANCE 1u
#define OPENER_NB_LLDP_DEFAULT_ENABLE (OPENER_NB_LLDP_GLOBAL_BIT | OPENER_NB_LLDP_PORT1_BIT)

#define OPENER_NB_LLDP_MCAST_NEAREST_BRIDGE 0x0180C200000Eu
#define OPENER_NB_LLDP_MCAST_NON_TPMR       0x0180C2000003u

static uint32_t g_lldp_net_rx_calls = 0u;
static uint32_t g_lldp_net_rx_lldp = 0u;

#define OPENER_NB_LLDP_RX_QUEUE_DEPTH 8u
#define OPENER_NB_LLDP_MAX_RX_PDU     512u

#if defined(__GNUC__)
#define OPENER_NB_LLDP_DMB() __asm volatile("dmb 0xF" ::: "memory")
#else
#define OPENER_NB_LLDP_DMB() ((void)0)
#endif

typedef struct opener_nb_lldp_rx_slot {
    uint8_t src_mac[6];
    uint16_t pdu_len;
    uint8_t pdu[OPENER_NB_LLDP_MAX_RX_PDU];
} opener_nb_lldp_rx_slot_t;

static opener_nb_lldp_rx_slot_t s_rx_queue[OPENER_NB_LLDP_RX_QUEUE_DEPTH];
static opener_nb_lldp_rx_slot_t s_rx_work_slot;
static volatile uint8_t s_rx_head = 0u;
static volatile uint8_t s_rx_tail = 0u;
static volatile uint32_t s_rx_drop_count = 0u;

static uint16_t frame_ethertype(const uint8_t *frame, uint16_t ocount)
{
    if (frame == NULL || ocount < 14u) {
        return 0u;
    }
    uint16_t et = (uint16_t)(((uint16_t)frame[12] << 8) | (uint16_t)frame[13]);
    if (et == (uint16_t)ETHERNET_ETHERTYPE_VLAN && ocount >= 18u) {
        et = (uint16_t)(((uint16_t)frame[16] << 8) | (uint16_t)frame[17]);
    }
    return et;
}

static void lldp_mac_from_u48(MACADR *mac, uint64_t addr48)
{
    uint8_t bytes[6];
    for (int i = 5; i >= 0; --i) {
        bytes[i] = (uint8_t)(addr48 & 0xFFu);
        addr48 >>= 8;
    }
    mac->SetFromBytes(bytes);
}

static void lldp_enable_multicast_rx(int ifnum)
{
    MACADR m;
    lldp_mac_from_u48(&m, OPENER_NB_LLDP_MCAST_NEAREST_BRIDGE);
    EnableMulticast(m, ifnum);
    lldp_mac_from_u48(&m, OPENER_NB_LLDP_MCAST_NON_TPMR);
    EnableMulticast(m, ifnum);
    OPENER_NB_LLDP_LOG_VERBOSE_NOTE("LLDP: multicast filters enabled on if %d\r\n", ifnum);
}

#define OPENER_NB_LLDP_DATASTORE_DATA_TABLE 0x0001u

#define OPENER_NB_LLDP_ID_KEY_LEN 12u
#define OPENER_NB_LLDP_CIP_IDENT_ATTR_LEN 12u

typedef struct opener_nb_lldp_nv_blob {
    uint32_t magic;
    uint8_t enable;
    uint8_t tx_hold;
    uint16_t tx_interval;
    uint8_t reserved[2];
} opener_nb_lldp_nv_blob_t;

typedef struct opener_nb_lldp_neighbor {
    bool active;
    uint16_t instance;
    uint8_t eth_link_instance;
    uint8_t mac[6];
    char label[65];
    char sys_name[65];
    char odva_label[65];
    uint16_t ttl_remaining;
    uint16_t ttl_initial;
    uint8_t sys_cap[4];
    uint8_t eth_cap[4];
    uint8_t cip_id[OPENER_NB_LLDP_ID_KEY_LEN];
    bool has_cip_id;
    uint8_t mgmt_count;
    uint8_t mgmt_ip[4];
    uint32_t last_change_sec;
    uint8_t chassis_len;
    uint8_t chassis[32];
    uint8_t port_len;
    uint8_t port[32];
} opener_nb_lldp_neighbor_t;

static opener_nb_lldp_neighbor_t s_rx_parse_scratch;

static struct {
    int ifnum;
    bool initialized;
    uint8_t enable;
    uint16_t tx_interval;
    uint8_t tx_hold;
    uint32_t db_last_change_sec;
    uint32_t rx_frame_count;
    bool last_link_active;
    uint16_t next_instance;
    opener_nb_lldp_neighbor_t neighbors[OPENER_NB_LLDP_MAX_NEIGHBORS];
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint8_t major_revision;
    uint8_t minor_revision;
    uint32_t serial_number;
    char host_name[65];
    char product_name[65];
    char interface_label[33];
    opener_nb_ipv4_t ip_address;
    uint8_t mac_address[6];
    OS_CRIT crit;
} g_lldp;

class ScipLLDPEntity : public LLDPEntity
{
   public:
    ScipLLDPEntity(InterfaceBlock &ib) : LLDPEntity(ib) {}

    void BuildPacket() override
    {
        StartNewPacket();
        if (g_lldp.host_name[0] != '\0') {
            AddHostName(g_lldp.host_name);
        }
        if (g_lldp.product_name[0] != '\0') {
            AddSysDescription(g_lldp.product_name);
        }
        AddSysCapabilities(0x0080, 0x0080);
        {
            const IPADDR4 ipa(g_lldp.ip_address.octets[0], g_lldp.ip_address.octets[1],
                              g_lldp.ip_address.octets[2], g_lldp.ip_address.octets[3]);
            if (!ipa.IsNull()) {
                AddManagmentAddr(ipa);
            }
        }
        AddCustomRaw(OPENER_NB_LLDP_ODVA_OUI, 2, 6, (puint8_t)g_lldp.mac_address);
        AddCustomString(OPENER_NB_LLDP_ODVA_OUI, 3, g_lldp.interface_label);
        {
            uint8_t key[OPENER_NB_LLDP_ID_KEY_LEN];
            key[0] = (uint8_t)((g_lldp.vendor_id >> 8) & 0xFFu);
            key[1] = (uint8_t)(g_lldp.vendor_id & 0xFFu);
            key[2] = (uint8_t)((g_lldp.device_type >> 8) & 0xFFu);
            key[3] = (uint8_t)(g_lldp.device_type & 0xFFu);
            key[4] = (uint8_t)((g_lldp.product_code >> 8) & 0xFFu);
            key[5] = (uint8_t)(g_lldp.product_code & 0xFFu);
            key[6] = g_lldp.major_revision;
            key[7] = g_lldp.minor_revision;
            key[8] = (uint8_t)((g_lldp.serial_number >> 24) & 0xFFu);
            key[9] = (uint8_t)((g_lldp.serial_number >> 16) & 0xFFu);
            key[10] = (uint8_t)((g_lldp.serial_number >> 8) & 0xFFu);
            key[11] = (uint8_t)(g_lldp.serial_number & 0xFFu);
            AddCustomRaw(OPENER_NB_LLDP_ODVA_OUI, 9, OPENER_NB_LLDP_ID_KEY_LEN, key);
        }
        UseNewPacket();
    }

    void ApplyTiming(uint16_t interval_sec, uint8_t hold_mult)
    {
        m_iTxTime = (int)interval_sec;
        m_iHoldTime = (int)interval_sec * (int)hold_mult;
        RebuildPacket();
        RequestFastTransmit();
    }

    void SetGlobalEnable(bool enable)
    {
        m_bEnable = enable;
        if (enable) {
            RebuildPacket();
            RequestFastTransmit();
        }
    }
};

static ScipLLDPEntity *g_lldp_entity = NULL;
static uint32_t g_lldp_next_tx_sec = 0u;
static bool s_lldp_hooks_armed = false;

static void opener_nb_lldp_arm_hooks(void);

static uint32_t sys_up_time_sec(void)
{
    return (uint32_t)(TimeTick / TICKS_PER_SECOND);
}

static void lldp_nv_set_defaults(void)
{
    g_lldp.enable = OPENER_NB_LLDP_DEFAULT_ENABLE;
    g_lldp.tx_interval = (uint16_t)OPENER_NB_LLDP_DEFAULT_TX_INTERVAL_SEC;
    g_lldp.tx_hold = (uint8_t)OPENER_NB_LLDP_DEFAULT_TX_HOLD;
}

static void lldp_nv_load(void)
{
    lldp_nv_set_defaults();
    opener_nb_lldp_nv_blob_t blob;
    const int n = HalStorage_Read(HalStore_UserParams, &blob, (int)sizeof(blob), OPENER_NB_LLDP_NV_OFFSET);
    if (n == (int)sizeof(blob) && blob.magic == OPENER_NB_LLDP_NV_MAGIC) {
        g_lldp.enable = blob.enable;
        g_lldp.tx_interval = blob.tx_interval;
        g_lldp.tx_hold = blob.tx_hold;
        if ((g_lldp.enable & OPENER_NB_LLDP_GLOBAL_BIT) != 0u) {
            g_lldp.enable |= OPENER_NB_LLDP_PORT1_BIT;
        }
        if (g_lldp.tx_interval < 5u) {
            g_lldp.tx_interval = (uint16_t)OPENER_NB_LLDP_DEFAULT_TX_INTERVAL_SEC;
        }
        if (g_lldp.tx_hold < 1u || g_lldp.tx_hold > 100u) {
            g_lldp.tx_hold = (uint8_t)OPENER_NB_LLDP_DEFAULT_TX_HOLD;
        }
    }
}

static void lldp_nv_save(void)
{
    opener_nb_lldp_nv_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    blob.magic = OPENER_NB_LLDP_NV_MAGIC;
    blob.enable = g_lldp.enable;
    blob.tx_interval = g_lldp.tx_interval;
    blob.tx_hold = g_lldp.tx_hold;
    (void)HalStorage_Save(HalStore_UserParams, &blob, (int)sizeof(blob), OPENER_NB_LLDP_NV_OFFSET);
}

static void mark_db_changed(void)
{
    g_lldp.db_last_change_sec = sys_up_time_sec();
}

static bool lldp_global_enabled(void)
{
    return (g_lldp.enable & OPENER_NB_LLDP_GLOBAL_BIT) != 0u;
}

static bool lldp_port_enabled(void)
{
    return (g_lldp.enable & OPENER_NB_LLDP_PORT1_BIT) != 0u;
}

static bool lldp_rx_enabled(void)
{
    return lldp_global_enabled() && lldp_port_enabled();
}

static bool lldp_tx_enabled(void)
{
    return lldp_global_enabled() && lldp_port_enabled();
}

static void opener_nb_lldp_schedule_tx(uint32_t delay_sec)
{
    if (delay_sec == 0u) {
        delay_sec = 1u;
    }
    g_lldp_next_tx_sec = sys_up_time_sec() + delay_sec;
}

static void OpenerNbLldpPoll_tx(void)
{
    if (g_lldp_entity == NULL || !lldp_tx_enabled()) {
        return;
    }

    InterfaceBlock *ib = GetInterfaceBlock(g_lldp.ifnum);
    if (ib == NULL || !ib->LinkActive()) {
        return;
    }

    const uint32_t now = sys_up_time_sec();
    if (now < g_lldp_next_tx_sec) {
        return;
    }

    const uint32_t next_delay = g_lldp_entity->ServiceTransmit();
    opener_nb_lldp_schedule_tx(next_delay);
}

static uint16_t lldp_enable_array_length(void)
{
    return (uint16_t)OPENER_NB_LLDP_ETH_LINK_MAX_INSTANCE;
}

static uint8_t lldp_enable_wire_byte(uint16_t port_index)
{
    if (port_index == 0u) {
        return lldp_tx_enabled() ? 1u : 0u;
    }
    return 0u;
}

static opener_nb_lldp_neighbor_t *neighbor_by_instance(uint16_t inst);
static uint16_t neighbor_count_locked(void);

static bool data_table_placeholder_instance_locked(uint16_t inst)
{
    if (inst != 1u) {
        return false;
    }
    return neighbor_count_locked() == 0u;
}

static void fill_data_table_placeholder(opener_nb_lldp_neighbor_t *n)
{
    memset(n, 0, sizeof(*n));
}

static void write_cip_identification_attr(const opener_nb_lldp_neighbor_t *n, uint8_t *rsp, size_t *o)
{
    if (n != NULL && n->has_cip_id) {
        const uint16_t vendor = (uint16_t)(((uint16_t)n->cip_id[0] << 8) | n->cip_id[1]);
        const uint16_t device_type = (uint16_t)(((uint16_t)n->cip_id[2] << 8) | n->cip_id[3]);
        const uint16_t product_code = (uint16_t)(((uint16_t)n->cip_id[4] << 8) | n->cip_id[5]);
        const uint32_t serial = ((uint32_t)n->cip_id[8] << 24) | ((uint32_t)n->cip_id[9] << 16) |
                                ((uint32_t)n->cip_id[10] << 8) | (uint32_t)n->cip_id[11];
        opener_nb_le_write_u16(rsp + *o, vendor);
        *o += 2;
        opener_nb_le_write_u16(rsp + *o, device_type);
        *o += 2;
        opener_nb_le_write_u16(rsp + *o, product_code);
        *o += 2;
        rsp[(*o)++] = n->cip_id[6];
        rsp[(*o)++] = n->cip_id[7];
        opener_nb_le_write_u32(rsp + *o, serial);
        *o += 4;
    } else {
        memset(rsp + *o, 0, OPENER_NB_LLDP_CIP_IDENT_ATTR_LEN);
        *o += OPENER_NB_LLDP_CIP_IDENT_ATTR_LEN;
    }
}

static void clear_neighbors_locked(void)
{
    for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
        memset(&g_lldp.neighbors[i], 0, sizeof(g_lldp.neighbors[i]));
    }
    g_lldp.next_instance = 1u;
    mark_db_changed();
}

static bool neighbor_key_equal(const opener_nb_lldp_neighbor_t *a, const opener_nb_lldp_neighbor_t *b)
{
    if (a->chassis_len != b->chassis_len || a->port_len != b->port_len) {
        return false;
    }
    if (a->chassis_len > 0 && memcmp(a->chassis, b->chassis, a->chassis_len) != 0) {
        return false;
    }
    if (a->port_len > 0 && memcmp(a->port, b->port, a->port_len) != 0) {
        return false;
    }
    return a->chassis_len > 0 || a->port_len > 0;
}

static opener_nb_lldp_neighbor_t *find_neighbor_by_key(const opener_nb_lldp_neighbor_t *key)
{
    for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
        opener_nb_lldp_neighbor_t *n = &g_lldp.neighbors[i];
        if (n->active && neighbor_key_equal(n, key)) {
            return n;
        }
    }
    return NULL;
}

static opener_nb_lldp_neighbor_t *alloc_neighbor(const opener_nb_lldp_neighbor_t *key)
{
    opener_nb_lldp_neighbor_t *existing = find_neighbor_by_key(key);
    if (existing != NULL) {
        return existing;
    }

    for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
        opener_nb_lldp_neighbor_t *n = &g_lldp.neighbors[i];
        if (!n->active) {
            memset(n, 0, sizeof(*n));
            n->active = true;
            n->instance = g_lldp.next_instance++;
            if (g_lldp.next_instance == 0u) {
                g_lldp.next_instance = 1u;
            }
            n->eth_link_instance = (uint8_t)OPENER_NB_LLDP_ETH_LINK_INSTANCE;
            n->chassis_len = key->chassis_len;
            n->port_len = key->port_len;
            if (n->chassis_len > 0) {
                memcpy(n->chassis, key->chassis, n->chassis_len);
            }
            if (n->port_len > 0) {
                memcpy(n->port, key->port, n->port_len);
            }
            n->last_change_sec = sys_up_time_sec();
            mark_db_changed();
            return n;
        }
    }
    return NULL;
}

static opener_nb_lldp_neighbor_t *neighbor_by_instance(uint16_t inst)
{
    if (inst == 0u) {
        return NULL;
    }
    for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
        opener_nb_lldp_neighbor_t *n = &g_lldp.neighbors[i];
        if (n->active && n->instance == inst) {
            return n;
        }
    }
    return NULL;
}

static uint16_t neighbor_count_locked(void)
{
    uint16_t count = 0;
    for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
        if (g_lldp.neighbors[i].active) {
            ++count;
        }
    }
    return count;
}

static void derive_neighbor_mac(opener_nb_lldp_neighbor_t *n)
{
    if (n->mac[0] | n->mac[1] | n->mac[2] | n->mac[3] | n->mac[4] | n->mac[5]) {
        return;
    }
    if (n->chassis_len >= 7 && n->chassis[0] == 4u) {
        memcpy(n->mac, n->chassis + 1, 6);
        return;
    }
    if (n->port_len >= 7 && n->port[0] == 3u) {
        memcpy(n->mac, n->port + 1, 6);
    }
}

static bool lldp_id_subtype_is_string(uint8_t subtype)
{
    return subtype == 1u || subtype == 2u || subtype == 5u || subtype == 7u;
}

static bool lldp_string_is_printable(const char *s, size_t max_len)
{
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    const size_t n = opener_nb_strnlen(s, max_len);
    if (n == 0u) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c < 0x20u || c > 0x7Eu) {
            return false;
        }
    }
    return true;
}

static bool lldp_copy_id_string(char *dst, size_t cap, const uint8_t *id, size_t id_len)
{
    if (id_len <= 1u || cap == 0u || !lldp_id_subtype_is_string(id[0])) {
        return false;
    }
    const size_t len = id_len - 1u;
    if (len >= cap) {
        return false;
    }
    memcpy(dst, id + 1, len);
    dst[len] = '\0';
    return lldp_string_is_printable(dst, len);
}

static void derive_neighbor_label(opener_nb_lldp_neighbor_t *n)
{
    n->label[0] = '\0';

    if (lldp_string_is_printable(n->odva_label, sizeof(n->odva_label) - 1u)) {
        strncpy(n->label, n->odva_label, sizeof(n->label) - 1u);
        n->label[sizeof(n->label) - 1u] = '\0';
        return;
    }
    if (lldp_copy_id_string(n->label, sizeof(n->label), n->port, n->port_len)) {
        return;
    }
    if (n->chassis_len > 1u && n->chassis[0] == 6u) {
        const size_t len = n->chassis_len - 1u;
        if (len < sizeof(n->label)) {
            memcpy(n->label, n->chassis + 1, len);
            n->label[len] = '\0';
            if (lldp_string_is_printable(n->label, len)) {
                return;
            }
        }
    }
    n->label[0] = '\0';
    if (lldp_string_is_printable(n->sys_name, sizeof(n->sys_name) - 1u)) {
        strncpy(n->label, n->sys_name, sizeof(n->label) - 1u);
        n->label[sizeof(n->label) - 1u] = '\0';
    }
}

static const char *neighbor_log_label(const opener_nb_lldp_neighbor_t *n)
{
    return lldp_string_is_printable(n->label, sizeof(n->label) - 1u) ? n->label : "?";
}

static size_t write_cip_short_string(uint8_t *dst, const char *text, size_t cap)
{
    size_t len = 0;
    if (text != NULL) {
        len = strlen(text);
    }
    if (len > 255u) {
        len = 255u;
    }
    if (len + 1u > cap) {
        return 0;
    }
    dst[0] = (uint8_t)len;
    if (len > 0) {
        memcpy(dst + 1, text, len);
    }
    return 1u + len;
}

static uint16_t read_tlv_u16_be(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void parse_lldp_tlvs(const uint8_t *data, size_t len, opener_nb_lldp_neighbor_t *parsed)
{
    size_t off = 0;
    memset(parsed, 0, sizeof(*parsed));
    parsed->eth_link_instance = (uint8_t)OPENER_NB_LLDP_ETH_LINK_INSTANCE;

    while (off + 2u <= len) {
        const uint16_t hdr = ((uint16_t)data[off] << 8) | (uint16_t)data[off + 1];
        off += 2u;
        const uint8_t type = (uint8_t)((hdr >> 9) & 0x7Fu);
        const uint16_t tlen = (uint16_t)(hdr & 0x01FFu);
        if (type == 0u && tlen == 0u) {
            break;
        }
        if (off + (size_t)tlen > len) {
            break;
        }
        const uint8_t *val = data + off;
        off += (size_t)tlen;

        switch (type) {
        case 1:
            if (tlen > 0u && tlen <= sizeof(parsed->chassis)) {
                parsed->chassis_len = (uint8_t)tlen;
                memcpy(parsed->chassis, val, tlen);
            }
            break;
        case 2:
            if (tlen > 0u && tlen <= sizeof(parsed->port)) {
                parsed->port_len = (uint8_t)tlen;
                memcpy(parsed->port, val, tlen);
            }
            break;
        case 3:
            if (tlen >= 2u) {
                parsed->ttl_initial = read_tlv_u16_be(val);
                parsed->ttl_remaining = parsed->ttl_initial;
            }
            break;
        case 5:
            if (tlen > 0u && tlen < sizeof(parsed->sys_name)) {
                char tmp[65];
                memcpy(tmp, val, tlen);
                tmp[tlen] = '\0';
                if (lldp_string_is_printable(tmp, tlen)) {
                    memcpy(parsed->sys_name, tmp, tlen + 1u);
                }
            }
            break;
        case 7:
            if (tlen >= 4u) {
                memcpy(parsed->sys_cap, val, 4);
            }
            break;
        case 8:
            /* IEEE 802.1AB: IPv4 mgmt addr is len=5, subtype=1, then 4 octets. */
            if (parsed->mgmt_count == 0u && tlen >= 6u && val[0] == 5u && val[1] == 1u) {
                parsed->mgmt_count = 1u;
                parsed->mgmt_ip[0] = val[2];
                parsed->mgmt_ip[1] = val[3];
                parsed->mgmt_ip[2] = val[4];
                parsed->mgmt_ip[3] = val[5];
            }
            break;
        case 127:
            if (tlen >= 4u && val[0] == 0x00u && val[1] == 0x12u && val[2] == 0x0Fu) {
                const uint8_t subtype = val[3];
                const uint8_t *payload = val + 4;
                const size_t plen = (size_t)tlen - 4u;
                switch (subtype) {
                case 2:
                    if (plen >= 6u) {
                        memcpy(parsed->mac, payload, 6);
                    }
                    break;
                case 3:
                    if (plen > 0u && plen < sizeof(parsed->odva_label)) {
                        memcpy(parsed->odva_label, payload, plen);
                        parsed->odva_label[plen] = '\0';
                    }
                    break;
                case 9:
                    if (plen >= OPENER_NB_LLDP_ID_KEY_LEN) {
                        memcpy(parsed->cip_id, payload, OPENER_NB_LLDP_ID_KEY_LEN);
                        parsed->has_cip_id = true;
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

}

/*
 * Enet (CustomNetDoRX / pProcessLLDP) must not call RTOS locks, printf, or use
 * large stack frames. Copy raw frame bytes into a lock-free SPSC queue only.
 */
static void opener_nb_lldp_rx_capture(const uint8_t *frame, uint16_t ocount)
{
    if (!s_lldp_hooks_armed || !g_lldp.initialized || !lldp_rx_enabled() || frame == NULL ||
        ocount < 14u) {
        return;
    }

    const uint16_t pdu_len = (uint16_t)(ocount - 14u);
    if (pdu_len == 0u || pdu_len > OPENER_NB_LLDP_MAX_RX_PDU) {
        return;
    }

    const uint8_t head = s_rx_head;
    const uint8_t next = (uint8_t)((head + 1u) % OPENER_NB_LLDP_RX_QUEUE_DEPTH);
    if (next == s_rx_tail) {
        ++s_rx_drop_count;
        return;
    }

    opener_nb_lldp_rx_slot_t *slot = &s_rx_queue[head];
    memcpy(slot->src_mac, frame + 6, 6);
    slot->pdu_len = pdu_len;
    memcpy(slot->pdu, frame + 14, pdu_len);
    OPENER_NB_LLDP_DMB();
    s_rx_head = next;
}

static void opener_nb_lldp_process_rx_slot(const opener_nb_lldp_rx_slot_t *slot)
{
    if (slot == NULL || slot->pdu_len == 0u) {
        return;
    }

    ++g_lldp.rx_frame_count;
    OPENER_NB_LLDP_LOG_VERBOSE_NOTE("LLDP: RX frame #%u from %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                               (unsigned)g_lldp.rx_frame_count,
                               slot->src_mac[0], slot->src_mac[1], slot->src_mac[2],
                               slot->src_mac[3], slot->src_mac[4], slot->src_mac[5]);

    opener_nb_lldp_neighbor_t *parsed = &s_rx_parse_scratch;
    memset(parsed, 0, sizeof(*parsed));
    parse_lldp_tlvs(slot->pdu, slot->pdu_len, parsed);
    if (parsed->chassis_len == 0u && parsed->port_len == 0u) {
        return;
    }

    derive_neighbor_mac(parsed);
    if (!(parsed->mac[0] | parsed->mac[1] | parsed->mac[2] | parsed->mac[3] | parsed->mac[4] |
          parsed->mac[5])) {
        memcpy(parsed->mac, slot->src_mac, 6);
    }

    uint16_t log_inst = 0u;
    uint8_t log_mac[6] = {0};
    char log_label[65];
    log_label[0] = '\0';

    {
        OSCriticalSectionObj lock(g_lldp.crit);
        const bool is_new_neighbor = (find_neighbor_by_key(parsed) == NULL);
        opener_nb_lldp_neighbor_t *n = alloc_neighbor(parsed);
        if (n == NULL) {
            return;
        }

        if (parsed->ttl_remaining > 0u) {
            n->ttl_remaining = parsed->ttl_remaining;
            n->ttl_initial = parsed->ttl_remaining;
        }
        if (parsed->chassis_len > 0u) {
            n->chassis_len = parsed->chassis_len;
            memcpy(n->chassis, parsed->chassis, n->chassis_len);
        }
        if (parsed->port_len > 0u) {
            n->port_len = parsed->port_len;
            memcpy(n->port, parsed->port, n->port_len);
        }
        if (parsed->sys_name[0] != '\0') {
            strncpy(n->sys_name, parsed->sys_name, sizeof(n->sys_name) - 1u);
            n->sys_name[sizeof(n->sys_name) - 1u] = '\0';
        }
        if (parsed->odva_label[0] != '\0') {
            strncpy(n->odva_label, parsed->odva_label, sizeof(n->odva_label) - 1u);
            n->odva_label[sizeof(n->odva_label) - 1u] = '\0';
        }
        if (parsed->mac[0] | parsed->mac[1] | parsed->mac[2] | parsed->mac[3] | parsed->mac[4] | parsed->mac[5]) {
            memcpy(n->mac, parsed->mac, 6);
        } else {
            derive_neighbor_mac(n);
        }
        derive_neighbor_label(n);
        if (parsed->sys_cap[0] | parsed->sys_cap[1] | parsed->sys_cap[2] | parsed->sys_cap[3]) {
            memcpy(n->sys_cap, parsed->sys_cap, 4);
        }
        if (parsed->mgmt_count > 0u) {
            n->mgmt_count = parsed->mgmt_count;
            memcpy(n->mgmt_ip, parsed->mgmt_ip, 4);
        }
        if (parsed->has_cip_id) {
            memcpy(n->cip_id, parsed->cip_id, OPENER_NB_LLDP_ID_KEY_LEN);
            n->has_cip_id = true;
        }
        n->last_change_sec = sys_up_time_sec();
        mark_db_changed();

        if (is_new_neighbor) {
            log_inst = n->instance;
            memcpy(log_mac, n->mac, 6);
            strncpy(log_label, neighbor_log_label(n), sizeof(log_label) - 1u);
            log_label[sizeof(log_label) - 1u] = '\0';
        }
    }

    if (log_inst != 0u) {
        OPENER_NB_LLDP_LOG_NEIGHBOR("LLDP: neighbor inst=%u %02X:%02X:%02X:%02X:%02X:%02X %s\r\n",
                               (unsigned)log_inst, log_mac[0], log_mac[1], log_mac[2], log_mac[3],
                               log_mac[4], log_mac[5], log_label);
    }
}

static void opener_nb_lldp_drain_rx_queue(void)
{
    while (s_rx_tail != s_rx_head) {
        const uint8_t tail = s_rx_tail;
        const opener_nb_lldp_rx_slot_t *slot = &s_rx_queue[tail];
        uint16_t pdu_len = slot->pdu_len;
        if (pdu_len > OPENER_NB_LLDP_MAX_RX_PDU) {
            pdu_len = OPENER_NB_LLDP_MAX_RX_PDU;
        }
        memcpy(s_rx_work_slot.src_mac, slot->src_mac, 6);
        s_rx_work_slot.pdu_len = pdu_len;
        memcpy(s_rx_work_slot.pdu, slot->pdu, pdu_len);
        OPENER_NB_LLDP_DMB();
        s_rx_tail = (uint8_t)((tail + 1u) % OPENER_NB_LLDP_RX_QUEUE_DEPTH);
        opener_nb_lldp_process_rx_slot(&s_rx_work_slot);
    }
}

#ifdef ALLOW_CUSTOM_NET_DO_RX
static inline bool opener_nb_fnptr_is_callable(uintptr_t fn)
{
    return fn != 0u && ((fn & 0x1u) != 0u);
}

extern "C" {
static int opener_nb_lldp_net_do_rx(PoolPtr pp, uint16_t ocount, int if_num)
{
    (void)if_num;
    ++g_lldp_net_rx_calls;

    if (!s_lldp_hooks_armed || !g_lldp.initialized || !lldp_rx_enabled() || pp == 0 || ocount < 14u) {
        return 0;
    }

    const uint8_t *frame = (const uint8_t *)pp->pData;
    if (frame_ethertype(frame, ocount) != (uint16_t)OPENER_NB_LLDP_ETHERTYPE) {
        return 0;
    }

    ++g_lldp_net_rx_lldp;
    opener_nb_lldp_rx_capture(frame, ocount);
    return 1;
}
}

static void opener_nb_lldp_register_net_rx(void)
{
    s_prev_custom_net_do_rx = SetCustomNetDoRX(opener_nb_lldp_net_do_rx);
}

static void opener_nb_lldp_unregister_net_rx(void)
{
    if (CustomNetDoRX == opener_nb_lldp_net_do_rx) {
        SetCustomNetDoRX(s_prev_custom_net_do_rx);
    }
    s_prev_custom_net_do_rx = NULL;
}
#endif

extern "C" void opener_nb_lldp_rx_frame_c(PoolPtr pp)
{
    if (pp == 0) {
        return;
    }
    opener_nb_lldp_rx_capture((const uint8_t *)pp->pData, (uint16_t)pp->usedsize);
}

static void apply_entity_config(void)
{
    if (g_lldp_entity == NULL) {
        return;
    }
    g_lldp_entity->ApplyTiming(g_lldp.tx_interval, g_lldp.tx_hold);
    g_lldp_entity->SetGlobalEnable(lldp_tx_enabled());
}

static void opener_nb_lldp_arm_hooks(void)
{
    if (s_lldp_hooks_armed || !g_lldp.initialized) {
        return;
    }

#if OPENER_NB_LLDP_RX_ENABLE
    lldp_enable_multicast_rx(g_lldp.ifnum);
#if defined(ALLOW_CUSTOM_NET_DO_RX)
    opener_nb_lldp_register_net_rx();
#endif
#endif

    apply_entity_config();

#if OPENER_NB_LLDP_TX_ENABLE
    if (g_lldp_entity != NULL) {
        g_lldp_entity->RequestFastTransmit();
        opener_nb_lldp_schedule_tx(2u);
    }
#endif

    s_lldp_hooks_armed = true;

    OPENER_NB_LLDP_LOG_NOTE("LLDP: armed interval=%u hold=%u enable=0x%02X tx=%s net_rx=%s if=%d\r\n",
                       (unsigned)g_lldp.tx_interval, (unsigned)g_lldp.tx_hold,
                       (unsigned)g_lldp.enable,
#if OPENER_NB_LLDP_TX_ENABLE
                       "enabled",
#else
                       "disabled",
#endif
#if !OPENER_NB_LLDP_RX_ENABLE
                       "disabled",
#elif defined(ALLOW_CUSTOM_NET_DO_RX)
                       "CustomNetDoRX+poll",
#else
                       "poll",
#endif
                       g_lldp.ifnum);
}

static void copy_identity_from_cfg(const OpenerNbLldpIdentity *cfg)
{
    if (cfg == NULL) {
        return;
    }
    g_lldp.vendor_id = cfg->vendor_id;
    g_lldp.device_type = cfg->device_type;
    g_lldp.product_code = cfg->product_code;
    g_lldp.major_revision = cfg->major_revision;
    g_lldp.minor_revision = cfg->minor_revision;
    g_lldp.serial_number = cfg->serial_number;
    g_lldp.ip_address = cfg->ip_address;
    memcpy(g_lldp.mac_address, cfg->mac_address, 6);
    if (cfg->host_name != NULL) {
        strncpy(g_lldp.host_name, cfg->host_name, sizeof(g_lldp.host_name) - 1u);
        g_lldp.host_name[sizeof(g_lldp.host_name) - 1u] = '\0';
    }
    if (cfg->product_name != NULL) {
        strncpy(g_lldp.product_name, cfg->product_name, sizeof(g_lldp.product_name) - 1u);
        g_lldp.product_name[sizeof(g_lldp.product_name) - 1u] = '\0';
    }
}

extern "C" {

bool OpenerNbLldpInit(int ifnum, const OpenerNbLldpIdentity *cfg)
{
    if (g_lldp.initialized) {
        OpenerNbLldpUpdateIdentity(cfg);
        return true;
    }

    g_lldp.ifnum = ifnum;
    g_lldp.initialized = false;
    g_lldp.last_link_active = false;
    g_lldp.next_instance = 1u;
    g_lldp.rx_frame_count = 0u;
    g_lldp.db_last_change_sec = sys_up_time_sec();
    memset(g_lldp.neighbors, 0, sizeof(g_lldp.neighbors));
    g_lldp.crit.Init();
    s_rx_head = 0u;
    s_rx_tail = 0u;
    s_rx_drop_count = 0u;
    strncpy(g_lldp.interface_label, OPENER_NB_LLDP_INTERFACE_LABEL, sizeof(g_lldp.interface_label) - 1u);

    lldp_nv_load();
    copy_identity_from_cfg(cfg);

    InterfaceBlock *ib = GetInterfaceBlock(ifnum);
    if (ib == NULL) {
        return false;
    }

#if OPENER_NB_LLDP_TX_ENABLE
    g_lldp_entity = new ScipLLDPEntity(*ib);
    if (g_lldp_entity == NULL) {
        return false;
    }
#else
    g_lldp_entity = NULL;
#endif

    g_lldp.last_link_active = ib->LinkActive() ? true : false;
    g_lldp.initialized = true;
    s_lldp_hooks_armed = false;

    OPENER_NB_LLDP_LOG_NOTE("LLDP: configured interval=%u hold=%u enable=0x%02X (hooks deferred)\r\n",
                       (unsigned)g_lldp.tx_interval, (unsigned)g_lldp.tx_hold,
                       (unsigned)g_lldp.enable);
    return true;
}

void OpenerNbLldpShutdown(void)
{
    if (!g_lldp.initialized) {
        return;
    }
#if OPENER_NB_LLDP_RX_ENABLE && defined(ALLOW_CUSTOM_NET_DO_RX)
    if (s_lldp_hooks_armed) {
        opener_nb_lldp_unregister_net_rx();
    }
#endif
    pProcessLLDP = NULL;
    delete g_lldp_entity;
    g_lldp_entity = NULL;
    g_lldp.initialized = false;
    s_lldp_hooks_armed = false;
}

void OpenerNbLldpPoll(void)
{
    if (!g_lldp.initialized) {
        return;
    }

    if (!s_lldp_hooks_armed) {
        opener_nb_lldp_arm_hooks();
    }

#if OPENER_NB_LLDP_RX_ENABLE
    opener_nb_lldp_drain_rx_queue();
#endif

#if OPENER_NB_LLDP_TX_ENABLE
    OpenerNbLldpPoll_tx();
#endif

    InterfaceBlock *ib = GetInterfaceBlock(g_lldp.ifnum);
    const bool link_active = (ib != NULL && ib->LinkActive()) ? true : false;
    if (link_active && !g_lldp.last_link_active && g_lldp_entity != NULL && lldp_tx_enabled()) {
        g_lldp_entity->RebuildPacket();
        g_lldp_entity->RequestFastTransmit();
        opener_nb_lldp_schedule_tx(1u);
    }
    g_lldp.last_link_active = link_active;

    static uint32_t last_ttl_sec = 0;
    const uint32_t now = sys_up_time_sec();
    if (now != last_ttl_sec) {
        last_ttl_sec = now;
        OSCriticalSectionObj lock(g_lldp.crit);
        bool changed = false;
        for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
            opener_nb_lldp_neighbor_t *n = &g_lldp.neighbors[i];
            if (!n->active) {
                continue;
            }
            if (n->ttl_remaining > 0u) {
                --n->ttl_remaining;
            }
            if (n->ttl_remaining == 0u) {
                OPENER_NB_LLDP_LOG_NEIGHBOR("LLDP: neighbor inst=%u expired (%s)\r\n",
                                       (unsigned)n->instance, neighbor_log_label(n));
                memset(n, 0, sizeof(*n));
                changed = true;
            }
        }
        if (changed) {
            mark_db_changed();
        }
    }
}

void OpenerNbLldpUpdateIdentity(const OpenerNbLldpIdentity *cfg)
{
    if (!g_lldp.initialized || cfg == NULL) {
        return;
    }
    copy_identity_from_cfg(cfg);
    if (g_lldp_entity != NULL && lldp_tx_enabled()) {
        g_lldp_entity->RebuildPacket();
        g_lldp_entity->RequestFastTransmit();
        opener_nb_lldp_schedule_tx(1u);
    }
}

uint32_t OpenerNbLldpGetRxFrameCount(void)
{
    return g_lldp.rx_frame_count;
}

bool OpenerNbLldpBuildMgmtAttr(uint8_t attr, uint8_t *rsp, size_t cap, size_t *len_out)
{
    if (rsp == NULL || len_out == NULL) {
        return false;
    }
    size_t o = 0;
    switch (attr) {
    case 1: {
        const uint16_t array_len = lldp_enable_array_length();
        if (cap < 2u + (size_t)array_len) {
            return false;
        }
        /* OpENer/FusionCore: UINT16 count + one enable byte per Ethernet Link instance. */
        opener_nb_le_write_u16(rsp + o, array_len);
        o += 2;
        for (uint16_t i = 0; i < array_len; ++i) {
            rsp[o++] = lldp_enable_wire_byte(i);
        }
        break;
    }
    case 2:
        if (cap < 2u) {
            return false;
        }
        opener_nb_le_write_u16(rsp + o, g_lldp.tx_interval);
        o += 2;
        break;
    case 3:
        if (cap < 1u) {
            return false;
        }
        rsp[o++] = g_lldp.tx_hold;
        break;
    case 4:
        if (cap < 2u) {
            return false;
        }
        opener_nb_le_write_u16(rsp + o, OPENER_NB_LLDP_DATASTORE_DATA_TABLE);
        o += 2;
        break;
    case 5:
        if (cap < 4u) {
            return false;
        }
        opener_nb_le_write_u32(rsp + o, g_lldp.db_last_change_sec);
        o += 4;
        break;
    default:
        return false;
    }
    *len_out = o;
    return true;
}

bool OpenerNbLldpBuildMgmtAll(uint8_t *rsp, size_t cap, size_t *len_out)
{
    if (rsp == NULL || len_out == NULL) {
        return false;
    }
    size_t o = 0;
    size_t n = 0;
    for (uint8_t attr = 1; attr <= 5; ++attr) {
        if (!OpenerNbLldpBuildMgmtAttr(attr, rsp + o, cap - o, &n)) {
            return false;
        }
        o += n;
    }
    *len_out = o;
    return true;
}

static opener_nb_status_t set_enable_attr(const uint8_t *data, size_t len, uint8_t *cip_status_out)
{
    const uint16_t expected_len = lldp_enable_array_length();
    size_t enable_off = 0;
    uint16_t array_len = 0;

    if (len >= 2u + (size_t)expected_len) {
        array_len = opener_nb_le_read_u16(data);
        enable_off = 2u;
    } else if (len >= 1u + (size_t)expected_len && data[0] == (uint8_t)expected_len) {
        array_len = (uint16_t)data[0];
        enable_off = 1u;
    } else {
        *cip_status_out = OPENER_NB_CIP_ERR_INVALID_PARAM;
        return OPENER_NB_ERR_INVALID_ARG;
    }

    if (array_len != expected_len || len < enable_off + (size_t)array_len) {
        *cip_status_out = OPENER_NB_CIP_ERR_INVALID_ATTR_VALUE;
        return OPENER_NB_ERR_INVALID_ARG;
    }

    const uint8_t old = g_lldp.enable;
    if (array_len > 0u && data[enable_off] != 0u) {
        g_lldp.enable = OPENER_NB_LLDP_GLOBAL_BIT | OPENER_NB_LLDP_PORT1_BIT;
    } else {
        g_lldp.enable = 0u;
    }
    if ((old & OPENER_NB_LLDP_GLOBAL_BIT) != 0u &&
        (g_lldp.enable & OPENER_NB_LLDP_GLOBAL_BIT) == 0u) {
        OSCriticalSectionObj lock(g_lldp.crit);
        clear_neighbors_locked();
    }
    mark_db_changed();
    lldp_nv_save();
    apply_entity_config();
    return OPENER_NB_OK;
}

static opener_nb_status_t set_interval_attr(const uint8_t *data, size_t len, uint8_t *cip_status_out)
{
    if (len < 2u) {
        *cip_status_out = OPENER_NB_CIP_ERR_INVALID_PARAM;
        return OPENER_NB_ERR_INVALID_ARG;
    }
    const uint16_t interval = opener_nb_le_read_u16(data);
    if (interval < 5u || interval > 32768u) {
        *cip_status_out = OPENER_NB_CIP_ERR_INVALID_ATTR_VALUE;
        return OPENER_NB_ERR_INVALID_ARG;
    }
    g_lldp.tx_interval = interval;
    mark_db_changed();
    lldp_nv_save();
    apply_entity_config();
    return OPENER_NB_OK;
}

static opener_nb_status_t set_hold_attr(const uint8_t *data, size_t len, uint8_t *cip_status_out)
{
    if (len < 1u) {
        *cip_status_out = OPENER_NB_CIP_ERR_INVALID_PARAM;
        return OPENER_NB_ERR_INVALID_ARG;
    }
    if (data[0] < 1u || data[0] > 100u) {
        *cip_status_out = OPENER_NB_CIP_ERR_INVALID_ATTR_VALUE;
        return OPENER_NB_ERR_INVALID_ARG;
    }
    g_lldp.tx_hold = data[0];
    mark_db_changed();
    lldp_nv_save();
    apply_entity_config();
    return OPENER_NB_OK;
}

opener_nb_status_t OpenerNbLldpSetMgmtAttr(uint8_t attr, const uint8_t *data, size_t len,
                                      uint8_t *cip_status_out)
{
    if (cip_status_out == NULL) {
        return OPENER_NB_ERR_INVALID_ARG;
    }
    switch (attr) {
    case 1:
        return set_enable_attr(data, len, cip_status_out);
    case 2:
        return set_interval_attr(data, len, cip_status_out);
    case 3:
        return set_hold_attr(data, len, cip_status_out);
    case 4:
    case 5:
        *cip_status_out = OPENER_NB_CIP_ERR_ATTR_NOT_SETTABLE;
        return OPENER_NB_ERR_NOT_SUPPORTED;
    default:
        *cip_status_out = OPENER_NB_CIP_ERR_ATTR_NOT_SETTABLE;
        return OPENER_NB_ERR_NOT_SUPPORTED;
    }
}

uint16_t OpenerNbLldpGetDataTableMaxInstance(void)
{
    OSCriticalSectionObj lock(g_lldp.crit);
    uint16_t max_inst = 0u;
    for (size_t i = 0; i < OPENER_NB_LLDP_MAX_NEIGHBORS; ++i) {
        const opener_nb_lldp_neighbor_t *n = &g_lldp.neighbors[i];
        if (n->active && n->instance > max_inst) {
            max_inst = n->instance;
        }
    }
    return max_inst;
}

uint16_t OpenerNbLldpGetNeighborCount(void)
{
    OSCriticalSectionObj lock(g_lldp.crit);
    return neighbor_count_locked();
}

bool OpenerNbLldpDataTableInstanceValid(uint16_t inst)
{
    OSCriticalSectionObj lock(g_lldp.crit);
    if (neighbor_by_instance(inst) != NULL) {
        return true;
    }
    return data_table_placeholder_instance_locked(inst);
}

bool OpenerNbLldpBuildDataTableAttr(uint16_t inst, uint8_t attr, uint8_t *rsp, size_t cap,
                                     size_t *len_out)
{
    if (rsp == NULL || len_out == NULL) {
        return false;
    }

    opener_nb_lldp_neighbor_t placeholder;
    opener_nb_lldp_neighbor_t *n = NULL;

    OSCriticalSectionObj lock(g_lldp.crit);
    n = neighbor_by_instance(inst);
    if (n == NULL) {
        if (!data_table_placeholder_instance_locked(inst)) {
            return false;
        }
        fill_data_table_placeholder(&placeholder);
        n = &placeholder;
    }

    size_t o = 0;
    switch (attr) {
    case 1:
        if (cap < 2u) {
            return false;
        }
        opener_nb_le_write_u16(rsp + o, n->eth_link_instance);
        o += 2;
        break;
    case 2:
        if (cap < 6u) {
            return false;
        }
        memcpy(rsp + o, n->mac, 6);
        o += 6;
        break;
    case 3: {
        const char *label = n->label;
        if (label[0] == '\0') {
            if (cap < 1u) {
                return false;
            }
            rsp[o++] = 0u;
        } else {
            const size_t nlen = write_cip_short_string(rsp + o, label, cap - o);
            if (nlen == 0u) {
                return false;
            }
            o += nlen;
        }
        break;
    }
    case 4:
        if (cap < 2u) {
            return false;
        }
        opener_nb_le_write_u16(rsp + o, n->ttl_remaining);
        o += 2;
        break;
    case 5:
        if (cap < 4u) {
            return false;
        }
        {
            const uint16_t sys_caps = (uint16_t)(((uint16_t)n->sys_cap[0] << 8) | n->sys_cap[1]);
            const uint16_t enabled = (uint16_t)(((uint16_t)n->sys_cap[2] << 8) | n->sys_cap[3]);
            opener_nb_le_write_u16(rsp + o, sys_caps);
            o += 2;
            opener_nb_le_write_u16(rsp + o, enabled);
            o += 2;
        }
        break;
    case 6: {
        const size_t need = 1u + ((size_t)n->mgmt_count * 4u);
        if (cap < need) {
            return false;
        }
        rsp[o++] = n->mgmt_count;
        if (n->mgmt_count > 0u) {
            opener_nb_ipv4_t mgmt_ip;
            memcpy(mgmt_ip.octets, n->mgmt_ip, 4);
            opener_nb_le_write_u32(rsp + o, opener_nb_ipv4_to_u32_be(mgmt_ip));
            o += 4;
        }
        break;
    }
    case 7:
        if (cap < OPENER_NB_LLDP_CIP_IDENT_ATTR_LEN) {
            return false;
        }
        write_cip_identification_attr(n, rsp, &o);
        break;
    case 8:
        if (cap < 4u) {
            return false;
        }
        memcpy(rsp + o, n->eth_cap, 4);
        o += 4;
        break;
    case 9:
        if (cap < 4u) {
            return false;
        }
        opener_nb_le_write_u32(rsp + o, n->last_change_sec);
        o += 4;
        break;
    default:
        return false;
    }

    *len_out = o;
    return true;
}

} /* extern "C" */

#endif /* OPENER_NB_LLDP */
