/*
 * LEAP device host for LP-AM243 (ICSSG raw Ethernet).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_host.h"
#include "board_config.h"
#include "leap_hw.h"
#include "leap_icssg_eth.h"

#include "leap/leap_device_host_perf.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_diag_device.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include "leap_am243_log.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include <string.h>

/* PD EXCHANGE fast-path TX (header + LEAP_PD_DEVICE_MAX_REPLY). */
#define LEAP_AM243_PD_TX_BUF_MAX \
    (LEAP_HEADER_LENGTH_V1 + LEAP_PD_DEVICE_MAX_REPLY + 8u)

typedef struct LeapHostRxItem
{
    EnetMp_PerCtxt *port;
    uint8_t  src_mac[6];
    uint16_t payload_length;
    uint8_t  payload[LEAP_HOST_MAX_FRAME];
} LeapHostRxItem;

static LeapDeviceStack       s_stack;
static LeapAm243IoShadow     s_io;
static LeapPdDeviceIoBinding s_pd_io;
static QueueHandle_t         s_rx_queue;
static StaticQueue_t         s_rx_queue_obj;
static uint8_t               s_rx_queue_storage[LEAP_HOST_RX_DEPTH * sizeof(LeapHostRxItem)];
static LeapHostStats         s_stats;
static TaskHandle_t          s_leap_task_notify;
static uint8_t               s_tx_frame[LEAP_HOST_MAX_FRAME];
static LeapDeviceStackResult s_process_result;
static uint64_t              s_locate_until_us;
static uint64_t              s_locate_next_toggle_us;
static uint8_t               s_locate_led_on;
static uint8_t               s_locate_pattern;
static uint8_t               s_locate_solid;
static SemaphoreHandle_t     s_stack_mutex;
static StaticSemaphore_t     s_stack_mutex_obj;

static void leap_pd_apply_outputs(uint16_t outputs, void *ctx)
{
    (void)ctx;
    leap_hw_apply_outputs(&s_io, outputs);
}

static void leap_record_tx_result(int send_ok)
{
    if (send_ok == 0) {
        leap_device_stack_notify_tx_ok(&s_stack, leap_hw_monotonic_us());
        ++s_stats.tx_ok;
    } else {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
    }
}

static int leap_send_error_reply(EnetMp_PerCtxt *port,
                                 const uint8_t *dst_mac,
                                 const LeapDeviceStackResult *result,
                                 uint16_t service_id, uint16_t message_type,
                                 uint16_t status_code)
{
    LeapErrorPayload err;
    size_t           tx_len = 0u;
    int              send_rc = -1;
    unsigned         attempt;

    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

    if (leap_frame_write(
            s_tx_frame, sizeof(s_tx_frame), &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR | LEAP_FLAG_ACK_REQUESTED),
            service_id, message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            (const uint8_t *)&err, sizeof(err)) != 0) {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return -1;
    }

    for (attempt = 0u; attempt < 4u; attempt++) {
        send_rc = leap_icssg_eth_send_on(port, dst_mac, s_tx_frame, tx_len);
        if (send_rc == 0) {
            break;
        }
        leap_icssg_eth_poll_tx();
    }

    leap_record_tx_result(send_rc);
    return send_rc;
}

static void leap_send_pd_error_from_view(EnetMp_PerCtxt *port,
                                         const uint8_t *dst_mac,
                                         const LeapFrameView *request,
                                         uint16_t status_code)
{
    LeapDeviceStackResult stack_result;

    if (port == NULL || dst_mac == NULL || request == NULL) {
        return;
    }

    memset(&stack_result, 0, sizeof(stack_result));
    stack_result.frame = *request;
    (void)leap_send_error_reply(
        port,
        dst_mac,
        &stack_result,
        (uint16_t)LEAP_SERVICE_PD,
        request->header.message_type,
        status_code);
}

