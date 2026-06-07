#include "bbb_cpsw_raw.h"

#include "bbb_hw.h"

#include "armv7a/cache.h"
#include "beaglebone.h"
#include "cpsw.h"
#include "mdio.h"
#include "phy.h"
#include "soc_AM335x.h"

#include <string.h>

#define LEAP_ETHERTYPE_DEVELOPMENT 0x88B6u

#define MDIO_INPUT_HZ              125000000u
#define MDIO_OUTPUT_HZ             1000000u
#define BBB_PHY_ADDR               0u
#define BBB_DMA_CACHE_LINE         64u
#define BBB_CPPI_RAM_SIZE          8192u

#define CPDMA_DESC_OWNER           0x20000000u
#define CPDMA_DESC_SOP             0x80000000u
#define CPDMA_DESC_EOP             0x40000000u
#define CPDMA_DESC_EOQ             0x10000000u
#define CPDMA_DESC_LEN_MASK        0x0000FFFFu
#define BBB_CPSW_PHY_PORT          1u
#define BBB_CPSW_ALE_SELF_IDX      0u
#define BBB_CPSW_ALE_BCAST_IDX     1u
#define BBB_CPSW_ALE_PEER_IDX      2u

#define CPSW_PORT_MASK_HOST        0x01u
#define CPSW_PORT_MASK_SLAVE1      0x02u
#define CPSW_PORT_MASK_SLAVE2      0x04u

#define ALE_ENTRY_WORDS            3u
#define ALE_ENTRY_UCAST            0x10u
#define ALE_ENTRY_MCAST            0xD0u
#define ALE_UCAST_ENTRY_TYPE       7u
#define ALE_UCAST_ENTRY_PORT       8u
#define ALE_UCAST_ENTRY_PORT_SHIFT 2u
#define ALE_MCAST_ENTRY_TYPE       7u
#define ALE_MCAST_ENTRY_PORTMASK   8u
#define ALE_MCAST_PORTMASK_SHIFT   2u

typedef struct CpdmaDesc
{
    volatile struct CpdmaDesc* next;
    volatile uint32_t          bufptr;
    volatile uint32_t          bufoff_len;
    volatile uint32_t          flags_pktlen;
} CpdmaDesc;

static volatile CpdmaDesc* g_rx_desc;
static volatile CpdmaDesc* g_tx_desc;
static uint8_t             g_rx_frame[1600] __attribute__((aligned(64)));
static uint8_t             g_tx_frame[1600] __attribute__((aligned(64)));

#define BBB_RX_QUEUE_SLOTS 8u

typedef struct BbbRxQueueSlot
{
    uint8_t  src_mac[6];
    uint16_t length;
    uint8_t  payload[BBB_CPSW_MAX_PAYLOAD];
} BbbRxQueueSlot;

static BbbRxQueueSlot g_rx_queue[BBB_RX_QUEUE_SLOTS];
static unsigned       g_rx_queue_head;
static unsigned       g_rx_queue_tail;
static unsigned       g_rx_queue_count;
static uint32_t       g_rx_queue_drops;

static void bbb_dma_cache_clean(const void* addr, size_t length)
{
    unsigned int start = (unsigned int)addr;
    unsigned int end;
    unsigned int size;

    if (addr == 0 || length == 0u) {
        return;
    }

    start &= ~(BBB_DMA_CACHE_LINE - 1u);
    end   = ((unsigned int)addr + (unsigned int)length +
             (BBB_DMA_CACHE_LINE - 1u)) &
            ~(BBB_DMA_CACHE_LINE - 1u);
    size = end - start;
    if (size != 0u) {
        CacheDataCleanBuff(start, size);
    }
}

static void bbb_dma_cache_invalidate(const void* addr, size_t length)
{
    unsigned int start = (unsigned int)addr;
    unsigned int end;
    unsigned int size;

    if (addr == 0 || length == 0u) {
        return;
    }

    start &= ~(BBB_DMA_CACHE_LINE - 1u);
    end   = ((unsigned int)addr + (unsigned int)length +
             (BBB_DMA_CACHE_LINE - 1u)) &
            ~(BBB_DMA_CACHE_LINE - 1u);
    size = end - start;
    if (size != 0u) {
        CacheDataInvalidateBuff(start, size);
    }
}

