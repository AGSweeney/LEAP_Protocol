/* Revision: 3.5.7 */

/******************************************************************************
* Copyright 1998-2024 NetBurner, Inc.  ALL RIGHTS RESERVED
*
*    Permission is hereby granted to purchasers of NetBurner Hardware to use or
*    modify this computer program for any use as long as the resultant program
*    is only executed on NetBurner provided hardware.
*
*    No other rights to use this program or its derivatives in part or in
*    whole are granted.
*
*    It may be possible to license this or other NetBurner software for use on
*    non-NetBurner Hardware. Contact sales@Netburner.com for more information.
*
*    NetBurner makes no representation or warranties with respect to the
*    performance of this computer program, and specifically disclaims any
*    responsibility for any damages, special or consequential, connected with
*    the use of this program.
*
* NetBurner
* 16855 W Bernardo Dr
* San Diego, CA 92127
* www.netburner.com
******************************************************************************/

#include <init.h>
#include <nbrtos.h>
#include <iosys.h>
#include <ipshow.h>
#include <pins.h>
#include <netinterface.h>
#define ALLOW_CUSTOM_NET_DO_RX
#include <netrx.h>
#include <ethervars.h>
#include <bsp_devboard.h>
#include <hal.h>

#include <string.h>

#include "leap_config.h"
#include "leap_runtime.h"
#include "leap_wire.h"

extern "C" int leap_net_process_events(void);

const char *AppName = "LEAP NetBurner Scaffold";

#define LEAP_DEBUG_LOG 0
#define LEAP_ENABLE_PROMISC 0
#define LEAP_ENABLE_VLAN_AWARE 0
#define LEAP_ENABLE_BOARD_LED 1
#define LEAP_ENABLE_BOARD_DIP 0
#define LEAP_MIRROR_DO_TO_DI_WHEN_DIP_DISABLED 1
#define LEAP_PD_TRACE 0

#define LEAP_LOG_LEVEL_ERROR 0
#define LEAP_LOG_LEVEL_WARN 1
#define LEAP_LOG_LEVEL_INFO 2
#define LEAP_LOG_LEVEL_DEBUG 3

#ifndef LEAP_SERIAL_LOG_LEVEL
#define LEAP_SERIAL_LOG_LEVEL LEAP_LOG_LEVEL_ERROR
#endif

#define LEAP_LOG_AT(level, ...)                 \
    do                                          \
    {                                           \
        if (LEAP_SERIAL_LOG_LEVEL >= (level))   \
        {                                       \
            printf(__VA_ARGS__);                \
        }                                       \
    } while (0)

#define LEAP_LOG_ERROR(...) LEAP_LOG_AT(LEAP_LOG_LEVEL_ERROR, __VA_ARGS__)
#define LEAP_LOG_WARN(...) LEAP_LOG_AT(LEAP_LOG_LEVEL_WARN, __VA_ARGS__)
#define LEAP_LOG_INFO(...) LEAP_LOG_AT(LEAP_LOG_LEVEL_INFO, __VA_ARGS__)
#define LEAP_LOG_DEBUG(...)                     \
    do                                          \
    {                                           \
        if (LEAP_DEBUG_LOG)                     \
        {                                       \
            LEAP_LOG_AT(LEAP_LOG_LEVEL_DEBUG, __VA_ARGS__); \
        }                                       \
    } while (0)

#define LEAP_LOG(...) LEAP_LOG_DEBUG(__VA_ARGS__)

static bool leap_runtime_dispatch_frame(LeapRuntime *runtime, const LeapRxFrame *frame);
static int leap_custom_net_rx(PoolPtr pp, uint16_t ocount, int if_num);
static constexpr uint16_t LEAP_PD_CHANNEL_BYTES = 8U;

struct __attribute__((packed)) LeapProfileDigital16x16Wire
{
    uint16_t digital_inputs;
    uint16_t digital_outputs;
    uint16_t io_status;
    uint8_t v_field_supply;
    uint8_t reserved0;
};

static constexpr uint8_t LEAP_RX_QUEUE_DEPTH = 8U;
static LeapRxFrame g_rx_queue[LEAP_RX_QUEUE_DEPTH];
// Reply buffers live in BSS, not on the UserMain stack (~3 KB saved per frame).
static uint8_t g_reply_payload[LEAP_MAX_FRAME_BYTES];
static uint8_t g_reply_frame[LEAP_MAX_FRAME_BYTES];
static volatile uint8_t g_rx_head = 0U;
static volatile uint8_t g_rx_tail = 0U;
static volatile uint32_t g_rx_queue_drop_count = 0U;
static volatile uint32_t g_rx_match_count = 0U;
static volatile uint32_t g_rx_callback_count = 0U;
static volatile uint32_t g_rx_nonmatch_count = 0U;
static volatile uint32_t g_rx_vlan_seen_count = 0U;
static volatile uint32_t g_rx_nonmatch_ipv4 = 0U;
static volatile uint32_t g_rx_nonmatch_arp = 0U;
static volatile uint32_t g_rx_nonmatch_ipv6 = 0U;
static volatile uint16_t g_rx_last_nonmatch_etype = 0U;
static uint16_t g_rx_ethertype_filter = LEAP_ETHERTYPE_IN_USE;

static uint16_t g_device_state = LEAP_STATE_CONFIGURED;
static uint32_t g_active_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
static uint32_t g_default_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
static uint32_t g_next_session_id = 1U;
static uint32_t g_owner_session_id = 0U;
static uint8_t g_owner_mac[6] = {0};
static uint8_t g_owner_active = 0U;
static uint8_t g_do_shadow = 0U;
static uint8_t g_di_shadow = 0U;
static uint16_t g_io_status = LEAP_DIO_STATUS_OK;
static bool g_board_io_initialized = false;
static uint8_t g_header_for_crc[LEAP_HEADER_BYTES];

static uint32_t g_granted_lease_us = 0U;
static uint32_t g_granted_watchdog_us = 0U;
static uint64_t g_lease_deadline_us = 0U;
static uint64_t g_watchdog_deadline_us = 0U;
static uint64_t g_last_frame_rx_us = 0U;
static uint32_t g_last_reply_latency_us = 0U;
static uint32_t g_max_reply_latency_us = 0U;
static uint32_t g_last_cycle_time_us = 0U;
static uint32_t g_max_cycle_time_us = 0U;
static uint32_t g_min_cycle_time_us = 0xFFFFFFFFU;
static uint32_t g_pd_cycles_accepted = 0U;
static uint32_t g_rx_frames_rejected = 0U;
static uint32_t g_tx_frames_dropped = 0U;

static uint64_t leap_monotonic_us(void)
{
    uint32_t ticks1;
    uint32_t ticks2;
    uint16_t fraction;
    uint64_t us_per_tick;
    uint64_t fraction_us;

    do
    {
        ticks1 = (uint32_t)TimeTick;
        fraction = HalGetTickFraction();
        ticks2 = (uint32_t)TimeTick;
    } while (ticks1 != ticks2);

    us_per_tick = 1000000ULL / (uint64_t)TICKS_PER_SECOND;
    fraction_us = (uint64_t)fraction * us_per_tick;
    if (HalTickMaxCount > 0U)
    {
        fraction_us /= (uint64_t)HalTickMaxCount;
    }
    return (uint64_t)ticks1 * us_per_tick + fraction_us;
}

static uint32_t leap_remaining_us(uint64_t deadline_us, uint64_t now_us)
{
    if (deadline_us <= now_us)
    {
        return 0U;
    }

    const uint64_t remaining = deadline_us - now_us;
    return (remaining > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32_t)remaining;
}