static void leap_send_reply(EnetMp_PerCtxt *port,
                            const uint8_t *dst_mac,
                            const LeapDeviceStackResult *result,
                            uint16_t service_id, uint16_t message_type,
                            const uint8_t *payload, size_t payload_length)
{
    size_t tx_len = 0u;
    int    send_rc;

    if (leap_frame_write(
            s_tx_frame, sizeof(s_tx_frame), &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            service_id, message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            payload, payload_length) != 0) {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return;
    }

    for (unsigned attempt = 0u; attempt < 4u; attempt++) {
        send_rc = leap_icssg_eth_send_on(port, dst_mac, s_tx_frame, tx_len);
        if (send_rc == 0) {
            break;
        }
        leap_icssg_eth_poll_tx();
    }
    if (service_id == (uint16_t)LEAP_SERVICE_PD &&
        message_type == LEAP_PD_EXCHANGE_REPLY)
    {
        if (send_rc != 0)
        {
            LEAP_AM243_LOG_ERROR(
                "LEAP PD EXCHANGE_REPLY TX failed len=%u sid=%u seq=%u\r\n",
                (unsigned)payload_length,
                (unsigned)result->frame.header.session_id,
                (unsigned)result->frame.header.sequence);
            (void)leap_send_error_reply(
                port,
                dst_mac,
                result,
                service_id,
                result->frame.header.message_type,
                LEAP_STATUS_BUSY);
        }
    }
    else if (send_rc != 0 &&
             service_id == (uint16_t)LEAP_SERVICE_MGMT)
    {
        LEAP_AM243_LOG_ERROR("LEAP MGMT TX failed msg=0x%04X sid=%u seq=%u\r\n",
                             (unsigned)message_type,
                             (unsigned)result->frame.header.session_id,
                             (unsigned)result->frame.header.sequence);
    }
    leap_record_tx_result(send_rc);
}

/*
 * M2a: PD EXCHANGE fast path — single parse, bypass full device-stack dispatch.
 * Ported from platforms/clearcore/.../clearcore_leap_host.c.
 */
static int leap_send_pd_exchange_reply(EnetMp_PerCtxt *port,
                                       const uint8_t *dst_mac,
                                       const LeapFrameView *request,
                                       uint16_t message_type,
                                       const uint8_t *payload,
                                       size_t payload_length)
{
    uint8_t tx[LEAP_AM243_PD_TX_BUF_MAX];
    size_t  tx_len = 0u;
    int     send_rc;

    if (port == NULL || dst_mac == NULL || request == NULL) {
        return -1;
    }

    if (leap_frame_write(
            tx, sizeof(tx), &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_PD,
            message_type,
            request->header.session_id,
            request->header.sequence,
            request->header.ack_sequence,
            payload, payload_length) != 0) {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return -1;
    }

    for (unsigned attempt = 0u; attempt < 4u; attempt++) {
        send_rc = leap_icssg_eth_send_on(port, dst_mac, tx, tx_len);
        if (send_rc == 0) {
            break;
        }
        leap_icssg_eth_poll_tx();
    }

    if (message_type == LEAP_PD_EXCHANGE_REPLY && send_rc != 0) {
        LEAP_AM243_LOG_ERROR(
            "LEAP PD EXCHANGE_REPLY TX failed len=%u sid=%u seq=%u\r\n",
            (unsigned)payload_length,
            (unsigned)request->header.session_id,
            (unsigned)request->header.sequence);
    }

    leap_record_tx_result(send_rc);
    return send_rc;
}