void bbb_cpsw_raw_debug_status(void)
{
    bbb_uart_puts("\ncpdma rx_flags=0x");
    bbb_uart_put_hex32(g_rx_desc != 0 ? g_rx_desc->flags_pktlen : 0u);
    bbb_uart_puts(" hdp=0x");
    bbb_uart_put_hex32(*(volatile uint32_t*)(SOC_CPSW_CPDMA_REGS + 0x220u));
    bbb_uart_puts(" stat=0x");
    bbb_uart_put_hex32(*(volatile uint32_t*)(SOC_CPSW_CPDMA_REGS + 0x24u));
    bbb_uart_puts(" mdio_link=0x");
    bbb_uart_put_hex32(MDIOPhyLinkStatusGet(SOC_CPSW_MDIO_REGS));
    bbb_uart_puts("\n");
}

static int mac_is_broadcast(const uint8_t* mac)
{
    unsigned i;

    for (i = 0u; i < 6u; i++) {
        if (mac[i] != 0xFFu) {
            return 0;
        }
    }

    return 1;
}

static int mac_matches(const uint8_t* a, const uint8_t* b)
{
    return memcmp(a, b, 6u) == 0;
}

static void bbb_mac_read_wire_order(uint8_t* mac)
{
    unsigned char raw[6];
    unsigned      i;

    if (mac == 0) {
        return;
    }

    /*
     * CONTROL_MAC_ID_* order from EVMMACAddrGet() is not Linux/eth0 order.
     * StarterWare lwIP reverses into eth_addr; match that for frames and ALE.
     */
    EVMMACAddrGet(0u, raw);
    for (i = 0u; i < 6u; i++) {
        mac[i] = raw[5u - i];
    }
}

static void cpsw_ale_set_mac_bytes(unsigned int* ale_entry, const uint8_t* mac)
{
    unsigned i;

    for (i = 0u; i < 6u; i++) {
        ((uint8_t*)ale_entry)[i] = mac[5u - i];
    }
}

static void cpsw_ale_set_unicast(unsigned index, unsigned port, const uint8_t* mac)
{
    unsigned int ale_entry[ALE_ENTRY_WORDS] = { 0u, 0u, 0u };

    cpsw_ale_set_mac_bytes(ale_entry, mac);
    ((uint8_t*)ale_entry)[ALE_UCAST_ENTRY_TYPE] = ALE_ENTRY_UCAST;
    ((uint8_t*)ale_entry)[ALE_UCAST_ENTRY_PORT] =
        (uint8_t)(port << ALE_UCAST_ENTRY_PORT_SHIFT);
    CPSWALETableEntrySet(SOC_CPSW_ALE_REGS, index, ale_entry);
}

static void cpsw_ale_set_multicast(unsigned index, uint32_t port_mask, const uint8_t* mac)
{
    unsigned int ale_entry[ALE_ENTRY_WORDS] = { 0u, 0u, 0u };

    cpsw_ale_set_mac_bytes(ale_entry, mac);
    ((uint8_t*)ale_entry)[ALE_MCAST_ENTRY_TYPE] = ALE_ENTRY_MCAST;
    ((uint8_t*)ale_entry)[ALE_MCAST_ENTRY_PORTMASK] =
        (uint8_t)(port_mask << ALE_MCAST_PORTMASK_SHIFT);
    CPSWALETableEntrySet(SOC_CPSW_ALE_REGS, index, ale_entry);
}

static void cpsw_ale_configure_rx(const uint8_t* mac)
{
    static const uint8_t bcast[6] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    const uint32_t all_ports =
        CPSW_PORT_MASK_HOST | CPSW_PORT_MASK_SLAVE1 | CPSW_PORT_MASK_SLAVE2;

    CPSWALEBypassDisable(SOC_CPSW_ALE_REGS);
    /* Match StarterWare lwIP: device unicast entry uses ALE port 0. */
    cpsw_ale_set_unicast(BBB_CPSW_ALE_SELF_IDX, 0u, mac);
    cpsw_ale_set_multicast(BBB_CPSW_ALE_BCAST_IDX, all_ports, bcast);
    CPSWALEUnknownRegFloodMaskSet(SOC_CPSW_ALE_REGS, all_ports);
    CPSWALEUnknownUnRegFloodMaskSet(SOC_CPSW_ALE_REGS, all_ports);
    CPSWALEUnknownMemberListSet(SOC_CPSW_ALE_REGS, all_ports);
    *(volatile uint32_t*)(SOC_CPSW_ALE_REGS + CPSW_ALE_CONTROL) |=
        CPSW_ALE_CONTROL_EN_P0_UNI_FLOOD;
}