static void leap_diag_note_request_rx(uint16_t service_id, uint64_t now_us)
{
    (void)service_id;
    g_last_frame_rx_us = now_us;
}

static void leap_diag_note_reply_tx(uint16_t service_id, uint64_t now_us)
{
    uint64_t latency_us;
    uint32_t latency_u32;

    if (g_last_frame_rx_us == 0U || now_us <= g_last_frame_rx_us)
    {
        return;
    }

    latency_us = now_us - g_last_frame_rx_us;
    latency_u32 = (latency_us > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32_t)latency_us;
    g_last_reply_latency_us = latency_u32;
    if (latency_u32 > g_max_reply_latency_us)
    {
        g_max_reply_latency_us = latency_u32;
    }

    if (service_id != LEAP_SERVICE_PD)
    {
        return;
    }

    g_pd_cycles_accepted++;
    g_last_cycle_time_us = latency_u32;
    if (latency_u32 > g_max_cycle_time_us)
    {
        g_max_cycle_time_us = latency_u32;
    }
    if (latency_u32 < g_min_cycle_time_us)
    {
        g_min_cycle_time_us = latency_u32;
    }

    if (g_owner_active != 0U && g_granted_watchdog_us > 0U)
    {
        g_watchdog_deadline_us = now_us + (uint64_t)g_granted_watchdog_us;
    }
}

static void leap_board_io_init()
{
    if (g_board_io_initialized)
    {
        return;
    }

#if LEAP_ENABLE_BOARD_LED
    LED1.function(PinGpioOutputFn);
    LED2.function(PinGpioOutputFn);
    LED3.function(PinGpioOutputFn);
    LED4.function(PinGpioOutputFn);
    LED5.function(PinGpioOutputFn);
    LED6.function(PinGpioOutputFn);
    LED7.function(PinGpioOutputFn);
    LED8.function(PinGpioOutputFn);
    LED1 = 1;
    LED2 = 1;
    LED3 = 1;
    LED4 = 1;
    LED5 = 1;
    LED6 = 1;
    LED7 = 1;
    LED8 = 1;
#endif

    g_board_io_initialized = true;
}

static void leap_board_apply_outputs(uint8_t do_bits)
{
#if !LEAP_ENABLE_BOARD_LED
    (void)do_bits;
    return;
#else
    LED1 = (do_bits & 0x01U) ? 0 : 1;
    LED2 = (do_bits & 0x02U) ? 0 : 1;
    LED3 = (do_bits & 0x04U) ? 0 : 1;
    LED4 = (do_bits & 0x08U) ? 0 : 1;
    LED5 = (do_bits & 0x10U) ? 0 : 1;
    LED6 = (do_bits & 0x20U) ? 0 : 1;
    LED7 = (do_bits & 0x40U) ? 0 : 1;
    LED8 = (do_bits & 0x80U) ? 0 : 1;
#if LEAP_PD_TRACE
    static uint8_t s_last_do = 0xFFU;
    if (s_last_do != do_bits)
    {
        s_last_do = do_bits;
        LEAP_LOG_DEBUG("LEAP DO -> LED 0x%02X\r\n", do_bits);
    }
#endif
#endif
}

static uint8_t leap_decode_do_bits(const uint8_t *data, uint16_t len)
{
    if (data == 0 || len == 0U)
    {
        return 0U;
    }

    // Legacy/simple profile: bitfield in first byte.
    if (len == 1U)
    {
        return data[0];
    }

    // Standard LEAP digital profile payload (8-byte LeapProfileDigital16x16).
    if (len >= sizeof(LeapProfileDigital16x16Wire))
    {
        const uint16_t outputs = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
        return (uint8_t)(outputs & 0x00FFU);
    }

    return data[0];
}

static uint16_t leap_read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t leap_read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void leap_write_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void leap_write_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void leap_write_le64(uint8_t *p, uint64_t v)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        p[i] = (uint8_t)((v >> (8U * i)) & 0xFFU);
    }
}

static void leap_pack_digital_profile(uint8_t *out,
                                      uint16_t len,
                                      uint16_t digital_inputs,
                                      uint16_t digital_outputs)
{
    if (out == 0 || len == 0U)
    {
        return;
    }

    if (len == 1U)
    {
        out[0] = (uint8_t)(digital_inputs & 0xFFU);
        return;
    }

    if (len >= sizeof(LeapProfileDigital16x16Wire))
    {
        memset(out, 0, len);
        leap_write_le16(out + 0, digital_inputs);
        leap_write_le16(out + 2, digital_outputs);
        leap_write_le16(out + 4, g_io_status);
        for (uint16_t i = (uint16_t)sizeof(LeapProfileDigital16x16Wire); i < len; i++)
        {
            out[i] = 0U;
        }
        return;
    }

    out[0] = (uint8_t)(digital_inputs & 0xFFU);
}

static void leap_encode_di_bytes(uint8_t di_bits, uint8_t *out, uint16_t len)
{
    leap_pack_digital_profile(out, len, di_bits, g_do_shadow);
}