static int leap_handle_pd_exchange_fast(const LeapHostRxItem *item,
                                        const LeapFrameView *view,
                                        uint64_t now_us)
{
    LeapPdDeviceResult    pd_result;
    LeapDeviceStackResult stack_result;
    LeapPdDeviceStatus    pd_status;

    if (item == NULL || view == NULL) {
        return 0;
    }

    pd_status = leap_pd_device_process_parsed_frame(
        &s_stack.mgmt,
        &s_stack.pd,
        &s_pd_io,
        item->src_mac,
        now_us,
        view,
        &pd_result);

    if (pd_status == LEAP_PD_DEVICE_OK &&
        pd_result.reply_payload_length > 0u) {
        leap_device_stack_note_frame_rx(
            &s_stack,
            now_us,
            (uint16_t)LEAP_SERVICE_PD);

        /*
         * ClearCore parity: PD is already consumed (sequence + watchdog).
         * Never fall through to the full stack on TX failure — that re-enters
         * leap_pd_device_process_parsed_frame and rejects DUPLICATE_SEQUENCE.
         */
        if (leap_send_pd_exchange_reply(
                item->port,
                item->src_mac,
                view,
                pd_result.reply_message_type,
                pd_result.reply_payload,
                pd_result.reply_payload_length) != 0) {
            /*
             * PD sequence is already consumed. A silent TX drop leaves the
             * controller waiting 500 ms; BUSY on the wire triggers fast
             * re-bootstrap instead of a soak timeout.
             */
            (void)leap_send_pd_error_from_view(
                item->port, item->src_mac, view, LEAP_STATUS_BUSY);
        }

        leap_icssg_eth_poll_tx();
        return 1;
    }

    if (pd_status == LEAP_PD_DEVICE_IGNORED_RESPONSE &&
        pd_result.error_code == LEAP_STATUS_NOT_OWNER) {
        static uint32_t not_owner_log_count;

        if (not_owner_log_count < 8u) {
            ++not_owner_log_count;
            LEAP_AM243_LOG_WARN(
                "LEAP PD NOT_OWNER msg=0x%04X req_sid=%u owner_sid=%u state=%u seq=%u\r\n",
                (unsigned)view->header.message_type,
                (unsigned)view->header.session_id,
                (unsigned)s_stack.mgmt.owner_session_id,
                (unsigned)s_stack.mgmt.device_state,
                (unsigned)view->header.sequence);
        }

        memset(&stack_result, 0, sizeof(stack_result));
        stack_result.frame = pd_result.frame;
        leap_send_error_reply(
            item->port,
            item->src_mac,
            &stack_result,
            (uint16_t)LEAP_SERVICE_PD,
            view->header.message_type,
            pd_result.error_code);
        leap_icssg_eth_poll_tx();
        return 1;
    }

    if (pd_status == LEAP_PD_DEVICE_REJECTED) {
        leap_diag_device_on_pd_result(&s_stack.diag, &pd_result, now_us);
        memset(&stack_result, 0, sizeof(stack_result));
        stack_result.frame = pd_result.frame;
        leap_send_error_reply(
            item->port,
            item->src_mac,
            &stack_result,
            (uint16_t)LEAP_SERVICE_PD,
            pd_result.frame.header.message_type,
            pd_result.error_code);
        LEAP_AM243_LOG_WARN("LEAP PD rejected err=0x%04X state=%u sid=%u seq=%u\r\n",
                   (unsigned)pd_result.error_code,
                   (unsigned)s_stack.mgmt.device_state,
                   (unsigned)pd_result.frame.header.session_id,
                   (unsigned)pd_result.frame.header.sequence);
        return 1;
    }

    return 0;
}

static int leap_host_frame_is_priority_control(const uint8_t *payload,
                                               size_t         payload_length)
{
    uint16_t service_id;

    if (payload == NULL ||
        leap_device_frame_peek_service_id(
            payload, payload_length, &service_id) != 0)
    {
        return 0;
    }

    return (service_id == (uint16_t)LEAP_SERVICE_MGMT ||
            service_id == (uint16_t)LEAP_SERVICE_DISC ||
            service_id == (uint16_t)LEAP_SERVICE_DIR) ?
               1 :
               0;
}