static void cpsw_requeue_rx(void)
{
    g_rx_desc->next         = 0;
    g_rx_desc->bufptr       = (uint32_t)g_rx_frame;
    g_rx_desc->bufoff_len   = sizeof(g_rx_frame);
    g_rx_desc->flags_pktlen = CPDMA_DESC_OWNER;

    CPSWCPDMARxHdrDescPtrWrite(SOC_CPSW_CPDMA_REGS, (uint32_t)g_rx_desc, 0u);
}

void bbb_cpsw_raw_note_peer(const uint8_t* mac)
{
    if (mac == 0 || mac_is_broadcast(mac) != 0) {
        return;
    }

    cpsw_ale_set_unicast(BBB_CPSW_ALE_PEER_IDX, BBB_CPSW_PHY_PORT, mac);
}

static void cpsw_zero_cppi_ram(void)
{
    volatile uint32_t* words =
        (volatile uint32_t*)SOC_CPSW_CPPI_RAM_REGS;
    unsigned           i;

    for (i = 0u; i < (BBB_CPPI_RAM_SIZE / 4u); i++) {
        words[i] = 0u;
    }
}

static void cpsw_init_dma(void)
{
    g_tx_desc = (volatile CpdmaDesc*)SOC_CPSW_CPPI_RAM_REGS;
    g_rx_desc =
        (volatile CpdmaDesc*)(SOC_CPSW_CPPI_RAM_REGS + (BBB_CPPI_RAM_SIZE / 2u));

    cpsw_zero_cppi_ram();

    g_tx_desc->next         = 0;
    g_tx_desc->bufptr       = 0;
    g_tx_desc->bufoff_len   = 0;
    g_tx_desc->flags_pktlen = 0;

    g_rx_desc->next         = 0;
    g_rx_desc->bufptr       = 0;
    g_rx_desc->bufoff_len   = 0;
    g_rx_desc->flags_pktlen = 0;

    CPSWCPDMAConfig(
        SOC_CPSW_CPDMA_REGS,
        CPDMA_CFG(
            0u,
            CPDMA_CFG_COPY_ERR_FRAMES,
            CPDMA_CFG_IDLE_COMMAND_NONE,
            CPDMA_CFG_NOT_BLOCK_RX_OFF_LEN_WRITE,
            CPDMA_CFG_RX_OWN_0,
            CPDMA_CFG_TX_PRI_FIXED));

    cpsw_requeue_rx();

    CPSWCPDMAEndOfIntVectorWrite(SOC_CPSW_CPDMA_REGS, CPSW_EOI_TX_PULSE);
    CPSWCPDMAEndOfIntVectorWrite(SOC_CPSW_CPDMA_REGS, CPSW_EOI_RX_PULSE);
    CPSWCPDMATxEnable(SOC_CPSW_CPDMA_REGS);
    CPSWCPDMARxEnable(SOC_CPSW_CPDMA_REGS);
}

static void cpsw_set_port_source_addresses(const uint8_t* mac)
{
    CPSWPortSrcAddrSet(SOC_CPSW_PORT_1_REGS, (unsigned char*)mac);
    CPSWPortSrcAddrSet(SOC_CPSW_PORT_2_REGS, (unsigned char*)mac);
}

static void cpsw_enable_sliver_mac(void)
{
    /* StarterWare lwIP CPSW port uses RGMII/MII enable on BBB, not GMII-only. */
    CPSWSlRGMIIEnable(SOC_CPSW_SLIVER_1_REGS);
}

static int cpsw_phy_read_bsr(unsigned short* bsr)
{
    if (bsr == 0) {
        return 0;
    }

    if (MDIOPhyRegRead(SOC_CPSW_MDIO_REGS, BBB_PHY_ADDR, PHY_BSR, bsr) != 0u) {
        return 1;
    }

    return 0;
}

static uint32_t cpsw_transfer_mode_for_partner(
    unsigned short adv,
    unsigned short partner)
{
    if ((adv & partner & PHY_100BTX_FD) != 0u) {
        return CPSW_SLIVER_NON_GIG_FULL_DUPLEX;
    }
    if ((adv & partner & PHY_100BTX) != 0u) {
        return CPSW_SLIVER_NON_GIG_HALF_DUPLEX;
    }
    if ((adv & partner & PHY_10BT_FD) != 0u) {
        return CPSW_SLIVER_INBAND | CPSW_SLIVER_NON_GIG_FULL_DUPLEX;
    }
    if ((adv & partner & PHY_10BT) != 0u) {
        return CPSW_SLIVER_INBAND | CPSW_SLIVER_NON_GIG_HALF_DUPLEX;
    }

    return CPSW_SLIVER_NON_GIG_FULL_DUPLEX;
}