static uint16_t leap_crc16_xmodem(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0x0000U;
    for (uint16_t i = 0U; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint32_t leap_crc32c(const uint8_t *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint16_t i = 0U; i < length; i++)
    {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            const uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1) ^ (0x82F63B78U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

static bool leap_frame_parse_header(const uint8_t *data,
                                    uint16_t length,
                                    LeapHeaderWire *header_out,
                                    const uint8_t **payload_out)
{
    uint16_t computed_header_crc;
    uint32_t computed_payload_crc;

    if (data == 0 || header_out == 0 || payload_out == 0 || length < LEAP_HEADER_BYTES)
    {
        return false;
    }

    if (data[0] != 0x4CU || data[1] != 0x45U || data[2] != 0x41U || data[3] != 0x50U)
    {
        return false;
    }

    header_out->magic = LEAP_MAGIC_U32;
    header_out->version_major = data[4];
    header_out->version_minor = data[5];
    header_out->header_length = data[6];
    header_out->flags = data[7];
    header_out->service_id = leap_read_le16(&data[8]);
    header_out->message_type = leap_read_le16(&data[10]);
    header_out->session_id = leap_read_le32(&data[12]);
    header_out->sequence = leap_read_le32(&data[16]);
    header_out->ack_sequence = leap_read_le32(&data[20]);
    header_out->payload_length = leap_read_le16(&data[24]);
    header_out->header_crc16 = leap_read_le16(&data[26]);
    header_out->payload_crc32c = leap_read_le32(&data[28]);

    if (header_out->version_major != LEAP_VERSION_MAJOR ||
        header_out->header_length != LEAP_HEADER_BYTES ||
        header_out->payload_length > LEAP_MAX_PAYLOAD_BYTES)
    {
        return false;
    }

    if ((uint16_t)(header_out->header_length + header_out->payload_length) > length)
    {
        return false;
    }

    memcpy(g_header_for_crc, data, LEAP_HEADER_BYTES);
    g_header_for_crc[LEAP_HEADER_CRC_OFFSET] = 0U;
    g_header_for_crc[LEAP_HEADER_CRC_OFFSET + 1U] = 0U;
    computed_header_crc = leap_crc16_xmodem(g_header_for_crc, LEAP_HEADER_BYTES);
    if (computed_header_crc != header_out->header_crc16)
    {
        return false;
    }

    *payload_out = data + header_out->header_length;
    if (header_out->payload_length == 0U)
    {
        return true;
    }

    if ((header_out->flags & LEAP_FLAG_NO_PAYLOAD_CRC) != 0U)
    {
        return true;
    }

    computed_payload_crc = leap_crc32c(*payload_out, header_out->payload_length);
    if (computed_payload_crc != header_out->payload_crc32c)
    {
        return false;
    }

    return true;
}

static bool leap_frame_build(uint8_t *out,
                             uint16_t out_capacity,
                             uint16_t *out_length,
                             uint8_t flags,
                             uint16_t service_id,
                             uint16_t message_type,
                             uint32_t session_id,
                             uint32_t sequence,
                             uint32_t ack_sequence,
                             const uint8_t *payload,
                             uint16_t payload_length)
{
    uint16_t total_length;
    uint16_t header_crc;

    if (out == 0 || out_length == 0)
    {
        return false;
    }

    if (payload_length > LEAP_MAX_PAYLOAD_BYTES)
    {
        return false;
    }

    if (payload_length > 0U && payload == 0)
    {
        return false;
    }

    total_length = (uint16_t)(LEAP_HEADER_BYTES + payload_length);
    if (total_length > out_capacity)
    {
        return false;
    }

    memset(out, 0, total_length);
    out[0] = 0x4CU;
    out[1] = 0x45U;
    out[2] = 0x41U;
    out[3] = 0x50U;
    out[4] = LEAP_VERSION_MAJOR;
    out[5] = LEAP_VERSION_MINOR;
    out[6] = LEAP_HEADER_BYTES;
    out[7] = flags;
    leap_write_le16(&out[8], service_id);
    leap_write_le16(&out[10], message_type);
    leap_write_le32(&out[12], session_id);
    leap_write_le32(&out[16], sequence);
    leap_write_le32(&out[20], ack_sequence);
    leap_write_le16(&out[24], payload_length);

    if (payload_length > 0U)
    {
        memcpy(out + LEAP_HEADER_BYTES, payload, payload_length);
        leap_write_le32(&out[28], leap_crc32c(out + LEAP_HEADER_BYTES, payload_length));
    }

    memcpy(g_header_for_crc, out, LEAP_HEADER_BYTES);
    g_header_for_crc[LEAP_HEADER_CRC_OFFSET] = 0U;
    g_header_for_crc[LEAP_HEADER_CRC_OFFSET + 1U] = 0U;
    header_crc = leap_crc16_xmodem(g_header_for_crc, LEAP_HEADER_BYTES);
    leap_write_le16(&out[LEAP_HEADER_CRC_OFFSET], header_crc);

    *out_length = total_length;
    return true;
}

static size_t leap_tlv_total_length(uint16_t value_length)
{
    const size_t padded = (size_t)((value_length + 3U) & ~3U);
    return sizeof(LeapTlvHeaderWire) + padded;
}

static size_t leap_append_tlv(uint8_t *out,
                              size_t out_capacity,
                              size_t offset,
                              uint16_t type,
                              const uint8_t *value,
                              uint16_t value_length)
{
    const size_t total = leap_tlv_total_length(value_length);
    LeapTlvHeaderWire *tlv;
    size_t padded;
    if (out == 0 || (offset + total) > out_capacity)
    {
        return 0U;
    }

    tlv = (LeapTlvHeaderWire *)(out + offset);
    leap_write_le16((uint8_t *)&tlv->type, type);
    leap_write_le16((uint8_t *)&tlv->length, value_length);

    if (value_length > 0U && value != 0)
    {
        memcpy(out + offset + sizeof(LeapTlvHeaderWire), value, value_length);
    }

    padded = (size_t)((value_length + 3U) & ~3U);
    if (padded > value_length)
    {
        memset(out + offset + sizeof(LeapTlvHeaderWire) + value_length,
               0,
               padded - value_length);
    }
    return total;
}

static uint16_t leap_build_mgmt_payload(const LeapHeaderWire *request_header,
                                        const uint8_t *request_payload,
                                        uint16_t request_payload_len,
                                        uint16_t *reply_message_type_out,
                                        uint8_t *out_payload,
                                        uint16_t out_capacity)
{
    if (request_header == 0 || reply_message_type_out == 0 || out_payload == 0)
    {
        return 0U;
    }

    if (request_header->message_type == LEAP_MGMT_OPEN_SESSION &&
        request_payload_len >= sizeof(LeapOpenSessionRequestWire))
    {
        const LeapOpenSessionRequestWire *req = (const LeapOpenSessionRequestWire *)request_payload;
        LeapOpenSessionReplyWire reply;
        uint32_t lease_us = leap_read_le32((const uint8_t *)&req->requested_lease_time_us);
        uint32_t watchdog_us = leap_read_le32((const uint8_t *)&req->requested_watchdog_time_us);
        uint16_t open_flags = leap_read_le16((const uint8_t *)&req->open_flags);

        if (lease_us == 0U) { lease_us = 5000000U; }
        if (watchdog_us == 0U) { watchdog_us = 5000000U; }

        memset(&reply, 0, sizeof(reply));
        g_owner_session_id = g_next_session_id++;
        memcpy(g_owner_mac, req->controller_mac, 6);
        g_owner_active = ((open_flags & LEAP_OPEN_FLAG_REQUEST_OWNER) != 0U) ? 1U : 0U;
        if (g_owner_active != 0U)
        {
            g_device_state = LEAP_STATE_SAFE;
        }

        leap_write_le32((uint8_t *)&reply.assigned_session_id, g_owner_session_id);
        leap_write_le32((uint8_t *)&reply.granted_lease_time_us, lease_us);
        leap_write_le32((uint8_t *)&reply.granted_watchdog_time_us, watchdog_us);
        leap_write_le16((uint8_t *)&reply.session_flags,
                        (uint16_t)(LEAP_SESSION_FLAG_OWNER | LEAP_SESSION_FLAG_LEASE_ACTIVE));
        leap_write_le16((uint8_t *)&reply.current_state, g_device_state);
        memcpy(reply.owner_mac, g_owner_mac, 6);

        g_granted_lease_us = lease_us;
        g_granted_watchdog_us = watchdog_us;
        {
            const uint64_t now_us = leap_monotonic_us();
            g_lease_deadline_us = now_us + (uint64_t)lease_us;
            g_watchdog_deadline_us = now_us + (uint64_t)watchdog_us;
        }

        if (out_capacity < sizeof(reply))
        {
            return 0U;
        }
        memcpy(out_payload, &reply, sizeof(reply));
        *reply_message_type_out = LEAP_MGMT_OPEN_SESSION_REPLY;
        return (uint16_t)sizeof(reply);
    }

    if (request_header->message_type == LEAP_MGMT_SET_STATE &&
        request_payload_len >= sizeof(LeapSetStateRequestWire))
    {
        const LeapSetStateRequestWire *req = (const LeapSetStateRequestWire *)request_payload;
        LeapStateReplyWire reply;
        const uint16_t requested_state = leap_read_le16((const uint8_t *)&req->requested_state);

        if (g_owner_active != 0U && request_header->session_id == g_owner_session_id)
        {
            if (requested_state == LEAP_STATE_SAFE || requested_state == LEAP_STATE_OP)
            {
                g_device_state = requested_state;
                if (requested_state == LEAP_STATE_SAFE)
                {
                    g_do_shadow = 0U;
#if !LEAP_ENABLE_BOARD_DIP && LEAP_MIRROR_DO_TO_DI_WHEN_DIP_DISABLED
                    g_di_shadow = 0U;
#endif
                    leap_board_apply_outputs(g_do_shadow);
                }
            }
        }

        memset(&reply, 0, sizeof(reply));
        leap_write_le16((uint8_t *)&reply.accepted_state, g_device_state);
        leap_write_le16((uint8_t *)&reply.current_state, g_device_state);
        leap_write_le32((uint8_t *)&reply.state_detail, 0U);
        if (out_capacity < sizeof(reply))
        {
            return 0U;
        }

        memcpy(out_payload, &reply, sizeof(reply));
        *reply_message_type_out = LEAP_MGMT_STATE_REPLY;
        return (uint16_t)sizeof(reply);
    }

    if (request_header->message_type == LEAP_MGMT_CLOSE_SESSION ||
        request_header->message_type == LEAP_MGMT_OWNER_RELEASE)
    {
        if (request_header->session_id == g_owner_session_id)
        {
            g_owner_active = 0U;
            g_owner_session_id = 0U;
            memset(g_owner_mac, 0, sizeof(g_owner_mac));
            g_granted_lease_us = 0U;
            g_granted_watchdog_us = 0U;
            g_lease_deadline_us = 0U;
            g_watchdog_deadline_us = 0U;
            g_device_state = LEAP_STATE_SAFE;
            g_do_shadow = 0U;
#if !LEAP_ENABLE_BOARD_DIP && LEAP_MIRROR_DO_TO_DI_WHEN_DIP_DISABLED
            g_di_shadow = 0U;
#endif
            leap_board_apply_outputs(g_do_shadow);
        }
        return 0U;
    }

    if (request_header->message_type == LEAP_MGMT_HEARTBEAT)
    {
        if (g_owner_active != 0U && request_header->session_id == g_owner_session_id &&
            g_granted_lease_us > 0U)
        {
            const uint64_t now_us = leap_monotonic_us();
            g_lease_deadline_us = now_us + (uint64_t)g_granted_lease_us;
        }
        return 0U;
    }

    return 0U;
}

static uint16_t leap_build_dir_payload(const LeapHeaderWire *request_header,
                                       const uint8_t *request_payload,
                                       uint16_t request_payload_len,
                                       uint16_t *reply_message_type_out,
                                       uint8_t *out_payload,
                                       uint16_t out_capacity,
                                       int32_t interface_number)
{
    MACADR source_mac;
    LeapIdentityWire identity;
    LeapProfileDescriptorWire profile;
    LeapEndpointDescriptorWire ep_out;
    LeapEndpointDescriptorWire ep_in;
    size_t tlv_offset;
    size_t chunk;

    if (request_header == 0 || reply_message_type_out == 0 || out_payload == 0)
    {
        return 0U;
    }

    source_mac = InterfaceMAC(interface_number > 0 ? interface_number : GetFirstInterface());
    memset(&identity, 0, sizeof(identity));
    for (uint8_t i = 0; i < 6U; i++)
    {
        identity.primary_mac[i] = source_mac.GetByte(i);
    }

    memset(&profile, 0, sizeof(profile));
    leap_write_le32((uint8_t *)&profile.profile_id, g_active_profile_id);
    leap_write_le16((uint8_t *)&profile.profile_revision, 1U);
    leap_write_le16((uint8_t *)&profile.endpoint_count, 2U);

    memset(&ep_out, 0, sizeof(ep_out));
    leap_write_le16((uint8_t *)&ep_out.endpoint_id, LEAP_ENDPOINT_DIGITAL_OUTPUTS);
    ep_out.direction = (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE;
    leap_write_le32((uint8_t *)&ep_out.profile_id, g_active_profile_id);
    leap_write_le16((uint8_t *)&ep_out.byte_length, 1U);
    ep_out.alignment = 1U;
    ep_out.flags = 0x01U;
    leap_write_le32((uint8_t *)&ep_out.schema_object_id, LEAP_DIR_PROFILE_OBJECT_ID);

    memset(&ep_in, 0, sizeof(ep_in));
    leap_write_le16((uint8_t *)&ep_in.endpoint_id, LEAP_ENDPOINT_DIGITAL_INPUTS);
    ep_in.direction = (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER;
    leap_write_le32((uint8_t *)&ep_in.profile_id, g_active_profile_id);
    leap_write_le16((uint8_t *)&ep_in.byte_length, 1U);
    ep_in.alignment = 1U;
    ep_in.flags = (uint8_t)(0x01U | 0x08U);
    leap_write_le32((uint8_t *)&ep_in.schema_object_id, LEAP_DIR_PROFILE_OBJECT_ID);

    if (request_header->message_type == LEAP_DIR_READ_DIRECTORY)
    {
        LeapReadDirectoryReplyWire hdr;
        uint8_t default_profile_le[4];
        uint8_t active_profile_le[4];
        uint8_t locate_flags_le[2];
        static uint8_t tlv_buf[384];
        size_t tlv_len = 0U;

        memset(tlv_buf, 0, sizeof(tlv_buf));
        leap_write_le32(default_profile_le, g_default_profile_id);
        leap_write_le32(active_profile_le, g_active_profile_id);
        leap_write_le16(locate_flags_le, LEAP_LOCATE_FLAG_LED);
        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_DEVICE_IDENTITY,
                                (const uint8_t *)&identity, (uint16_t)sizeof(identity));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_DEFAULT_PROFILE_ID,
                                default_profile_le, (uint16_t)sizeof(default_profile_le));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_ACTIVE_PROFILE_ID,
                                active_profile_le, (uint16_t)sizeof(active_profile_le));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_PROFILE_DESCRIPTOR,
                                (const uint8_t *)&profile, (uint16_t)sizeof(profile));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_ENDPOINT_DESCRIPTOR,
                                (const uint8_t *)&ep_out, (uint16_t)sizeof(ep_out));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_ENDPOINT_DESCRIPTOR,
                                (const uint8_t *)&ep_in, (uint16_t)sizeof(ep_in));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        chunk = leap_append_tlv(tlv_buf, sizeof(tlv_buf), tlv_len, LEAP_TLV_LOCATE_CAPABILITY,
                                locate_flags_le, (uint16_t)sizeof(locate_flags_le));
        if (chunk == 0U) { return 0U; }
        tlv_len += chunk;

        if (out_capacity < (uint16_t)(sizeof(hdr) + tlv_len))
        {
            return 0U;
        }

        memset(&hdr, 0, sizeof(hdr));
        leap_write_le16((uint8_t *)&hdr.total_bytes, (uint16_t)tlv_len);
        leap_write_le16((uint8_t *)&hdr.returned_bytes, (uint16_t)tlv_len);
        memcpy(out_payload, &hdr, sizeof(hdr));
        memcpy(out_payload + sizeof(hdr), tlv_buf, tlv_len);
        *reply_message_type_out = LEAP_DIR_READ_DIRECTORY_REPLY;
        return (uint16_t)(sizeof(hdr) + tlv_len);
    }

    if (request_header->message_type == LEAP_DIR_READ_OBJECT &&
        request_payload_len >= sizeof(LeapReadObjectRequestWire))
    {
        const LeapReadObjectRequestWire *req = (const LeapReadObjectRequestWire *)request_payload;
        const uint32_t object_id = leap_read_le32((const uint8_t *)&req->object_id);
        uint8_t object_bytes[128];
        uint16_t object_len = 0U;
        LeapReadObjectReplyWire reply;

        if (object_id == LEAP_DIR_IDENTITY_OBJECT_ID)
        {
            memcpy(object_bytes, &identity, sizeof(identity));
            object_len = (uint16_t)sizeof(identity);
        }
        else if (object_id == LEAP_DIR_PROFILE_OBJECT_ID)
        {
            memcpy(object_bytes, &profile, sizeof(profile));
            memcpy(object_bytes + sizeof(profile), &ep_out, sizeof(ep_out));
            memcpy(object_bytes + sizeof(profile) + sizeof(ep_out), &ep_in, sizeof(ep_in));
            object_len = (uint16_t)(sizeof(profile) + sizeof(ep_out) + sizeof(ep_in));
        }
        else
        {
            return 0U;
        }

        if (out_capacity < (uint16_t)(sizeof(reply) + object_len))
        {
            return 0U;
        }

        memset(&reply, 0, sizeof(reply));
        leap_write_le32((uint8_t *)&reply.object_id, object_id);
        leap_write_le32((uint8_t *)&reply.length, object_len);
        memcpy(out_payload, &reply, sizeof(reply));
        memcpy(out_payload + sizeof(reply), object_bytes, object_len);
        *reply_message_type_out = LEAP_DIR_READ_OBJECT_REPLY;
        return (uint16_t)(sizeof(reply) + object_len);
    }

    if (request_header->message_type == LEAP_DIR_SELECT_PROFILE &&
        request_payload_len >= sizeof(LeapSelectProfileRequestWire))
    {
        const LeapSelectProfileRequestWire *req = (const LeapSelectProfileRequestWire *)request_payload;
        const uint32_t requested = leap_read_le32((const uint8_t *)&req->requested_profile_id);
        LeapProfileReplyWire reply;
        if (requested == LEAP_PROFILE_DIGITAL_IO_8X8)
        {
            g_active_profile_id = requested;
            if (g_device_state == LEAP_STATE_CONFIGURED || g_device_state == LEAP_STATE_SAFE)
            {
                g_device_state = LEAP_STATE_CONFIGURED;
            }
        }

        if (out_capacity < sizeof(reply))
        {
            return 0U;
        }

        memset(&reply, 0, sizeof(reply));
        leap_write_le32((uint8_t *)&reply.active_profile_id, g_active_profile_id);
        leap_write_le16((uint8_t *)&reply.endpoint_count, 2U);
        memcpy(out_payload, &reply, sizeof(reply));
        *reply_message_type_out = LEAP_DIR_PROFILE_REPLY;
        return (uint16_t)sizeof(reply);
    }

    (void)tlv_offset;
    return 0U;
}

