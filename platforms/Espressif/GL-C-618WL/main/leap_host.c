// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "leap_host.h"
#include "board_config.h"
#include "leap_eth.h"
#include "leap_hw.h"

#include "leap/leap_build_info.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/netif.h"

#include <string.h>

static const char *TAG = "leap_host";

#define LEAP_GL_PRODUCT_CODE      0x0618C618u
#define LEAP_GL_FIRMWARE_REVISION 1u

typedef struct LeapHostRxItem
{
    uint8_t src_mac[6];
    uint16_t payload_length;
    uint8_t payload[LEAP_HOST_MAX_FRAME];
} LeapHostRxItem;

static LeapDeviceStack       s_stack;
static LeapGlc618wlIoShadow  s_io;
static LeapPdDeviceIoBinding s_pd_io;
static struct netif         *s_netif;
static QueueHandle_t         s_rx_queue;
static LeapHostStats         s_stats;
static uint64_t              s_locate_until_us;
static uint64_t              s_locate_next_toggle_us;
static uint8_t               s_locate_led_on;
static uint8_t               s_locate_pattern;
static uint8_t               s_locate_solid;
static uint8_t               s_tx_frame[LEAP_HOST_MAX_FRAME];
static LeapDeviceStackResult s_process_result;

static uint64_t leap_monotonic_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

static const char *leap_state_name(uint16_t state)
{
    switch (state) {
    case LEAP_STATE_BOOT:        return "BOOT";
    case LEAP_STATE_INIT:        return "INIT";
    case LEAP_STATE_CONFIGURED:  return "CONFIGURED";
    case LEAP_STATE_SAFE:        return "SAFE";
    case LEAP_STATE_OP:          return "OP";
    case LEAP_STATE_FAULT:       return "FAULT";
    default:                     return "unknown";
    }
}

static const char *leap_disc_message_name(uint16_t message_type)
{
    switch (message_type) {
    case LEAP_DISC_HELLO:              return "HELLO";
    case LEAP_DISC_HELLO_REPLY:        return "HELLO_REPLY";
    case LEAP_DISC_IDENTIFY:           return "IDENTIFY";
    case LEAP_DISC_IDENTIFY_REPLY:     return "IDENTIFY_REPLY";
    case LEAP_DISC_LOCATE_DEVICE:      return "LOCATE_DEVICE";
    case LEAP_DISC_LOCATE_DEVICE_REPLY: return "LOCATE_DEVICE_REPLY";
    default:                           return "DISC";
    }
}

static const char *leap_locate_pattern_name(uint8_t pattern)
{
    switch (pattern) {
    case LEAP_LOCATE_PATTERN_DEFAULT:      return "default";
    case LEAP_LOCATE_PATTERN_SLOW_BLINK:   return "slow-blink";
    case LEAP_LOCATE_PATTERN_FAST_BLINK:   return "fast-blink";
    case LEAP_LOCATE_PATTERN_DOUBLE_BLINK: return "double-blink";
    case LEAP_LOCATE_PATTERN_SOLID:        return "solid";
    default:                               return "custom";
    }
}