static void cpsw_apply_sliver(unsigned short adv, unsigned short partner)
{
    CPSWSlTransferModeSet(
        SOC_CPSW_SLIVER_1_REGS,
        cpsw_transfer_mode_for_partner(adv, partner));
    CPSWSlRxMaxLenSet(SOC_CPSW_SLIVER_1_REGS, 1518u);
    cpsw_enable_sliver_mac();
}

static int cpsw_phy_link_confirmed(void)
{
    return (int)PhyLinkStatusGet(SOC_CPSW_MDIO_REGS, BBB_PHY_ADDR, 1000u);
}

void bbb_cpsw_raw_print_phy_bsr(void)
{
    unsigned short bsr = 0u;

    bbb_uart_puts(" phy_bsr=0x");
    if (cpsw_phy_read_bsr(&bsr) != 0) {
        bbb_uart_put_hex16(bsr);
    } else {
        bbb_uart_puts("read_fail");
    }
}

static int cpsw_autonegotiate_link(void)
{
    unsigned       timeout;
    unsigned short adv = (PHY_100BTX | PHY_100BTX_FD | PHY_10BT | PHY_10BT_FD);
    unsigned short gig_adv = 0u;
    unsigned short partner = 0u;
    unsigned short gig_partner = 0u;
    unsigned short bsr = 0u;

    if ((MDIOPhyAliveStatusGet(SOC_CPSW_MDIO_REGS) & (1u << BBB_PHY_ADDR)) == 0u) {
        return 0;
    }

    /*
     * After U-Boot, BMSR often already shows link+autoneg complete (e.g. 0x782D).
     * Do not restart autoneg; just match the CPSW sliver to the partner.
     */
    if (cpsw_phy_read_bsr(&bsr) != 0 &&
        (bsr & PHY_LINK_STATUS) != 0u &&
        (bsr & PHY_AUTONEG_STATUS) != 0u) {
        (void)PhyPartnerAbilityGet(
            SOC_CPSW_MDIO_REGS,
            BBB_PHY_ADDR,
            &partner,
            &gig_partner);
        cpsw_apply_sliver(adv, partner);
        return cpsw_phy_link_confirmed();
    }

    if (PhyAutoNegotiate(SOC_CPSW_MDIO_REGS, BBB_PHY_ADDR, &adv, &gig_adv) == 0u) {
        if (cpsw_phy_read_bsr(&bsr) != 0 && (bsr & PHY_LINK_STATUS) != 0u) {
            cpsw_apply_sliver(adv, partner);
            return 1;
        }

        return 0;
    }

    timeout = 500u;
    while (timeout-- != 0u) {
        bbb_delay(500000u);
        if (PhyAutoNegStatusGet(SOC_CPSW_MDIO_REGS, BBB_PHY_ADDR) != 0u) {
            break;
        }
    }

    if (timeout == 0u) {
        if (cpsw_phy_read_bsr(&bsr) != 0 && (bsr & PHY_LINK_STATUS) != 0u) {
            (void)PhyPartnerAbilityGet(
                SOC_CPSW_MDIO_REGS,
                BBB_PHY_ADDR,
                &partner,
                &gig_partner);
            cpsw_apply_sliver(adv, partner);
            return 1;
        }

        return 0;
    }

    (void)PhyPartnerAbilityGet(
        SOC_CPSW_MDIO_REGS,
        BBB_PHY_ADDR,
        &partner,
        &gig_partner);
    cpsw_apply_sliver(adv, partner);

    return cpsw_phy_link_confirmed();
}