static uint16_t leap_build_pd_payload(const LeapHeaderWire *request_header,
                                      const uint8_t *request_payload,
                                      uint16_t request_payload_len,
                                      const uint8_t *src_mac,
                                      uint16_t *reply_message_type_out,
                                      uint8_t *out_payload,
                                      uint16_t out_capacity)
{
    if (request_header == 0 || reply_message_type_out == 0 || out_payload == 0)
    {
        return 0U;
    }

    if (g_owner_active == 0U || request_header->session_id != g_owner_session_id)
    {
        return 0U;
    }

    if (src_mac != 0 && memcmp(src_mac, g_owner_mac, 6) != 0)
    {
        return 0U;
    }

#if !LEAP_ENABLE_BOARD_DIP && LEAP_MIRROR_DO_TO_DI_WHEN_DIP_DISABLED
    g_di_shadow = g_do_shadow;
#endif

    if (request_header->message_type == LEAP_PD_WRITE_ENDPOINT &&
        request_payload_len >= sizeof(LeapEndpointDataHeaderWire))
    {
        const LeapEndpointDataHeaderWire *hdr = (const LeapEndpointDataHeaderWire *)request_payload;
        const uint16_t endpoint_id = leap_read_le16((const uint8_t *)&hdr->endpoint_id);
        const uint16_t data_len = leap_read_le16((const uint8_t *)&hdr->data_length);
        const uint8_t *data = request_payload + sizeof(LeapEndpointDataHeaderWire);
        LeapEndpointDataHeaderWire reply_hdr;
        uint16_t reply_len = (data_len >= LEAP_PD_CHANNEL_BYTES) ? LEAP_PD_CHANNEL_BYTES : 1U;

        if (data_len >= 1U &&
            request_payload_len >= (uint16_t)(sizeof(LeapEndpointDataHeaderWire) + data_len))
        {
            g_do_shadow = leap_decode_do_bits(data, data_len);
#if !LEAP_ENABLE_BOARD_DIP && LEAP_MIRROR_DO_TO_DI_WHEN_DIP_DISABLED
            g_di_shadow = g_do_shadow;
#endif
            leap_board_apply_outputs(g_do_shadow);
#if LEAP_PD_TRACE
            LEAP_LOG_DEBUG("LEAP PD WRITE ep=0x%04X len=%u do=0x%02X\r\n",
                           endpoint_id,
                           data_len,
                           g_do_shadow);
#endif
        (void)endpoint_id;
        }

        if (out_capacity < (uint16_t)(sizeof(reply_hdr) + reply_len))
        {
            return 0U;
        }
        memcpy(&reply_hdr, hdr, sizeof(reply_hdr));
        leap_write_le16((uint8_t *)&reply_hdr.endpoint_id, LEAP_ENDPOINT_DIGITAL_INPUTS);
        leap_write_le16((uint8_t *)&reply_hdr.data_length, reply_len);
        memcpy(out_payload, &reply_hdr, sizeof(reply_hdr));
        leap_encode_di_bytes(g_di_shadow, out_payload + sizeof(reply_hdr), reply_len);
        *reply_message_type_out = LEAP_PD_ENDPOINT_DATA;
        return (uint16_t)(sizeof(reply_hdr) + reply_len);
    }

    if (request_header->message_type == LEAP_PD_READ_ENDPOINT &&
        request_payload_len >= sizeof(LeapEndpointDataHeaderWire))
    {
        const LeapEndpointDataHeaderWire *hdr = (const LeapEndpointDataHeaderWire *)request_payload;
        uint16_t endpoint_id = leap_read_le16((const uint8_t *)&hdr->endpoint_id);
        uint16_t requested_len = leap_read_le16((const uint8_t *)&hdr->data_length);
        LeapEndpointDataHeaderWire reply_hdr;
        uint8_t value = (endpoint_id == LEAP_ENDPOINT_DIGITAL_OUTPUTS) ? g_do_shadow : g_di_shadow;
        uint16_t reply_len = (requested_len == 0U) ? 1U : requested_len;

        if (out_capacity < (uint16_t)(sizeof(reply_hdr) + reply_len))
        {
            return 0U;
        }
        memcpy(&reply_hdr, hdr, sizeof(reply_hdr));
        leap_write_le16((uint8_t *)&reply_hdr.data_length, reply_len);
        memcpy(out_payload, &reply_hdr, sizeof(reply_hdr));
        leap_encode_di_bytes(value, out_payload + sizeof(reply_hdr), reply_len);
        *reply_message_type_out = LEAP_PD_ENDPOINT_DATA;
        return (uint16_t)(sizeof(reply_hdr) + reply_len);
    }

    if (request_header->message_type == LEAP_PD_EXCHANGE_ENDPOINTS &&
        request_payload_len >= sizeof(LeapExchangeHeaderWire))
    {
        const LeapExchangeHeaderWire *hdr = (const LeapExchangeHeaderWire *)request_payload;
        const uint16_t write_len = leap_read_le16((const uint8_t *)&hdr->write_length);
        const uint16_t read_len = leap_read_le16((const uint8_t *)&hdr->read_length);
        const uint8_t *write_data = request_payload + sizeof(LeapExchangeHeaderWire);
        const uint16_t status_len = (uint16_t)sizeof(LeapExchangeStatusWire);
        const uint16_t total = (uint16_t)(sizeof(LeapExchangeHeaderWire) + write_len + read_len + status_len);
        const uint32_t process_seq = leap_read_le32((const uint8_t *)&hdr->process_sequence);
        uint8_t *status_bytes;

        if (request_payload_len < (uint16_t)(sizeof(LeapExchangeHeaderWire) + write_len) ||
            out_capacity < total)
        {
            return 0U;
        }

        if (write_len > 0U)
        {
            g_do_shadow = leap_decode_do_bits(write_data, write_len);
#if !LEAP_ENABLE_BOARD_DIP && LEAP_MIRROR_DO_TO_DI_WHEN_DIP_DISABLED
            g_di_shadow = g_do_shadow;
#endif
            leap_board_apply_outputs(g_do_shadow);
        }
#if LEAP_PD_TRACE
        const uint16_t write_endpoint_id = leap_read_le16((const uint8_t *)&hdr->write_endpoint_id);
        const uint16_t read_endpoint_id = leap_read_le16((const uint8_t *)&hdr->read_endpoint_id);
        LEAP_LOG_DEBUG("LEAP PD EXCH w_ep=0x%04X r_ep=0x%04X w_len=%u r_len=%u do=0x%02X di=0x%02X\r\n",
                       write_endpoint_id,
                       read_endpoint_id,
                       write_len,
                       read_len,
                       g_do_shadow,
                       g_di_shadow);
#endif

        memcpy(out_payload, hdr, sizeof(LeapExchangeHeaderWire));
        if (write_len > 0U)
        {
            memcpy(out_payload + sizeof(LeapExchangeHeaderWire), write_data, write_len);
        }
        leap_pack_digital_profile(out_payload + sizeof(LeapExchangeHeaderWire) + write_len,
                                  read_len,
                                  g_di_shadow,
                                  g_do_shadow);

        status_bytes = out_payload + sizeof(LeapExchangeHeaderWire) + write_len + read_len;
        memset(status_bytes, 0, status_len);
        leap_write_le32(status_bytes + 0, process_seq);
        leap_write_le32(status_bytes + 4, process_seq);
        leap_write_le16(status_bytes + 20, LEAP_STATUS_OK);

        *reply_message_type_out = LEAP_PD_EXCHANGE_REPLY;
        return total;
    }

    return 0U;
}