static void leap_log_mac(const char *label, const uint8_t *mac)
{
    if (mac == NULL) {
        return;
    }

    ESP_LOGI(TAG, "%s%02x:%02x:%02x:%02x:%02x:%02x",
             label,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void leap_log_identity(const LeapIdentity *identity)
{
    if (identity == NULL) {
        return;
    }

    ESP_LOGI(TAG, "identity:");
    leap_log_mac("  primary_mac=", identity->primary_mac);
    ESP_LOGI(TAG,
             "  vendor_id=0x%04X product_code=0x%08X serial=0x%08X",
             (unsigned)identity->vendor_id,
             (unsigned)identity->product_code,
             (unsigned)identity->serial_number);
    ESP_LOGI(TAG,
             "  hw_rev=%u fw_rev=%u caps=0x%08X",
             (unsigned)identity->hardware_revision,
             (unsigned)identity->firmware_revision,
             (unsigned)identity->device_capability_flags);
}

static void leap_log_session_timers(uint64_t now_us)
{
    LeapState_u16 state = leap_mgmt_device_get_state(&s_stack.mgmt);
    uint64_t      lease_ms = 0u;
    uint64_t      watchdog_ms = 0u;

    if (state != LEAP_STATE_OP) {
        return;
    }

    if (s_stack.mgmt.lease_deadline_us > now_us) {
        lease_ms = (s_stack.mgmt.lease_deadline_us - now_us) / 1000u;
    }
    if (s_stack.mgmt.watchdog_deadline_us > now_us) {
        watchdog_ms = (s_stack.mgmt.watchdog_deadline_us - now_us) / 1000u;
    }

    ESP_LOGI(TAG,
             "  session OP: lease_expires_in=%u ms watchdog_expires_in=%u ms",
             (unsigned)lease_ms, (unsigned)watchdog_ms);
}

static void leap_log_device_config(void)
{
    const LeapIdentity *identity = &s_stack.dir.config.identity;

    ESP_LOGI(TAG,
             "LEAP GL-C-618WL protocol %u.%u git=%s built=%s fw_rev=%u",
             (unsigned)LEAP_VERSION_MAJOR,
             (unsigned)LEAP_VERSION_MINOR,
             LEAP_BUILD_GIT,
             LEAP_BUILD_DATE,
             (unsigned)LEAP_GL_FIRMWARE_REVISION);
    leap_log_identity(identity);
    ESP_LOGI(TAG,
             "  default_profile=0x%08X active_profile=0x%08X state=%s",
             (unsigned)s_stack.dir.config.default_profile_id,
             (unsigned)s_stack.dir.config.active_profile_id,
             leap_state_name((uint16_t)leap_mgmt_device_get_state(&s_stack.mgmt)));
    ESP_LOGI(TAG,
             "  locate_caps=0x%04X (GPIO%d status LED)",
             (unsigned)LEAP_LOCATE_FLAG_LED,
             STATUS_LED_PIN);
    ESP_LOGI(TAG,
             "  strip CH0 GPIO%d CH1 GPIO%d x%u LEDs  PD max=0x%02X",
             LED_DATA_PIN_0, LED_DATA_PIN_1,
             (unsigned)DEFAULT_NUM_LEDS,
             (unsigned)LEAP_LED_MAX_LEVEL);
}

static void leap_log_disc_reply(const uint8_t *peer_mac, uint16_t reply_type,
                                const uint8_t *payload, size_t payload_length)
{
    const char *reply_name = leap_disc_message_name(reply_type);

    if (payload == NULL) {
        return;
    }

    if (reply_type == LEAP_DISC_HELLO_REPLY &&
        payload_length >= sizeof(LeapHelloReply)) {
        const LeapHelloReply *reply = (const LeapHelloReply *)payload;

        ESP_LOGD(TAG,
                 "DISC HELLO_REPLY state=%s profile=0x%08X locate_caps=0x%04X",
                 leap_state_name(reply->current_state),
                 (unsigned)reply->active_profile_id,
                 (unsigned)reply->locate_capability_flags);
        (void)peer_mac;
        return;
    }

    if (reply_type == LEAP_DISC_IDENTIFY_REPLY &&
        payload_length >= sizeof(LeapIdentifyReply)) {
        const LeapIdentifyReply *reply = (const LeapIdentifyReply *)payload;

        leap_log_mac("DISC IDENTIFY_REPLY to ", peer_mac);
        ESP_LOGI(TAG,
                 "  state=%s profile=0x%08X locate_caps=0x%04X",
                 leap_state_name(reply->current_state),
                 (unsigned)reply->active_profile_id,
                 (unsigned)reply->locate_capability_flags);
        leap_log_session_timers(leap_monotonic_us());
        leap_log_identity(&reply->identity);
        return;
    }

    if (reply_type == LEAP_DISC_LOCATE_DEVICE_REPLY &&
        payload_length >= sizeof(LeapLocateDeviceReply)) {
        const LeapLocateDeviceReply *reply =
            (const LeapLocateDeviceReply *)payload;

        ESP_LOGI(TAG,
                 "DISC LOCATE_DEVICE_REPLY to peer supported=%u active=%u "
                 "remaining_ms=%u",
                 (unsigned)reply->supported,
                 (unsigned)reply->active,
                 (unsigned)reply->remaining_ms);
        return;
    }

    ESP_LOGI(TAG, "DISC %s to peer", reply_name);
    leap_log_mac("  peer=", peer_mac);
}

static void leap_log_disc_request(const uint8_t *peer_mac,
                                  const LeapDeviceStackResult *result)
{
    uint16_t message_type = result->frame.header.message_type;

    if (message_type == LEAP_DISC_HELLO) {
        leap_log_mac("DISC HELLO from ", peer_mac);
        return;
    }

    leap_log_mac("DISC ", peer_mac);
    ESP_LOGI(TAG, "  request=%s", leap_disc_message_name(message_type));

    if (message_type == LEAP_DISC_IDENTIFY &&
        result->frame.payload_length >= sizeof(LeapIdentifyRequest)) {
        const LeapIdentifyRequest *req =
            (const LeapIdentifyRequest *)result->frame.payload;

        leap_log_mac("  target_mac=", req->target_mac);
        ESP_LOGI(TAG, "  request_flags=0x%04X", (unsigned)req->request_flags);
    } else if (message_type == LEAP_DISC_LOCATE_DEVICE &&
               result->frame.payload_length >= sizeof(LeapLocateDeviceRequest)) {
        const LeapLocateDeviceRequest *req =
            (const LeapLocateDeviceRequest *)result->frame.payload;

        ESP_LOGI(TAG,
                 "  duration_ms=%u pattern=%u (%s) flags=0x%02X",
                 (unsigned)req->duration_ms,
                 (unsigned)req->pattern,
                 leap_locate_pattern_name(req->pattern),
                 (unsigned)req->flags);
    }
}

static void leap_record_tx_result(int send_ok)
{
    if (send_ok == 0) {
        leap_device_stack_notify_tx_ok(&s_stack, leap_monotonic_us());
        ++s_stats.tx_ok;
    } else {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
    }
}

static void leap_send_error_reply(struct netif *netif, const uint8_t *dst_mac,
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
            (const uint8_t *)&err, sizeof(err)) != 0) {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return;
    }

    leap_record_tx_result(leap_eth_send(netif, dst_mac, s_tx_frame, tx_len));
}