int bbb_cpsw_raw_init(BbbCpswRaw* net)
{
    unsigned timeout;

    if (net == 0) {
        return -1;
    }

    memset(net, 0, sizeof(*net));
    g_rx_queue_head  = 0u;
    g_rx_queue_tail  = 0u;
    g_rx_queue_count = 0u;
    g_rx_queue_drops = 0u;

    CPSWClkEnable();
    CPSWPinMuxSetup();
    EVMPortMIIModeSelect();
    bbb_mac_read_wire_order(net->mac);

    CPSWSSReset(SOC_CPSW_SS_REGS);
    CPSWWrReset(SOC_CPSW_WR_REGS);
    CPSWSlReset(SOC_CPSW_SLIVER_1_REGS);
    CPSWSlReset(SOC_CPSW_SLIVER_2_REGS);
    CPSWCPDMAReset(SOC_CPSW_CPDMA_REGS);

    MDIOInit(SOC_CPSW_MDIO_REGS, MDIO_INPUT_HZ, MDIO_OUTPUT_HZ);
    bbb_delay(100000u);

    CPSWALEInit(SOC_CPSW_ALE_REGS);
    CPSWVLANAwareDisable(SOC_CPSW_ALE_REGS);
    CPSWALEVLANAwareClear(SOC_CPSW_ALE_REGS);
    CPSWALEPortStateSet(SOC_CPSW_ALE_REGS, 0u, CPSW_ALE_PORT_STATE_FWD);
    CPSWALEPortStateSet(SOC_CPSW_ALE_REGS, 1u, CPSW_ALE_PORT_STATE_FWD);
    CPSWALEPortStateSet(SOC_CPSW_ALE_REGS, 2u, CPSW_ALE_PORT_STATE_FWD);
    cpsw_ale_configure_rx(net->mac);
    cpsw_set_port_source_addresses(net->mac);

    timeout = 2000000u;
    while ((MDIOPhyAliveStatusGet(SOC_CPSW_MDIO_REGS) & (1u << BBB_PHY_ADDR)) == 0u &&
           timeout-- != 0u) {
        bbb_delay(10u);
    }

    net->link_up = timeout != 0u ? cpsw_autonegotiate_link() : 0;

    CPSWStatisticsEnable(SOC_CPSW_SS_REGS);
    cpsw_init_dma();

    return 0;
}

static int bbb_cpsw_wait_tx_done(BbbCpswRaw* net)
{
    volatile uint32_t spins = 2000000u;

    while ((g_tx_desc->flags_pktlen & CPDMA_DESC_OWNER) != 0u) {
        if (net != 0) {
            bbb_cpsw_raw_poll_rx(net);
        }
        if (spins-- == 0u) {
            return -1;
        }
        __asm__ volatile("nop");
    }

    CPSWCPDMATxCPWrite(SOC_CPSW_CPDMA_REGS, 0u, (uint32_t)g_tx_desc);
    CPSWCPDMAEndOfIntVectorWrite(SOC_CPSW_CPDMA_REGS, CPSW_EOI_TX_PULSE);
    return 0;
}

int bbb_cpsw_raw_send(
    BbbCpswRaw*    net,
    const uint8_t* dst_mac,
    const uint8_t* payload,
    size_t         payload_length)
{
    size_t i;
    size_t total;

    if (net == 0 || dst_mac == 0 || payload == 0 ||
        payload_length > BBB_CPSW_MAX_PAYLOAD ||
        payload_length + 14u > sizeof(g_tx_frame)) {
        return -1;
    }

    total = payload_length + 14u;
    if (total < 60u) {
        total = 60u;
    }

    memcpy(g_tx_frame, dst_mac, 6u);
    memcpy(g_tx_frame + 6u, net->mac, 6u);
    g_tx_frame[12] = (uint8_t)((LEAP_ETHERTYPE_DEVELOPMENT >> 8) & 0xFFu);
    g_tx_frame[13] = (uint8_t)(LEAP_ETHERTYPE_DEVELOPMENT & 0xFFu);
    memcpy(g_tx_frame + 14u, payload, payload_length);
    for (i = payload_length + 14u; i < total; i++) {
        g_tx_frame[i] = 0u;
    }

    bbb_dma_cache_clean(g_tx_frame, total);

    g_tx_desc->next         = 0;
    g_tx_desc->bufptr       = (uint32_t)g_tx_frame;
    g_tx_desc->bufoff_len   = (uint32_t)total & CPDMA_DESC_LEN_MASK;
    g_tx_desc->flags_pktlen = ((uint32_t)total & CPDMA_DESC_LEN_MASK) |
                              CPDMA_DESC_SOP |
                              CPDMA_DESC_EOP |
                              CPDMA_DESC_OWNER;

    __asm__ volatile("dsb" ::: "memory");

    CPSWCPDMATxHdrDescPtrWrite(SOC_CPSW_CPDMA_REGS, (uint32_t)g_tx_desc, 0u);

    if (bbb_cpsw_wait_tx_done(net) != 0) {
        return -1;
    }

    return 0;
}