static uint16_t leap_build_diag_payload(const LeapHeaderWire *request_header,
                                        const uint8_t *request_payload,
                                        uint16_t request_payload_len,
                                        const LeapRuntime *runtime,
                                        uint16_t *reply_message_type_out,
                                        uint8_t *out_payload,
                                        uint16_t out_capacity)
{
    (void)request_payload;
    (void)request_payload_len;
    if (request_header == 0 || reply_message_type_out == 0 || out_payload == 0 || runtime == 0)
    {
        return 0U;
    }

    if (request_header->message_type == LEAP_DIAG_READ_COUNTERS)
    {
        LeapCountersReplyWire reply;
        LeapCounterEntryWire entries[6];
        if (out_capacity < (uint16_t)(sizeof(reply) + sizeof(entries)))
        {
            return 0U;
        }
        memset(&reply, 0, sizeof(reply));
        leap_write_le16((uint8_t *)&reply.counter_count, 6U);

        memset(entries, 0, sizeof(entries));
        leap_write_le16((uint8_t *)&entries[0].counter_id, LEAP_COUNTER_RX_FRAMES_ACCEPTED);
        leap_write_le64((uint8_t *)&entries[0].value, (uint64_t)runtime->rx_frames);
        leap_write_le16((uint8_t *)&entries[1].counter_id, LEAP_COUNTER_RX_FRAMES_REJECTED);
        leap_write_le64((uint8_t *)&entries[1].value, (uint64_t)g_rx_frames_rejected);
        leap_write_le16((uint8_t *)&entries[2].counter_id, LEAP_COUNTER_TX_FRAMES_ACCEPTED);
        leap_write_le64((uint8_t *)&entries[2].value, (uint64_t)runtime->tx_frames);
        leap_write_le16((uint8_t *)&entries[3].counter_id, LEAP_COUNTER_TX_FRAMES_DROPPED);
        leap_write_le64((uint8_t *)&entries[3].value,
                        (uint64_t)(g_tx_frames_dropped + g_rx_queue_drop_count));
        leap_write_le16((uint8_t *)&entries[4].counter_id, LEAP_COUNTER_PROCESS_CYCLES_ACCEPTED);
        leap_write_le64((uint8_t *)&entries[4].value, (uint64_t)g_pd_cycles_accepted);
        leap_write_le16((uint8_t *)&entries[5].counter_id, LEAP_COUNTER_MAX_REPLY_LATENCY_US);
        leap_write_le64((uint8_t *)&entries[5].value, (uint64_t)g_max_reply_latency_us);

        memcpy(out_payload, &reply, sizeof(reply));
        memcpy(out_payload + sizeof(reply), entries, sizeof(entries));
        *reply_message_type_out = LEAP_DIAG_COUNTERS_REPLY;
        return (uint16_t)(sizeof(reply) + sizeof(entries));
    }

    if (request_header->message_type == LEAP_DIAG_READ_TIMING)
    {
        LeapTimingReplyWire reply;
        const uint64_t now_us = leap_monotonic_us();
        const uint32_t min_cycle =
            (g_min_cycle_time_us == 0xFFFFFFFFU) ? 0U : g_min_cycle_time_us;

        if (out_capacity < sizeof(reply))
        {
            return 0U;
        }
        memset(&reply, 0, sizeof(reply));
        leap_write_le32((uint8_t *)&reply.last_cycle_time_us, g_last_cycle_time_us);
        leap_write_le32((uint8_t *)&reply.max_cycle_time_us, g_max_cycle_time_us);
        leap_write_le32((uint8_t *)&reply.min_cycle_time_us, min_cycle);
        leap_write_le32((uint8_t *)&reply.last_reply_latency_us, g_last_reply_latency_us);
        leap_write_le32((uint8_t *)&reply.max_reply_latency_us, g_max_reply_latency_us);
        leap_write_le32((uint8_t *)&reply.process_watchdog_remaining_us,
                          leap_remaining_us(g_watchdog_deadline_us, now_us));
        leap_write_le32((uint8_t *)&reply.owner_lease_remaining_us,
                          leap_remaining_us(g_lease_deadline_us, now_us));
        memcpy(out_payload, &reply, sizeof(reply));
        *reply_message_type_out = LEAP_DIAG_TIMING_REPLY;
        return (uint16_t)sizeof(reply);
    }

    return 0U;
}