static int leap_item_needs_input_refresh(const LeapHostRxItem *item)
{
    uint16_t service_id;

    if (item == NULL) {
        return 0;
    }

    if (leap_device_frame_peek_service_id(
            item->payload,
            item->payload_length,
            &service_id) != 0) {
        return 0;
    }

    return (service_id == (uint16_t)LEAP_SERVICE_PD) ? 1 : 0;
}

static uint32_t leap_locate_toggle_us(uint8_t pattern)
{
    switch (pattern) {
    case LEAP_LOCATE_PATTERN_FAST_BLINK:
        return 100000u;
    case LEAP_LOCATE_PATTERN_DOUBLE_BLINK:
        return 125000u;
    case LEAP_LOCATE_PATTERN_SLOW_BLINK:
    case LEAP_LOCATE_PATTERN_DEFAULT:
    default:
        return 250000u;
    }
}

static void leap_locate_stop(uint64_t now_us)
{
    s_locate_until_us       = now_us;
    s_locate_next_toggle_us = 0u;
    s_locate_solid          = 0u;
    if (s_locate_led_on != 0u) {
        s_locate_led_on = 0u;
        leap_hw_set_status_led(0u);
    }
}

static void leap_locate_start(uint32_t duration_us, uint8_t pattern, int cancel)
{
    uint64_t now_us = leap_hw_monotonic_us();

    if (cancel != 0) {
        leap_locate_stop(now_us);
        return;
    }

    if (duration_us == 0u) {
        duration_us = 3000000u;
    }

    s_locate_until_us       = now_us + (uint64_t)duration_us;
    s_locate_next_toggle_us = 0u;
    s_locate_pattern        = pattern;
    s_locate_solid          =
        (pattern == LEAP_LOCATE_PATTERN_SOLID) ? 1u : 0u;

    if (s_locate_solid != 0u) {
        s_locate_led_on = 1u;
        leap_hw_set_status_led(1u);
    }
}