static void bbb_rx_queue_drop_oldest(void)
{
    if (g_rx_queue_count == 0u) {
        return;
    }

    g_rx_queue_head = (g_rx_queue_head + 1u) % BBB_RX_QUEUE_SLOTS;
    g_rx_queue_count--;
    g_rx_queue_drops++;
}

void bbb_cpsw_raw_poll_rx(BbbCpswRaw* net)
{
    for (;;) {
        BbbRxQueueSlot* slot;
        size_t            rx_len = 0u;

        if (g_rx_queue_count >= BBB_RX_QUEUE_SLOTS) {
            bbb_rx_queue_drop_oldest();
        }

        slot = &g_rx_queue[g_rx_queue_tail];
        if (bbb_cpsw_raw_recv(
                net,
                slot->src_mac,
                slot->payload,
                sizeof(slot->payload),
                &rx_len) != 0) {
            break;
        }

        slot->length = (uint16_t)rx_len;
        g_rx_queue_tail = (g_rx_queue_tail + 1u) % BBB_RX_QUEUE_SLOTS;
        g_rx_queue_count++;
    }
}

uint32_t bbb_cpsw_raw_rx_queue_drops(void)
{
    return g_rx_queue_drops;
}

int bbb_cpsw_raw_dequeue(
    BbbCpswRaw* net,
    uint8_t*    src_mac,
    uint8_t*    payload,
    size_t      payload_capacity,
    size_t*     payload_length)
{
    BbbRxQueueSlot* slot;

    if (net == 0 || src_mac == 0 || payload == 0 || payload_length == 0) {
        return -1;
    }

    bbb_cpsw_raw_poll_rx(net);

    if (g_rx_queue_count == 0u) {
        return 1;
    }

    slot = &g_rx_queue[g_rx_queue_head];
    if ((size_t)slot->length > payload_capacity) {
        g_rx_queue_head = (g_rx_queue_head + 1u) % BBB_RX_QUEUE_SLOTS;
        g_rx_queue_count--;
        return -1;
    }

    memcpy(src_mac, slot->src_mac, 6u);
    memcpy(payload, slot->payload, slot->length);
    *payload_length = slot->length;

    g_rx_queue_head = (g_rx_queue_head + 1u) % BBB_RX_QUEUE_SLOTS;
    g_rx_queue_count--;
    return 0;
}

int bbb_cpsw_raw_recv(
    BbbCpswRaw* net,
    uint8_t*    src_mac,
    uint8_t*    payload,
    size_t      payload_capacity,
    size_t*     payload_length)
{
    uint32_t flags;
    size_t   frame_len;
    uint16_t ethertype;
    int      accepted;

    if (net == 0 || src_mac == 0 || payload == 0 || payload_length == 0) {
        return -1;
    }

    *payload_length = 0u;
    flags = g_rx_desc->flags_pktlen;

    if ((flags & CPDMA_DESC_OWNER) != 0u) {
        return 1;
    }

    frame_len = (size_t)(flags & CPDMA_DESC_LEN_MASK);
    accepted = 0;

    if (frame_len > sizeof(g_rx_frame)) {
        frame_len = sizeof(g_rx_frame);
    }

    if (frame_len != 0u) {
        bbb_dma_cache_invalidate(g_rx_frame, frame_len);
    }

    if (frame_len >= 14u) {
        ethertype = ((uint16_t)g_rx_frame[12] << 8) | (uint16_t)g_rx_frame[13];
        if (ethertype == LEAP_ETHERTYPE_DEVELOPMENT &&
            (mac_matches(g_rx_frame, net->mac) || mac_is_broadcast(g_rx_frame)) &&
            frame_len - 14u <= payload_capacity) {
            memcpy(src_mac, g_rx_frame + 6u, 6u);
            memcpy(payload, g_rx_frame + 14u, frame_len - 14u);
            *payload_length = frame_len - 14u;
            accepted = 1;
        }
    }

    CPSWCPDMARxCPWrite(SOC_CPSW_CPDMA_REGS, 0u, (uint32_t)g_rx_desc);
    CPSWCPDMAEndOfIntVectorWrite(SOC_CPSW_CPDMA_REGS, CPSW_EOI_RX_PULSE);

    /* Single-descriptor RX ring: always post a fresh buffer to the CPDMA. */
    cpsw_requeue_rx();

    return accepted != 0 ? 0 : 1;
}