static int leap_custom_net_rx(PoolPtr pp, uint16_t ocount, int if_num)
{
    const uint8_t *frame;
    uint16_t ethertype;
    uint16_t leap_length;
    uint16_t payload_offset = LEAP_ETH_HEADER_BYTES;
    uint8_t next_head;
    LeapRxFrame *slot;

    if (pp == 0 || ocount < LEAP_ETH_HEADER_BYTES)
    {
        return 0;
    }

    g_rx_callback_count++;

    frame = pp->pData;
    ethertype = (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);

    // Handle 802.1Q VLAN tagged frames only when explicitly enabled.
#if LEAP_ENABLE_VLAN_AWARE
    if (ethertype == 0x8100U && ocount >= (LEAP_ETH_HEADER_BYTES + 4U))
    {
        g_rx_vlan_seen_count++;
        ethertype = (uint16_t)(((uint16_t)frame[16] << 8) | frame[17]);
        payload_offset = (uint16_t)(LEAP_ETH_HEADER_BYTES + 4U);
    }
#endif

    if (ethertype != g_rx_ethertype_filter && ethertype != LEAP_ETHERTYPE_ALT)
    {
        g_rx_nonmatch_count++;
        g_rx_last_nonmatch_etype = ethertype;
        if (ethertype == 0x0800U)
        {
            g_rx_nonmatch_ipv4++;
        }
        else if (ethertype == 0x0806U)
        {
            g_rx_nonmatch_arp++;
        }
        else if (ethertype == 0x86DDU)
        {
            g_rx_nonmatch_ipv6++;
        }
        return 0;
    }

    USER_ENTER_CRITICAL();
    next_head = (uint8_t)((g_rx_head + 1U) % LEAP_RX_QUEUE_DEPTH);
    if (next_head == g_rx_tail)
    {
        g_rx_queue_drop_count++;
        USER_EXIT_CRITICAL();
        return 1;
    }
    slot = &g_rx_queue[g_rx_head];
    USER_EXIT_CRITICAL();

    slot->interface_number = if_num;
    slot->ethertype = ethertype;
    memcpy(slot->src_mac, &frame[6], 6);

    leap_length = (uint16_t)(ocount - payload_offset);
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
    return 1;
}