static void leap_handle_result(EnetMp_PerCtxt *port,
                               const uint8_t *src_mac,
                               LeapDeviceStackStatus status,
                               const LeapDeviceStackResult *result)
{
    if (status == LEAP_DEVICE_STACK_OK) {
        if (result->service_id == LEAP_SERVICE_DISC &&
            result->frame.header.message_type == LEAP_DISC_LOCATE_DEVICE &&
            result->frame.payload_length >= sizeof(LeapLocateDeviceRequest)) {
            const LeapLocateDeviceRequest *req =
                (const LeapLocateDeviceRequest *)result->frame.payload;

            if ((req->flags & LEAP_LOCATE_FLAG_CANCEL) != 0u) {
                leap_locate_start(0u, 0u, 1);
            } else {
                uint16_t accepted_ms =
                    leap_disc_clamp_locate_duration_ms(req->duration_ms);

                leap_locate_start((uint32_t)accepted_ms * 1000u, req->pattern, 0);
            }
        }

        if ((result->flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u) {
            leap_send_reply(port, src_mac, result,
                            (uint16_t)LEAP_SERVICE_DISC,
                            result->disc_message_type,
                            result->disc_payload,
                            result->disc_payload_length);
        } else if ((result->flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u) {
            leap_send_reply(port, src_mac, result,
                            (uint16_t)LEAP_SERVICE_MGMT,
                            result->mgmt_reply.message_type,
                            result->mgmt_reply.payload,
                            result->mgmt_reply.payload_length);
        } else if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u) {
            leap_send_reply(port, src_mac, result,
                            (uint16_t)LEAP_SERVICE_DIR,
                            result->dir_message_type,
                            result->dir_payload,
                            result->dir_payload_length);
        }

        /* ClearCore pattern: PD reply is not chained behind DISC/MGMT/DIR. */
        if ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u ||
            result->pd_reply_payload_length > 0u) {
            leap_send_reply(port, src_mac, result,
                            (uint16_t)LEAP_SERVICE_PD,
                            result->pd_reply_message_type,
                            result->pd_reply_payload,
                            result->pd_reply_payload_length);
        } else if (result->service_id == (uint16_t)LEAP_SERVICE_PD &&
                   result->frame.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS &&
                   result->error_code != 0u &&
                   result->error_code != (uint16_t)LEAP_STATUS_OK) {
            leap_send_error_reply(
                port,
                src_mac,
                result,
                (uint16_t)LEAP_SERVICE_PD,
                result->frame.header.message_type,
                result->error_code);
            LEAP_AM243_LOG_WARN(
                "LEAP PD EXCHANGE error reply err=0x%04X state=%u sid=%u seq=%u\r\n",
                (unsigned)result->error_code,
                (unsigned)result->device_state,
                (unsigned)result->frame.header.session_id,
                (unsigned)result->frame.header.sequence);
        } else if (result->service_id == (uint16_t)LEAP_SERVICE_PD &&
                   result->frame.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS) {
            LEAP_AM243_LOG_WARN(
                "LEAP PD EXCHANGE no reply flags=0x%08X err=0x%04X state=%u sid=%u seq=%u\r\n",
                       (unsigned)result->flags,
                       (unsigned)result->error_code,
                       (unsigned)result->device_state,
                       (unsigned)result->frame.header.session_id,
                       (unsigned)result->frame.header.sequence);
        } else if (result->service_id == (uint16_t)LEAP_SERVICE_DIAG &&
                   (result->flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u) {
            leap_send_reply(port, src_mac, result,
                            (uint16_t)LEAP_SERVICE_DIAG,
                            result->diag_message_type,
                            result->diag_payload,
                            result->diag_payload_length);
        }
    } else if (status == LEAP_DEVICE_STACK_PD_REJECTED) {
        LEAP_AM243_LOG_WARN("LEAP PD rejected err=0x%04X state=%u sid=%u seq=%u\r\n",
                   (unsigned)result->error_code,
                   (unsigned)result->device_state,
                   (unsigned)result->frame.header.session_id,
                   (unsigned)result->frame.header.sequence);
        leap_send_error_reply(port, src_mac, result,
                              (uint16_t)LEAP_SERVICE_PD,
                              result->frame.header.message_type,
                              result->error_code);
    } else if (status == LEAP_DEVICE_STACK_DIAG_ERROR) {
        leap_send_error_reply(port, src_mac, result,
                              (uint16_t)LEAP_SERVICE_DIAG,
                              result->frame.header.message_type,
                              result->error_code);
    } else if (status == LEAP_DEVICE_STACK_DIR_ERROR) {
        leap_send_error_reply(port, src_mac, result,
                              (uint16_t)LEAP_SERVICE_DIR,
                              result->frame.header.message_type,
                              result->error_code);
    } else if (status == LEAP_DEVICE_STACK_MGMT_ERROR) {
        LEAP_AM243_LOG_WARN(
            "LEAP MGMT rejected msg=0x%04X err=0x%04X req_sid=%u owner_sid=%u state=%u\r\n",
            (unsigned)result->frame.header.message_type,
            (unsigned)result->error_code,
            (unsigned)result->frame.header.session_id,
            (unsigned)s_stack.mgmt.owner_session_id,
            (unsigned)s_stack.mgmt.device_state);
        if (result->error_code != 0u &&
            result->error_code != (uint16_t)LEAP_STATUS_OK) {
            leap_send_error_reply(
                port,
                src_mac,
                result,
                (uint16_t)LEAP_SERVICE_MGMT,
                result->frame.header.message_type,
                result->error_code);
            leap_icssg_eth_poll_tx();
        }
    } else if (status == LEAP_DEVICE_STACK_FRAME_ERROR) {
        LEAP_AM243_LOG_ERROR("LEAP frame error=%d\r\n", (int)result->frame_error);
    } else {
        LEAP_AM243_LOG_WARN("LEAP stack status=%d svc=0x%04X\r\n",
                   (int)status, (unsigned)result->service_id);
    }
}

static void leap_process_item(const LeapHostRxItem *item)
{
    LeapDeviceStackStatus status;
    uint64_t              now_us;

    if (item == NULL || item->payload_length == 0u) {
        return;
    }

    if (leap_item_needs_input_refresh(item) != 0) {
        leap_hw_refresh_inputs(&s_io);
    }

    now_us = leap_hw_monotonic_us();

    {
        LeapFrameView        view;
        LeapFrameParseResult parse_result;

        parse_result =
            leap_frame_parse(item->payload, item->payload_length, &view);
        if (parse_result == LEAP_FRAME_OK &&
            view.header.service_id == (uint16_t)LEAP_SERVICE_PD &&
            view.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS &&
            leap_handle_pd_exchange_fast(item, &view, now_us) != 0) {
            ++s_stats.rx_ok;
            return;
        }
    }

    status = leap_device_stack_process_frame(
        &s_stack, item->src_mac, now_us,
        item->payload, item->payload_length, &s_process_result);

    if (status == LEAP_DEVICE_STACK_FRAME_ERROR) {
        LEAP_AM243_LOG_ERROR(
            "LEAP RX parse fail err=%d len=%u b=%02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   (int)s_process_result.frame_error,
                   (unsigned)item->payload_length,
                   item->payload_length > 0u ? item->payload[0] : 0U,
                   item->payload_length > 1u ? item->payload[1] : 0U,
                   item->payload_length > 2u ? item->payload[2] : 0U,
                   item->payload_length > 3u ? item->payload[3] : 0U,
                   item->payload_length > 4u ? item->payload[4] : 0U,
                   item->payload_length > 5u ? item->payload[5] : 0U,
                   item->payload_length > 6u ? item->payload[6] : 0U,
                   item->payload_length > 7u ? item->payload[7] : 0U);
    }

    leap_handle_result(item->port, item->src_mac, status, &s_process_result);
    leap_icssg_eth_poll_tx();
    ++s_stats.rx_ok;
}

static void leap_update_locate_led(uint64_t now_us)
{
    if (s_locate_until_us == 0u) {
        return;
    }

    if (now_us >= s_locate_until_us) {
        leap_locate_stop(now_us);
        s_locate_until_us = 0u;
        return;
    }

    if (s_locate_solid != 0u) {
        if (s_locate_led_on == 0u) {
            s_locate_led_on = 1u;
            leap_hw_set_status_led(1u);
        }
        return;
    }

    if (s_locate_next_toggle_us == 0u || now_us >= s_locate_next_toggle_us) {
        s_locate_led_on ^= 1u;
        s_locate_next_toggle_us =
            now_us + leap_locate_toggle_us(s_locate_pattern);
        leap_hw_set_status_led(s_locate_led_on);
    }
}

int leap_host_init(const uint8_t mac[6])
{
    LeapDeviceStackConfig stack_config;

    if (mac == NULL) {
        return -1;
    }

    memset(&s_stats, 0, sizeof(s_stats));

    s_rx_queue = xQueueCreateStatic(LEAP_HOST_RX_DEPTH,
                                    sizeof(LeapHostRxItem),
                                    s_rx_queue_storage,
                                    &s_rx_queue_obj);
    if (s_rx_queue == NULL) {
        LEAP_AM243_LOG_ERROR("LEAP RX queue create failed\r\n");
        return -1;
    }

    s_stack_mutex = xSemaphoreCreateMutexStatic(&s_stack_mutex_obj);
    if (s_stack_mutex == NULL) {
        LEAP_AM243_LOG_ERROR("LEAP stack mutex create failed\r\n");
        return -1;
    }

    memset(&s_io, 0, sizeof(s_io));
    s_io.io_status = LEAP_DIO_STATUS_OK;
    s_io.safe_active = 1u;
    leap_hw_init();

    memset(&stack_config, 0, sizeof(stack_config));
    stack_config.mgmt.default_lease_us    = 5000000u;
    stack_config.mgmt.default_watchdog_us = 5000000u;
    stack_config.mgmt.max_lease_us        = 10000000u;
    stack_config.mgmt.max_watchdog_us     = 10000000u;
    (void)leap_dir_device_config_set_digital_io(
        &stack_config.dir,
        LEAP_AM243_PROFILE_ID,
        (uint16_t)LEAP_AM243_DO_COUNT,
        (uint16_t)LEAP_AM243_DI_COUNT);

    leap_device_stack_init_full(&s_stack, &stack_config);

    memset(&s_pd_io, 0, sizeof(s_pd_io));
    s_pd_io.digital_outputs   = &s_io.digital_outputs;
    s_pd_io.digital_inputs    = &s_io.digital_inputs;
    s_pd_io.io_status         = &s_io.io_status;
    s_pd_io.apply_outputs     = leap_pd_apply_outputs;
    s_pd_io.apply_outputs_ctx = NULL;
    leap_device_stack_bind_pd_io(&s_stack, &s_pd_io);

    memcpy(s_stack.dir.config.identity.primary_mac, mac, 6);
    s_stack.dir.config.identity.product_code      = LEAP_AM243_PRODUCT_CODE;
    s_stack.dir.config.identity.firmware_revision = LEAP_AM243_FIRMWARE_REVISION;
    memcpy(s_stack.disc.config.identity.primary_mac, mac, 6);
    s_stack.disc.config.identity.product_code      = LEAP_AM243_PRODUCT_CODE;
    s_stack.disc.config.identity.firmware_revision = LEAP_AM243_FIRMWARE_REVISION;
    leap_dir_device_sync_disc(&s_stack.dir, &s_stack.disc);
    leap_mgmt_device_on_transport_ready(&s_stack.mgmt);

    s_locate_until_us       = 0u;
    s_locate_next_toggle_us = 0u;
    s_locate_led_on         = 0u;
    s_locate_pattern        = LEAP_LOCATE_PATTERN_DEFAULT;
    s_locate_solid          = 0u;

    LEAP_AM243_LOG_INFO("LEAP AM243 host ready — waiting for master\r\n");
    return 0;
}

int leap_host_try_inline_pd_exchange(EnetMp_PerCtxt *port,
                                     const uint8_t *src_mac,
                                     const uint8_t *payload,
                                     size_t payload_length)
{
    LeapHostRxItem      item;
    LeapFrameView       view;
    LeapFrameParseResult parse_result;
    uint16_t            service_id;
    uint64_t            now_us;

    if (port == NULL || src_mac == NULL || payload == NULL ||
        payload_length == 0u || s_stack_mutex == NULL) {
        return 0;
    }

    if (leap_device_frame_peek_service_id(
            payload, payload_length, &service_id) != 0 ||
        service_id != (uint16_t)LEAP_SERVICE_PD) {
        return 0;
    }

    parse_result = leap_frame_parse(payload, payload_length, &view);
    if (parse_result != LEAP_FRAME_OK ||
        view.header.message_type != LEAP_PD_EXCHANGE_ENDPOINTS) {
        return 0;
    }

    if (xSemaphoreTake(s_stack_mutex, 0) != pdTRUE) {
        return 0;
    }

    item.port = port;
    memcpy(item.src_mac, src_mac, 6);
    item.payload_length = (uint16_t)payload_length;
    if (payload_length > LEAP_HOST_MAX_FRAME) {
        xSemaphoreGive(s_stack_mutex);
        return 0;
    }
    memcpy(item.payload, payload, payload_length);

    leap_hw_refresh_inputs(&s_io);
    now_us = leap_hw_monotonic_us();

    if (leap_handle_pd_exchange_fast(&item, &view, now_us) != 0) {
        ++s_stats.rx_ok;
        xSemaphoreGive(s_stack_mutex);
        return 1;
    }

    xSemaphoreGive(s_stack_mutex);
    return 0;
}

int leap_host_queue_frame(EnetMp_PerCtxt *port, const uint8_t *src_mac,
                          const uint8_t *payload, size_t payload_length)
{
    LeapHostRxItem item;

    if (port == NULL || src_mac == NULL || payload == NULL || payload_length == 0u ||
        payload_length > LEAP_HOST_MAX_FRAME || s_rx_queue == NULL) {
        ++s_stats.rx_drop;
        return -1;
    }

    item.port = port;
    memcpy(item.src_mac, src_mac, 6);
    memcpy(item.payload, payload, payload_length);
    item.payload_length = (uint16_t)payload_length;

    if (leap_host_frame_is_priority_control(payload, payload_length) != 0)
    {
        if (xQueueSendToFront(s_rx_queue, &item, 0) != pdTRUE)
        {
            if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE)
            {
                ++s_stats.rx_drop;
                return -1;
            }
        }
    }
    else if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE)
    {
        static uint32_t rx_drop_log_count;

        ++s_stats.rx_drop;
        if (rx_drop_log_count < 4u) {
            ++rx_drop_log_count;
            LEAP_AM243_LOG_ERROR("LEAP RX queue full (drop=%u)\r\n",
                       (unsigned)s_stats.rx_drop);
        }
        return -1;
    }

    ++s_stats.rx_queued;
    if (s_leap_task_notify != NULL) {
        (void)xTaskNotifyGive(s_leap_task_notify);
    }
    return 0;
}

