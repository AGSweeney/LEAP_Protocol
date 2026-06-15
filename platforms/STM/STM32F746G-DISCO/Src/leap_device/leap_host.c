/*
 * LEAP device host for STM32F746G-Discovery.
 *
 * IO profile: LEAP_PROFILE_DIGITAL_IO_8X8 (8 outputs + 8 inputs, simulated).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_host.h"
#include "board_config.h"
#include "leap_eth.h"
#include "leap_hw.h"

#include "leap/leap_device_host_perf.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "lwip/netif.h"

#include <string.h>

typedef struct LeapHostRxItem
{
    uint8_t  src_mac[6];
    uint16_t payload_length;
    uint8_t  payload[LEAP_HOST_MAX_FRAME];
} LeapHostRxItem;

static LeapDeviceStack       s_stack;
static LeapStm746IoShadow    s_io;
static LeapPdDeviceIoBinding s_pd_io;
static struct netif         *s_netif;
static QueueHandle_t         s_rx_queue;
static StaticQueue_t         s_rx_queue_struct;
static uint8_t               s_rx_queue_storage[LEAP_HOST_RX_DEPTH * sizeof(LeapHostRxItem)];
static TaskHandle_t          s_leap_task_notify;
static uint8_t               s_host_ready;
static LeapHostStats         s_stats;
static uint64_t              s_locate_until_us;
static uint64_t              s_locate_next_toggle_us;
static uint8_t               s_locate_led_on;
static uint8_t               s_locate_pattern;
static uint8_t               s_locate_solid;
static uint8_t               s_tx_frame[LEAP_HOST_MAX_FRAME];
static LeapDeviceStackResult s_process_result;

static void leap_host_enter_safe_cb(void *ctx)
{
    leap_hw_enter_safe((LeapStm746IoShadow *)ctx);
}

static void leap_pd_apply_outputs(uint16_t outputs, void *ctx)
{
    (void)ctx;
    leap_hw_apply_outputs(&s_io, outputs);
}

static void leap_record_tx_result(int send_ok)
{
    if (send_ok == 0)
    {
        leap_device_stack_notify_tx_ok(&s_stack, leap_hw_monotonic_us());
        ++s_stats.tx_ok;
    }
    else
    {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
    }
}

static void leap_send_error_reply(const uint8_t *dst_mac,
                                  const LeapDeviceStackResult *result,
                                  uint16_t service_id, uint16_t message_type,
                                  uint16_t status_code)
{
    LeapErrorPayload err;
    size_t           tx_len = 0u;

    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

    if (leap_frame_write(
            s_tx_frame, sizeof(s_tx_frame), &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR | LEAP_FLAG_ACK_REQUESTED),
            service_id, message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            (const uint8_t *)&err, sizeof(err)) != 0)
    {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return;
    }

    leap_record_tx_result(leap_eth_send(s_netif, dst_mac, s_tx_frame, tx_len));
}

static void leap_send_reply(const uint8_t *dst_mac,
                            const LeapDeviceStackResult *result,
                            uint16_t service_id, uint16_t message_type,
                            const uint8_t *payload, size_t payload_length)
{
    size_t tx_len = 0u;

    if (leap_frame_write(
            s_tx_frame, sizeof(s_tx_frame), &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            service_id, message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            payload, payload_length) != 0)
    {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return;
    }

    leap_record_tx_result(leap_eth_send(s_netif, dst_mac, s_tx_frame, tx_len));
}

static uint32_t leap_locate_toggle_us(uint8_t pattern)
{
    switch (pattern)
    {
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
    if (s_locate_led_on != 0u)
    {
        s_locate_led_on = 0u;
        leap_hw_set_locate_led(0);
    }
}

static void leap_locate_start(uint32_t duration_us, uint8_t pattern, int cancel)
{
    uint64_t now_us = leap_hw_monotonic_us();

    if (cancel != 0)
    {
        leap_locate_stop(now_us);
        return;
    }

    if (duration_us == 0u)
    {
        duration_us = 3000000u;
    }

    s_locate_until_us       = now_us + (uint64_t)duration_us;
    s_locate_next_toggle_us = 0u;
    s_locate_pattern        = pattern;
    s_locate_solid          = (pattern == LEAP_LOCATE_PATTERN_SOLID) ? 1u : 0u;

    if (s_locate_solid != 0u)
    {
        s_locate_led_on = 1u;
        leap_hw_set_locate_led(1);
    }
}

static void leap_host_sync_identity(struct netif *netif)
{
    if (netif == NULL)
    {
        return;
    }

    memcpy(s_stack.dir.config.identity.primary_mac, netif->hwaddr, 6);
    s_stack.dir.config.identity.product_code      = LEAP_STM746_PRODUCT_CODE;
    s_stack.dir.config.identity.firmware_revision = LEAP_STM746_FIRMWARE_REVISION;
    memcpy(s_stack.disc.config.identity.primary_mac, netif->hwaddr, 6);
    s_stack.disc.config.identity.product_code      = LEAP_STM746_PRODUCT_CODE;
    s_stack.disc.config.identity.firmware_revision = LEAP_STM746_FIRMWARE_REVISION;
    leap_dir_device_sync_disc(&s_stack.dir, &s_stack.disc);
}

static void leap_update_locate_led(uint64_t now_us)
{
    if (s_locate_until_us == 0u)
    {
        return;
    }

    if (now_us >= s_locate_until_us)
    {
        leap_locate_stop(now_us);
        s_locate_until_us = 0u;
        return;
    }

    if (s_locate_solid != 0u)
    {
        if (s_locate_led_on == 0u)
        {
            s_locate_led_on = 1u;
            leap_hw_set_locate_led(1);
        }
        return;
    }

    if (s_locate_next_toggle_us == 0u || now_us >= s_locate_next_toggle_us)
    {
        s_locate_led_on ^= 1u;
        s_locate_next_toggle_us = now_us + leap_locate_toggle_us(s_locate_pattern);
        leap_hw_set_locate_led(s_locate_led_on);
    }
}

static void leap_handle_result(const uint8_t *src_mac,
                               LeapDeviceStackStatus status,
                               const LeapDeviceStackResult *result)
{
    if (status == LEAP_DEVICE_STACK_OK)
    {
        leap_device_stack_apply_safe_on_flags(
            result->flags,
            leap_host_enter_safe_cb,
            &s_io);

        if (result->service_id == LEAP_SERVICE_DISC &&
            result->frame.header.message_type == LEAP_DISC_LOCATE_DEVICE &&
            result->frame.payload_length >= sizeof(LeapLocateDeviceRequest))
        {
            const LeapLocateDeviceRequest *req =
                (const LeapLocateDeviceRequest *)result->frame.payload;

            if ((req->flags & LEAP_LOCATE_FLAG_CANCEL) != 0u)
            {
                leap_locate_start(0u, 0u, 1);
            }
            else
            {
                uint16_t accepted_ms =
                    leap_disc_clamp_locate_duration_ms(req->duration_ms);

                leap_locate_start((uint32_t)accepted_ms * 1000u, req->pattern, 0);
            }
        }

        if (result->service_id == (uint16_t)LEAP_SERVICE_DISC &&
            (result->flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u)
        {
            leap_send_reply(src_mac, result,
                            (uint16_t)LEAP_SERVICE_DISC,
                            result->disc_message_type,
                            result->disc_payload,
                            result->disc_payload_length);
        }
        else if (result->service_id == (uint16_t)LEAP_SERVICE_MGMT &&
                 (result->flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u)
        {
            leap_send_reply(src_mac, result,
                            (uint16_t)LEAP_SERVICE_MGMT,
                            result->mgmt_reply.message_type,
                            result->mgmt_reply.payload,
                            result->mgmt_reply.payload_length);
        }
        else if (result->service_id == (uint16_t)LEAP_SERVICE_DIR &&
                 (result->flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u)
        {
            leap_send_reply(src_mac, result,
                            (uint16_t)LEAP_SERVICE_DIR,
                            result->dir_message_type,
                            result->dir_payload,
                            result->dir_payload_length);
        }
        else if (result->service_id == (uint16_t)LEAP_SERVICE_PD &&
                 ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u ||
                  result->pd_reply_payload_length > 0u))
        {
            leap_send_reply(src_mac, result,
                            (uint16_t)LEAP_SERVICE_PD,
                            result->pd_reply_message_type,
                            result->pd_reply_payload,
                            result->pd_reply_payload_length);
        }
        else if (result->service_id == (uint16_t)LEAP_SERVICE_DIAG &&
                 (result->flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u)
        {
            leap_send_reply(src_mac, result,
                            (uint16_t)LEAP_SERVICE_DIAG,
                            result->diag_message_type,
                            result->diag_payload,
                            result->diag_payload_length);
        }
    }
    else if (status == LEAP_DEVICE_STACK_MGMT_ERROR)
    {
        leap_send_error_reply(src_mac, result,
                              (uint16_t)LEAP_SERVICE_MGMT,
                              result->frame.header.message_type,
                              result->error_code);
    }
    else if (status == LEAP_DEVICE_STACK_DISC_ERROR)
    {
        leap_send_error_reply(src_mac, result,
                              (uint16_t)LEAP_SERVICE_DISC,
                              result->frame.header.message_type,
                              result->error_code);
    }
    else if (status == LEAP_DEVICE_STACK_PD_REJECTED)
    {
        leap_send_error_reply(src_mac, result,
                              (uint16_t)LEAP_SERVICE_PD,
                              result->frame.header.message_type,
                              result->error_code);
    }
    else if (status == LEAP_DEVICE_STACK_DIR_ERROR)
    {
        leap_send_error_reply(src_mac, result,
                              (uint16_t)LEAP_SERVICE_DIR,
                              result->frame.header.message_type,
                              result->error_code);
    }
    else if (status == LEAP_DEVICE_STACK_DIAG_ERROR)
    {
        leap_send_error_reply(src_mac, result,
                              (uint16_t)LEAP_SERVICE_DIAG,
                              result->frame.header.message_type,
                              result->error_code);
    }
}

static int leap_item_needs_input_refresh(const LeapHostRxItem *item)
{
    uint16_t service_id;

    if (item == NULL)
    {
        return 0;
    }

    if (leap_device_frame_peek_service_id(
            item->payload, item->payload_length, &service_id) != 0)
    {
        return 0;
    }

    return (service_id == (uint16_t)LEAP_SERVICE_PD) ? 1 : 0;
}

static void leap_process_item(const LeapHostRxItem *item)
{
    LeapDeviceStackStatus status;
    uint64_t              now_us;

    if (item == NULL || s_netif == NULL || item->payload_length == 0u)
    {
        return;
    }

    if (leap_item_needs_input_refresh(item) != 0)
    {
        leap_hw_refresh_inputs(&s_io);
    }

    now_us = leap_hw_monotonic_us();
    status = leap_device_stack_process_frame(
        &s_stack, item->src_mac, now_us,
        item->payload, item->payload_length, &s_process_result);

    leap_handle_result(item->src_mac, status, &s_process_result);
    ++s_stats.rx_ok;
}

int leap_host_init(struct netif *netif)
{
    LeapDeviceStackConfig stack_config;

    if (netif == NULL)
    {
        return -1;
    }

    s_netif = netif;
    memset(&s_stats, 0, sizeof(s_stats));

    s_rx_queue = xQueueCreateStatic(
        LEAP_HOST_RX_DEPTH,
        sizeof(LeapHostRxItem),
        s_rx_queue_storage,
        &s_rx_queue_struct);
    if (s_rx_queue == NULL)
    {
        return -1;
    }

    memset(&s_io, 0, sizeof(s_io));
    s_io.io_status   = LEAP_DIO_STATUS_OK;
    s_io.safe_active = 1u;
    leap_hw_init();

    memset(&stack_config, 0, sizeof(stack_config));
    stack_config.mgmt.default_lease_us    = 5000000u;
    stack_config.mgmt.default_watchdog_us = 5000000u;
    stack_config.mgmt.max_lease_us        = 10000000u;
    stack_config.mgmt.max_watchdog_us     = 10000000u;
    (void)leap_dir_device_config_set_digital_io(
        &stack_config.dir,
        LEAP_STM746_PROFILE_ID,
        (uint16_t)LEAP_STM746_DO_COUNT,
        (uint16_t)LEAP_STM746_DI_COUNT);

    leap_device_stack_init_full(&s_stack, &stack_config);

    memset(&s_pd_io, 0, sizeof(s_pd_io));
    s_pd_io.digital_outputs   = &s_io.digital_outputs;
    s_pd_io.digital_inputs    = &s_io.digital_inputs;
    s_pd_io.io_status         = &s_io.io_status;
    s_pd_io.apply_outputs     = leap_pd_apply_outputs;
    s_pd_io.apply_outputs_ctx = NULL;
    leap_device_stack_bind_pd_io(&s_stack, &s_pd_io);

    leap_host_sync_identity(netif);

    s_locate_until_us       = 0u;
    s_locate_next_toggle_us = 0u;
    s_locate_led_on         = 0u;
    s_locate_pattern        = LEAP_LOCATE_PATTERN_DEFAULT;
    s_locate_solid          = 0u;
    s_host_ready            = 1u;

    if (netif_is_link_up(netif) != 0)
    {
        leap_mgmt_device_on_transport_ready(&s_stack.mgmt);
    }

    return 0;
}

void leap_host_on_link_up(struct netif *netif)
{
    if (netif == NULL || s_host_ready == 0u)
    {
        return;
    }

    if (s_netif == NULL)
    {
        s_netif = netif;
    }

    leap_host_sync_identity(netif);
    leap_mgmt_device_on_transport_ready(&s_stack.mgmt);
}

int leap_host_queue_frame(struct netif *netif, const uint8_t *src_mac,
                          const uint8_t *payload, size_t payload_length)
{
    LeapHostRxItem item;
    uint16_t       service_id;
    int            priority_control = 0;

    if (netif == NULL || src_mac == NULL || payload == NULL ||
        payload_length == 0u || payload_length > LEAP_HOST_MAX_FRAME ||
        s_rx_queue == NULL)
    {
        ++s_stats.rx_drop;
        return -1;
    }

    memcpy(item.src_mac, src_mac, 6);
    memcpy(item.payload, payload, payload_length);
    item.payload_length = (uint16_t)payload_length;

    if (leap_device_frame_peek_service_id(payload, payload_length, &service_id) == 0)
    {
        priority_control =
            (service_id == (uint16_t)LEAP_SERVICE_MGMT ||
             service_id == (uint16_t)LEAP_SERVICE_DISC) ? 1 : 0;
    }

    if (priority_control != 0)
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
        ++s_stats.rx_drop;
        return -1;
    }

    ++s_stats.rx_queued;

    if (s_netif == NULL)
    {
        s_netif = netif;
    }

    if (s_leap_task_notify != NULL)
    {
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
    if (s_rx_queue == NULL)
    {
        return 0;
    }

    return (uxQueueMessagesWaiting(s_rx_queue) > 0u) ? 1 : 0;
}

void leap_host_cyclic(void)
{
    uint32_t tick_flags = 0u;
    uint64_t now_us;

    LeapHostRxItem item;

    if (s_netif == NULL || s_rx_queue == NULL)
    {
        return;
    }

    if (xQueueReceive(s_rx_queue, &item, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        leap_process_item(&item);

        while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE)
        {
            leap_process_item(&item);
        }
    }

    now_us = leap_hw_monotonic_us();
    (void)leap_device_stack_tick(&s_stack, now_us, &tick_flags);

    leap_device_stack_apply_safe_on_flags(
        tick_flags,
        leap_host_enter_safe_cb,
        &s_io);

    leap_update_locate_led(now_us);
}

const LeapHostStats *leap_host_stats(void)
{
    return &s_stats;
}