bool leap_transport_init(LeapTransport *transport, uint16_t ethertype_filter)
{
    if (transport == 0)
    {
        return false;
    }

    *transport = {};
    transport->ethertype_filter = ethertype_filter;
    transport->interface_number = GetFirstInterface();
    g_rx_ethertype_filter = ethertype_filter;
    g_rx_head = 0U;
    g_rx_tail = 0U;
    g_rx_queue_drop_count = 0U;
    g_rx_match_count = 0U;
    g_rx_callback_count = 0U;
    g_rx_nonmatch_count = 0U;
    g_rx_vlan_seen_count = 0U;
    g_rx_nonmatch_ipv4 = 0U;
    g_rx_nonmatch_arp = 0U;
    g_rx_nonmatch_ipv6 = 0U;
    g_rx_last_nonmatch_etype = 0U;

#if LEAP_ENABLE_PROMISC
    SetPromiscuous(TRUE);
#else
    SetPromiscuous(FALSE);
#endif

    SetCustomNetDoRX(leap_custom_net_rx);
    transport->custom_rx_registered = true;
    transport->initialized = true;

    LEAP_LOG_INFO("LEAP transport hook active on interface %ld, EtherType 0x%04X (alt 0x%04X enabled, VLAN-aware %s, promisc %s)\r\n",
                  (long)transport->interface_number,
                  transport->ethertype_filter,
                  LEAP_ETHERTYPE_ALT,
#if LEAP_ENABLE_VLAN_AWARE
                  "on",
#else
                  "off",
#endif
#if LEAP_ENABLE_PROMISC
                  "on"
#else
                  "off"
#endif
    );
    return true;
}

bool leap_transport_receive(LeapTransport *transport, LeapRxFrame *frame_out)
{
    LeapRxFrame *slot;

    if (transport == 0 || frame_out == 0 || !transport->initialized)
    {
        return false;
    }

    if (g_rx_tail == g_rx_head)
    {
        return false;
    }

    slot = &g_rx_queue[g_rx_tail];
    *frame_out = *slot;
    g_rx_tail = (uint8_t)((g_rx_tail + 1U) % LEAP_RX_QUEUE_DEPTH);
    return true;
}

bool leap_transport_send(LeapTransport *transport,
                         const uint8_t dst_mac[6],
                         uint16_t ethertype,
                         const uint8_t *payload,
                         uint16_t payload_len)
{
    PoolPtr pp;
    MACADR my_mac;
    uint16_t wire_payload_len;
    uint16_t frame_len;
    int32_t ifn;

    if (transport == 0 || dst_mac == 0 || payload == 0 || !transport->initialized)
    {
        return false;
    }

    if (payload_len > LEAP_MAX_FRAME_BYTES)
    {
        return false;
    }

    ifn = transport->interface_number;
    if (ifn <= 0)
    {
        ifn = GetFirstInterface();
    }
    if (ifn <= 0)
    {
        return false;
    }

    pp = GetBuffer();
    if (pp == 0)
    {
        return false;
    }

    my_mac = InterfaceMAC(ifn);
    memcpy(pp->pData, dst_mac, 6);
    for (uint8_t i = 0; i < 6U; i++)
    {
        pp->pData[6 + i] = my_mac.GetByte(i);
    }
    pp->pData[12] = (uint8_t)((ethertype >> 8) & 0xFFU);
    pp->pData[13] = (uint8_t)(ethertype & 0xFFU);
    memcpy(pp->pData + LEAP_ETH_HEADER_BYTES, payload, payload_len);

    wire_payload_len = payload_len;
    if (wire_payload_len < LEAP_MIN_TX_ETH_PAYLOAD)
    {
        memset(pp->pData + LEAP_ETH_HEADER_BYTES + wire_payload_len, 0,
               (size_t)(LEAP_MIN_TX_ETH_PAYLOAD - wire_payload_len));
        wire_payload_len = LEAP_MIN_TX_ETH_PAYLOAD;
    }

    frame_len = (uint16_t)(LEAP_ETH_HEADER_BYTES + wire_payload_len);
    pp->usedsize = frame_len;
    TransmitBuffer(pp, ifn);
    return true;
}

static uint16_t leap_build_discovery_payload(const LeapHeaderWire *request_header,
                                             const uint8_t *request_payload,
                                             uint16_t request_payload_len,
                                             uint16_t *reply_message_type_out,
                                             uint8_t *out_payload,
                                             uint16_t out_capacity,
                                             int32_t interface_number)
{
    static const uint16_t k_services[] = {
        LEAP_SERVICE_MGMT,
        LEAP_SERVICE_DISC,
        LEAP_SERVICE_DIR,
        LEAP_SERVICE_PD,
        LEAP_SERVICE_DIAG};

    if (request_header == 0 || reply_message_type_out == 0 || out_payload == 0)
    {
        return 0U;
    }

    if (request_header->message_type == LEAP_DISC_HELLO ||
        request_header->message_type == LEAP_DISC_IDENTIFY)
    {
        LeapHelloReplyWire body;
        MACADR source_mac = InterfaceMAC(interface_number > 0 ? interface_number : GetFirstInterface());
        uint16_t offset;

        if (out_capacity < (uint16_t)(sizeof(LeapHelloReplyWire) + sizeof(k_services)))
        {
            return 0U;
        }

        memset(&body, 0, sizeof(body));
        for (uint8_t i = 0; i < 6U; i++)
        {
            body.identity.primary_mac[i] = source_mac.GetByte(i);
        }
        leap_write_le32((uint8_t *)&body.default_profile_id, g_default_profile_id);
        leap_write_le32((uint8_t *)&body.active_profile_id, g_active_profile_id);
        leap_write_le16((uint8_t *)&body.current_state, g_device_state);
        leap_write_le16((uint8_t *)&body.supported_service_count, (uint16_t)(sizeof(k_services) / sizeof(k_services[0])));
        if (g_owner_active != 0U)
        {
            memcpy(body.active_owner_mac, g_owner_mac, sizeof(g_owner_mac));
        }
        leap_write_le16((uint8_t *)&body.locate_capability_flags, LEAP_LOCATE_FLAG_LED);

        memcpy(out_payload, &body, sizeof(body));
        offset = (uint16_t)sizeof(body);
        for (uint8_t i = 0U; i < (sizeof(k_services) / sizeof(k_services[0])); i++)
        {
            leap_write_le16(out_payload + offset, k_services[i]);
            offset = (uint16_t)(offset + 2U);
        }

        *reply_message_type_out = (request_header->message_type == LEAP_DISC_HELLO)
                                      ? LEAP_DISC_HELLO_REPLY
                                      : LEAP_DISC_IDENTIFY_REPLY;
        return offset;
    }

    if (request_header->message_type == LEAP_DISC_LOCATE_DEVICE)
    {
        LeapLocateDeviceReplyWire body;
        uint16_t duration_ms = 2500U;

        if (out_capacity < (uint16_t)sizeof(body))
        {
            return 0U;
        }

        memset(&body, 0, sizeof(body));
        body.supported = 1U;
        body.active = 1U;

        if (request_payload != 0 && request_payload_len >= (uint16_t)sizeof(LeapLocateDeviceRequestWire))
        {
            duration_ms = leap_read_le16(request_payload);
            if (duration_ms == 0U)
            {
                duration_ms = 2500U;
            }
        }
        leap_write_le16((uint8_t *)&body.remaining_ms, duration_ms);

        memcpy(out_payload, &body, sizeof(body));
        *reply_message_type_out = LEAP_DISC_LOCATE_DEVICE_REPLY;
        return (uint16_t)sizeof(body);
    }

    return 0U;
}