static void leap_send_reply(struct netif *netif, const uint8_t *dst_mac,
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
            payload, payload_length) != 0) {
        leap_device_stack_notify_tx_drop(&s_stack);
        ++s_stats.tx_drop;
        return;
    }

    leap_record_tx_result(leap_eth_send(netif, dst_mac, s_tx_frame, tx_len));
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
        leap_hw_set_status_led(0);
        ESP_LOGI(TAG, "status LED locate ended (GPIO%d off)", STATUS_LED_PIN);
    }
}

static void leap_locate_start(uint32_t duration_us, uint8_t pattern, int cancel)
{
    uint64_t now_us = leap_monotonic_us();

    if (cancel != 0) {
        ESP_LOGI(TAG, "status LED locate cancel (GPIO%d)", STATUS_LED_PIN);
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
        leap_hw_set_status_led(1);
    }

    ESP_LOGI(TAG,
             "status LED locate GPIO%d %u ms pattern=%u (%s) mode=%s",
             STATUS_LED_PIN,
             (unsigned)(duration_us / 1000u),
             (unsigned)pattern,
             leap_locate_pattern_name(pattern),
             s_locate_solid != 0u ? "solid" : "blink");
}

static void leap_apply_result(const LeapDeviceStackResult *result)
{
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) == 0u) {
        return;
    }

    leap_hw_apply_outputs(&s_io, result->pd_outputs_applied);
}