void leap_host_bind_task_handle(void *task_handle)
{
    s_leap_task_notify = (TaskHandle_t)task_handle;
}

int leap_host_rx_pending(void)
{
    if (s_rx_queue == NULL) {
        return 0;
    }

    return (uxQueueMessagesWaiting(s_rx_queue) > 0u) ? 1 : 0;
}

void leap_host_cyclic(void)
{
    uint32_t tick_flags = 0u;
    uint64_t now_us;
    LeapHostRxItem item;

    if (s_stack_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_stack_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    leap_icssg_eth_poll_tx();

    for (;;) {
        if (xQueueReceive(s_rx_queue, &item, 0) != pdTRUE) {
            break;
        }
        leap_process_item(&item);
        leap_icssg_eth_poll_tx();
    }

    now_us = leap_hw_monotonic_us();
    (void)leap_device_stack_tick(&s_stack, now_us, &tick_flags);

    if ((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u) {
        uint64_t wd_remain_us = 0u;
        uint64_t lease_remain_us = 0u;

        if (s_stack.mgmt.watchdog_deadline_us > now_us) {
            wd_remain_us = s_stack.mgmt.watchdog_deadline_us - now_us;
        }
        if (s_stack.mgmt.lease_deadline_us > now_us) {
            lease_remain_us = s_stack.mgmt.lease_deadline_us - now_us;
        }

        LEAP_AM243_LOG_WARN(
            "LEAP SAFE (lease/watchdog) owner_sid=%u state=%u "
            "wd_remain=%u ms lease_remain=%u ms rx_drop=%u\r\n",
            (unsigned)s_stack.mgmt.owner_session_id,
            (unsigned)s_stack.mgmt.device_state,
            (unsigned)(wd_remain_us / 1000u),
            (unsigned)(lease_remain_us / 1000u),
            (unsigned)s_stats.rx_drop);
        leap_hw_enter_safe(&s_io);
    }

    leap_icssg_eth_poll_tx();
    leap_update_locate_led(now_us);
    xSemaphoreGive(s_stack_mutex);
}

const LeapHostStats *leap_host_stats(void)
{
    return &s_stats;
}