bool leap_runtime_init(LeapRuntime *runtime)
{
    if (runtime == 0)
    {
        return false;
    }

    *runtime = {};
    runtime->next_sequence = 1U;
    g_device_state = LEAP_STATE_CONFIGURED;
    g_active_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
    g_default_profile_id = LEAP_PROFILE_DIGITAL_IO_8X8;
    g_next_session_id = 1U;
    g_owner_session_id = 0U;
    memset(g_owner_mac, 0, sizeof(g_owner_mac));
    g_owner_active = 0U;
    g_do_shadow = 0U;
    g_di_shadow = 0U;
    g_io_status = LEAP_DIO_STATUS_OK;
    leap_board_io_init();
    leap_board_apply_outputs(g_do_shadow);
    if (!leap_transport_init(&runtime->transport, LEAP_ETHERTYPE_IN_USE))
    {
        return false;
    }

    return true;
}

void leap_runtime_poll(LeapRuntime *runtime)
{
    LeapRxFrame *slot;

    if (runtime == 0)
    {
        return;
    }

    while (g_rx_tail != g_rx_head)
    {
        USER_ENTER_CRITICAL();
        slot = &g_rx_queue[g_rx_tail];
        g_rx_tail = (uint8_t)((g_rx_tail + 1U) % LEAP_RX_QUEUE_DEPTH);
        USER_EXIT_CRITICAL();

        runtime->rx_frames++;
        if (!leap_runtime_dispatch_frame(runtime, slot))
        {
            runtime->dropped_frames++;
        }
    }

    /*
     * No LEAP frames queued: service NetBurner network timeouts/events so RX
     * keeps flowing without OSTimeDly(1) (50 ms per tick on default NBRTOS).
     */
    (void)leap_net_process_events();
}

static bool leap_runtime_dispatch_frame(LeapRuntime *runtime, const LeapRxFrame *frame)
{
    LeapHeaderWire header;
    const uint8_t *payload = 0;
    uint16_t reply_message_type = 0U;
    uint16_t reply_payload_len = 0U;
    uint16_t reply_frame_len = 0U;
    uint8_t *const reply_payload = g_reply_payload;
    uint8_t *const reply_frame = g_reply_frame;

    if (runtime == 0 || frame == 0)
    {
        return false;
    }

    if (frame->ethertype != LEAP_ETHERTYPE_IN_USE && frame->ethertype != LEAP_ETHERTYPE_ALT)
    {
        return false;
    }

    if (!leap_frame_parse_header(frame->payload, frame->payload_len, &header, &payload))
    {
        g_rx_frames_rejected++;
        if (LEAP_DEBUG_LOG)
        {
            static uint32_t parse_fail_count = 0U;
            parse_fail_count++;
            if ((parse_fail_count % 64U) == 1U)
            {
                LEAP_LOG_DEBUG("LEAP parse fail #%lu: etype=0x%04X len=%u first8=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                               (unsigned long)parse_fail_count,
                               frame->ethertype,
                               frame->payload_len,
                               frame->payload_len > 0U ? frame->payload[0] : 0U,
                               frame->payload_len > 1U ? frame->payload[1] : 0U,
                               frame->payload_len > 2U ? frame->payload[2] : 0U,
                               frame->payload_len > 3U ? frame->payload[3] : 0U,
                               frame->payload_len > 4U ? frame->payload[4] : 0U,
                               frame->payload_len > 5U ? frame->payload[5] : 0U,
                               frame->payload_len > 6U ? frame->payload[6] : 0U,
                               frame->payload_len > 7U ? frame->payload[7] : 0U);
            }
        }
        return false;
    }

    if ((header.flags & LEAP_FLAG_RESPONSE) != 0U)
    {
        return false;
    }

    leap_diag_note_request_rx(header.service_id, leap_monotonic_us());

    switch (header.service_id)
    {
    case LEAP_SERVICE_DISC:
        reply_payload_len = leap_build_discovery_payload(&header,
                                                         payload,
                                                         header.payload_length,
                                                         &reply_message_type,
                                                         reply_payload,
                                                         LEAP_MAX_FRAME_BYTES,
                                                         frame->interface_number);
        break;
    case LEAP_SERVICE_MGMT:
        reply_payload_len = leap_build_mgmt_payload(&header,
                                                    payload,
                                                    header.payload_length,
                                                    &reply_message_type,
                                                    reply_payload,
                                                    LEAP_MAX_FRAME_BYTES);
        break;
    case LEAP_SERVICE_DIR:
        reply_payload_len = leap_build_dir_payload(&header,
                                                   payload,
                                                   header.payload_length,
                                                   &reply_message_type,
                                                   reply_payload,
                                                   LEAP_MAX_FRAME_BYTES,
                                                   frame->interface_number);
        break;
    case LEAP_SERVICE_PD:
        reply_payload_len = leap_build_pd_payload(&header,
                                                  payload,
                                                  header.payload_length,
                                                  frame->src_mac,
                                                  &reply_message_type,
                                                  reply_payload,
                                                  LEAP_MAX_FRAME_BYTES);
        break;
    case LEAP_SERVICE_DIAG:
        reply_payload_len = leap_build_diag_payload(&header,
                                                    payload,
                                                    header.payload_length,
                                                    runtime,
                                                    &reply_message_type,
                                                    reply_payload,
                                                    LEAP_MAX_FRAME_BYTES);
        break;
    default:
        reply_payload_len = 0U;
        break;
    }

    if (reply_payload_len == 0U)
    {
        return true;
    }

    if (!leap_frame_build(reply_frame,
                          LEAP_MAX_FRAME_BYTES,
                          &reply_frame_len,
                          LEAP_FLAG_RESPONSE,
                          header.service_id,
                          reply_message_type,
                          header.session_id,
                          runtime->next_sequence++,
                          header.sequence,
                          reply_payload,
                          reply_payload_len))
    {
        return false;
    }

    if (!leap_transport_send(&runtime->transport,
                             frame->src_mac,
                             frame->ethertype,
                             reply_frame,
                             reply_frame_len))
    {
        g_tx_frames_dropped++;
        LEAP_LOG_ERROR("E LEAP TX failed: reply msg=0x%04X len=%u\r\n",
                       reply_message_type,
                       reply_frame_len);
        return false;
    }

    runtime->tx_frames++;
    leap_diag_note_reply_tx(header.service_id, leap_monotonic_us());
    return true;
}

/*-----------------------------------------------------------------------------
 * User Main
 *------------------------------------------------------------------------------*/
void UserMain(void *pd)
{
    (void)pd;
    init();
    // Always enable diagnostics so bus errors and exceptions print rather than
    // silently hard-resetting.  This is independent of the LEAP serial log level.
    EnableSystemDiagnostics();
    WaitForActiveNetwork(TICKS_PER_SECOND * 10);
#if LEAP_SERIAL_LOG_LEVEL >= LEAP_LOG_LEVEL_INFO
    showIpAddresses();
#endif

    LeapRuntime runtime;
    if (!leap_runtime_init(&runtime))
    {
        LEAP_LOG_ERROR("E LEAP runtime init failed\r\n");
        // Spin here — the NetBurner watchdog will eventually fire and restart.
        // EnableSystemDiagnostics() above will have printed the failure reason.
        for (;;) { OSTimeDly(10); }
    }

    while (1)
    {
        leap_runtime_poll(&runtime);
    }
}