static void leap_handle_result(struct netif *netif, const uint8_t *src_mac,
                               LeapDeviceStackStatus status,
                               const LeapDeviceStackResult *result)
{
    if (status == LEAP_DEVICE_STACK_OK) {
        if (result->service_id == LEAP_SERVICE_DISC) {
            leap_log_disc_request(src_mac, result);

            if (result->frame.header.message_type == LEAP_DISC_LOCATE_DEVICE &&
                       result->frame.payload_length >=
                           sizeof(LeapLocateDeviceRequest)) {
                const LeapLocateDeviceRequest *req =
                    (const LeapLocateDeviceRequest *)result->frame.payload;

                if ((req->flags & LEAP_LOCATE_FLAG_CANCEL) != 0u) {
                    leap_locate_start(0u, 0u, 1);
                } else {
                    uint16_t accepted_ms =
                        leap_disc_clamp_locate_duration_ms(req->duration_ms);

                    leap_locate_start(
                        (uint32_t)accepted_ms * 1000u,
                        req->pattern,
                        0);
                }
            }
        }

        leap_apply_result(result);

        if (result->service_id == (uint16_t)LEAP_SERVICE_DISC &&
            (result->flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u) {
            leap_log_disc_reply(src_mac, result->disc_message_type,
                                result->disc_payload,
                                result->disc_payload_length);
            leap_send_reply(netif, src_mac, result,
                            (uint16_t)LEAP_SERVICE_DISC,
                            result->disc_message_type,
                            result->disc_payload,
                            result->disc_payload_length);
        } else if (result->service_id == (uint16_t)LEAP_SERVICE_MGMT &&
                   (result->flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u) {
            if ((result->flags & LEAP_DEVICE_STACK_FLAG_OWNERSHIP_CHANGED) != 0u) {
                leap_log_mac("MGMT session owner ", s_stack.mgmt.owner_mac);
                ESP_LOGI(TAG, "  session_id=0x%08X lease=%u ms watchdog=%u ms",
                         (unsigned)s_stack.mgmt.owner_session_id,
                         (unsigned)(s_stack.mgmt.granted_lease_us / 1000u),
                         (unsigned)(s_stack.mgmt.granted_watchdog_us / 1000u));
            }

            if ((result->flags & LEAP_DEVICE_STACK_FLAG_STATE_CHANGED) != 0u) {
                ESP_LOGI(TAG, "MGMT state -> %s (0x%04X)",
                         leap_state_name(result->device_state),
                         (unsigned)result->device_state);
            } else if (result->mgmt_reply.message_type == LEAP_MGMT_STATE_REPLY &&
                       result->device_state == (uint16_t)LEAP_STATE_OP) {
                ESP_LOGI(TAG, "MGMT entered OP");
            }

            leap_send_reply(netif, src_mac, result,
                            (uint16_t)LEAP_SERVICE_MGMT,
                            result->mgmt_reply.message_type,
                            result->mgmt_reply.payload,
                            result->mgmt_reply.payload_length);
        } else if (result->service_id == (uint16_t)LEAP_SERVICE_DIR &&
                   (result->flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u) {
            if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIR_PROFILE_SELECTED) != 0u) {
                ESP_LOGI(TAG, "DIR profile selected 0x%08X",
                         (unsigned)s_stack.dir.config.active_profile_id);
            }

            leap_send_reply(netif, src_mac, result,
                            (uint16_t)LEAP_SERVICE_DIR,
                            result->dir_message_type,
                            result->dir_payload,
                            result->dir_payload_length);
        }

        if (result->service_id == (uint16_t)LEAP_SERVICE_PD &&
            ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u ||
             result->pd_reply_payload_length > 0u)) {
            leap_send_reply(netif, src_mac, result,
                            (uint16_t)LEAP_SERVICE_PD,
                            result->pd_reply_message_type,
                            result->pd_reply_payload,
                            result->pd_reply_payload_length);
        } else if (result->service_id == (uint16_t)LEAP_SERVICE_DIAG &&
                   (result->flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u) {
            leap_send_reply(netif, src_mac, result,
                            (uint16_t)LEAP_SERVICE_DIAG,
                            result->diag_message_type,
                            result->diag_payload,
                            result->diag_payload_length);
        }
    } else if (status == LEAP_DEVICE_STACK_PD_REJECTED) {
        leap_send_error_reply(netif, src_mac, result,
                              (uint16_t)LEAP_SERVICE_PD,
                              result->frame.header.message_type,
                              result->error_code);
        ESP_LOGW(TAG, "PD rejected msg=0x%04X status=0x%04X",
                 result->frame.header.message_type, result->error_code);
    } else if (status == LEAP_DEVICE_STACK_DIAG_ERROR) {
        leap_send_error_reply(netif, src_mac, result,
                              (uint16_t)LEAP_SERVICE_DIAG,
                              result->frame.header.message_type,
                              result->error_code);
        ESP_LOGW(TAG, "DIAG error msg=0x%04X status=0x%04X",
                 result->frame.header.message_type, result->error_code);
    } else if (status == LEAP_DEVICE_STACK_DIR_ERROR) {
        leap_send_error_reply(netif, src_mac, result,
                              (uint16_t)LEAP_SERVICE_DIR,
                              result->frame.header.message_type,
                              result->error_code);
        ESP_LOGW(TAG, "DIR error msg=0x%04X status=0x%04X",
                 result->frame.header.message_type, result->error_code);
    } else if (status == LEAP_DEVICE_STACK_MGMT_ERROR &&
               result->service_id == (uint16_t)LEAP_SERVICE_MGMT &&
               result->frame.header.message_type == LEAP_MGMT_OWNER_RELEASE &&
               s_stack.mgmt.owner_active == 0u) {
        ESP_LOGD(TAG, "OWNER_RELEASE ignored (no active owner)");
    } else {
        ESP_LOGW(TAG, "stack status=%d svc=0x%04X msg=0x%04X",
                 (int)status, (unsigned)result->service_id,
                 result->frame.header.message_type);
    }
}

static void leap_process_item(const LeapHostRxItem *item)
{
    LeapDeviceStackStatus status;
    uint64_t              now_us;

    if (item == NULL || s_netif == NULL || item->payload_length == 0u) {
        return;
    }

    leap_hw_refresh_inputs(&s_io);
    now_us = leap_monotonic_us();

    status = leap_device_stack_process_frame(
        &s_stack, item->src_mac, now_us,
        item->payload, item->payload_length, &s_process_result);

    leap_handle_result(s_netif, item->src_mac, status, &s_process_result);
    ++s_stats.rx_ok;
    taskYIELD();
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
            leap_hw_set_status_led(1);
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

int leap_host_init(struct netif *netif, uint16_t num_leds)
{
    LeapDeviceStackConfig stack_config;

    if (netif == NULL) {
        return -1;
    }

    s_netif = netif;
    memset(&s_stats, 0, sizeof(s_stats));

    if (s_rx_queue != NULL) {
        vQueueDelete(s_rx_queue);
        s_rx_queue = NULL;
    }

    s_rx_queue = xQueueCreate(LEAP_HOST_RX_DEPTH, sizeof(LeapHostRxItem));
    if (s_rx_queue == NULL) {
        ESP_LOGE(TAG, "RX queue create failed");
        return -1;
    }

    memset(&s_io, 0, sizeof(s_io));
    s_io.io_status = LEAP_DIO_STATUS_OK;
    s_io.safe_active = 1;
    leap_hw_init(num_leds);

    memset(&stack_config, 0, sizeof(stack_config));
    stack_config.mgmt.default_lease_us    = 5000000u;
    stack_config.mgmt.default_watchdog_us = 5000000u;
    stack_config.mgmt.max_lease_us        = 10000000u;
    stack_config.mgmt.max_watchdog_us     = 10000000u;

    leap_device_stack_init_full(&s_stack, &stack_config);

    memset(&s_pd_io, 0, sizeof(s_pd_io));
    s_pd_io.digital_outputs = &s_io.digital_outputs;
    s_pd_io.digital_inputs  = &s_io.digital_inputs;
    s_pd_io.io_status       = &s_io.io_status;
    leap_device_stack_bind_pd_io(&s_stack, &s_pd_io);

    memcpy(s_stack.dir.config.identity.primary_mac, netif->hwaddr, 6);
    s_stack.dir.config.identity.product_code      = LEAP_GL_PRODUCT_CODE;
    s_stack.dir.config.identity.firmware_revision = LEAP_GL_FIRMWARE_REVISION;
    memcpy(s_stack.disc.config.identity.primary_mac, netif->hwaddr, 6);
    s_stack.disc.config.identity.product_code      = LEAP_GL_PRODUCT_CODE;
    s_stack.disc.config.identity.firmware_revision = LEAP_GL_FIRMWARE_REVISION;
    leap_dir_device_sync_disc(&s_stack.dir, &s_stack.disc);
    leap_mgmt_device_on_transport_ready(&s_stack.mgmt);
    leap_log_device_config();

    s_locate_until_us       = 0u;
    s_locate_next_toggle_us = 0u;
    s_locate_led_on         = 0u;
    s_locate_pattern        = LEAP_LOCATE_PATTERN_DEFAULT;
    s_locate_solid          = 0u;

    ESP_LOGI(TAG, "LEAP host ready — waiting for master");

    return 0;
}

int leap_host_queue_frame(struct netif *netif, const uint8_t *src_mac,
                          const uint8_t *payload, size_t payload_length)
{
    LeapHostRxItem item;

    if (netif == NULL || src_mac == NULL || payload == NULL ||
        payload_length == 0u || payload_length > LEAP_HOST_MAX_FRAME ||
        s_rx_queue == NULL) {
        ++s_stats.rx_drop;
        return -1;
    }

    memcpy(item.src_mac, src_mac, 6);
    memcpy(item.payload, payload, payload_length);
    item.payload_length = (uint16_t)payload_length;

    if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
        ++s_stats.rx_drop;
        return -1;
    }

    ++s_stats.rx_queued;

    if (s_netif == NULL) {
        s_netif = netif;
    }

    return 0;
}

void leap_host_cyclic(void)
{
    uint32_t tick_flags = 0u;
    uint64_t now_us;

    if (s_netif == NULL) {
        return;
    }

    for (;;) {
        LeapHostRxItem item;

        if (xQueueReceive(s_rx_queue, &item, 0) != pdTRUE) {
            break;
        }

        leap_process_item(&item);
    }

    now_us = leap_monotonic_us();
    (void)leap_device_stack_tick(&s_stack, now_us, &tick_flags);

    if ((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u) {
        ESP_LOGW(TAG, "lease/watchdog expired -> SAFE");
        leap_hw_enter_safe(&s_io);
    }

    leap_update_locate_led(now_us);
}

const LeapHostStats *leap_host_stats(void)
{
    return &s_stats;
}
