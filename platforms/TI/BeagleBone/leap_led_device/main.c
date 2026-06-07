#include "bbb_cpsw_raw.h"
#include "bbb_hw.h"
#include "bbb_log.h"

#include "leap/leap_device_host_perf.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_diag_device.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_crc.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_pd_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define RX_BUF_SIZE 1536u
#define TX_BUF_SIZE 1536u
#define BBB_RX_DRAIN_LIMIT 4u

static uint8_t g_rx[RX_BUF_SIZE] __attribute__((aligned(8)));
static uint8_t g_tx[TX_BUF_SIZE] __attribute__((aligned(8)));

static uint16_t g_digital_outputs;
static uint16_t g_digital_inputs;
static uint16_t g_io_status;
static int      g_outputs_dirty;

static uint16_t      g_uart_last_outputs = 0xFFFFu;
static int           g_uart_last_state   = -1;
static int           g_uart_profile_logged;
static int           g_uart_in_safe;
static uint64_t      g_locate_until_us;
static uint64_t      g_locate_next_toggle_us;
static int           g_locate_led_on;
static uint8_t       g_locate_pattern;
static int           g_locate_solid;

static LeapDeviceStack       g_stack;
static LeapDeviceStackConfig g_stack_config;
static LeapDeviceStackResult g_stack_result;

static const char* bbb_stack_status_string(LeapDeviceStackStatus status)
{
    switch (status) {
    case LEAP_DEVICE_STACK_OK:
        return "ok";
    case LEAP_DEVICE_STACK_FRAME_ERROR:
        return "frame";
    case LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE:
        return "svc";
    case LEAP_DEVICE_STACK_MGMT_ERROR:
        return "mgmt";
    case LEAP_DEVICE_STACK_PD_REJECTED:
        return "pd";
    case LEAP_DEVICE_STACK_DISC_ERROR:
        return "disc";
    case LEAP_DEVICE_STACK_DIR_ERROR:
        return "dir";
    case LEAP_DEVICE_STACK_DIAG_ERROR:
        return "diag";
    default:
        return "?";
    }
}

static void bbb_log_stack_error(LeapDeviceStackStatus status)
{
    static uint32_t s_log_count;
    static uint16_t s_last_ec;
    static uint16_t s_last_st;

    if (s_log_count < 8u ||
        g_stack_result.error_code != s_last_ec ||
        (uint16_t)g_stack.mgmt.device_state != s_last_st) {
        bbb_uart_puts("LEAP WRN: ");
        bbb_uart_puts(bbb_stack_status_string(status));
        bbb_uart_puts(" ec=0x");
        bbb_uart_put_hex16(g_stack_result.error_code);
        bbb_uart_puts(" st=0x");
        bbb_uart_put_hex16((uint16_t)g_stack.mgmt.device_state);
        bbb_uart_puts("\n");
        s_last_ec = g_stack_result.error_code;
        s_last_st = (uint16_t)g_stack.mgmt.device_state;
    }

    s_log_count++;
}

static uint32_t bbb_locate_toggle_us(uint8_t pattern)
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

static void bbb_locate_stop(uint64_t now_us)
{
    g_locate_until_us       = now_us;
    g_locate_next_toggle_us = 0u;
    g_locate_solid          = 0;
    g_locate_led_on         = 0;
}

static void bbb_locate_start(uint32_t duration_us, uint8_t pattern, int cancel)
{
    uint64_t now_us = bbb_monotonic_us();

    if (cancel != 0) {
        bbb_locate_stop(now_us);
        return;
    }

    if (duration_us == 0u) {
        duration_us = (uint32_t)LEAP_LOCATE_DURATION_DEFAULT_MS * 1000u;
    }

    g_locate_until_us       = now_us + (uint64_t)duration_us;
    g_locate_next_toggle_us = 0u;
    g_locate_pattern        = pattern;
    g_locate_solid          =
        (pattern == LEAP_LOCATE_PATTERN_SOLID) ? 1 : 0;
    g_locate_led_on         = g_locate_solid;
}

static void bbb_handle_disc_locate(const LeapDeviceStackResult* result)
{
    if (result == NULL ||
        result->service_id != (LeapServiceId_u16)LEAP_SERVICE_DISC) {
        return;
    }

    if (result->frame.header.message_type == LEAP_DISC_IDENTIFY) {
        bbb_locate_start(5000000u, LEAP_LOCATE_PATTERN_DEFAULT, 0);
        return;
    }

    if (result->frame.header.message_type != LEAP_DISC_LOCATE_DEVICE ||
        result->frame.payload_length < sizeof(LeapLocateDeviceRequest)) {
        return;
    }

    {
        const LeapLocateDeviceRequest* req =
            (const LeapLocateDeviceRequest*)result->frame.payload;

        if ((req->flags & LEAP_LOCATE_FLAG_CANCEL) != 0u) {
            bbb_locate_start(0u, 0u, 1);
        } else {
            uint16_t accepted_ms =
                leap_disc_clamp_locate_duration_ms(req->duration_ms);

            bbb_locate_start(
                (uint32_t)accepted_ms * 1000u,
                req->pattern,
                0);
        }
    }
}

#if LEAP_DEVICE_HOST_TRACE_ENABLE
static void print_mac(const uint8_t* mac)
{
    unsigned i;

    for (i = 0u; i < 6u; i++) {
        if (i != 0u) {
            bbb_uart_putc(':');
        }
        bbb_uart_put_hex8(mac[i]);
    }
}
#endif

static int send_reply(
    BbbCpswRaw*                  net,
    const uint8_t*               dst_mac,
    const LeapDeviceStackResult* result,
    uint16_t                     service_id,
    uint16_t                     message_type,
    const uint8_t*               payload,
    size_t                       payload_length)
{
    size_t tx_len = 0u;

    if (leap_frame_write(
            g_tx,
            sizeof(g_tx),
            &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            payload,
            payload_length) != 0) {
        leap_device_stack_notify_tx_drop(&g_stack);
        bbb_log_error("reply build failed");
        return -1;
    }

    if (bbb_cpsw_raw_send(net, dst_mac, g_tx, tx_len) != 0) {
        leap_device_stack_notify_tx_drop(&g_stack);
        bbb_log_error("tx stall");
        return -1;
    }

    leap_device_stack_notify_tx_ok(&g_stack, bbb_monotonic_us());
    return 0;
}

static int send_error_reply(
    BbbCpswRaw*                  net,
    const uint8_t*               dst_mac,
    const LeapDeviceStackResult* result,
    uint16_t                     service_id,
    uint16_t                     message_type,
    uint16_t                     status_code)
{
    LeapErrorPayload err;
    size_t           tx_len = 0u;

    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

    if (leap_frame_write(
            g_tx,
            sizeof(g_tx),
            &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            (const uint8_t*)&err,
            sizeof(err)) != 0) {
        leap_device_stack_notify_tx_drop(&g_stack);
        bbb_log_error("error reply build failed");
        return -1;
    }

    if (bbb_cpsw_raw_send(net, dst_mac, g_tx, tx_len) != 0) {
        leap_device_stack_notify_tx_drop(&g_stack);
        bbb_log_error("error tx stall");
        return -1;
    }

    leap_device_stack_notify_tx_ok(&g_stack, bbb_monotonic_us());
    return 0;
}

static void apply_outputs(uint16_t outputs, int log_change);
static void send_stack_reply(
    BbbCpswRaw*                  net,
    const uint8_t*               src_mac,
    const LeapDeviceStackResult* result);

static void bbb_stack_tick(uint64_t now_us)
{
    uint32_t tick_flags = 0u;

    if (leap_device_stack_tick(&g_stack, now_us, &tick_flags) ==
        LEAP_DEVICE_STACK_OK) {
        if ((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u) {
            if (g_uart_in_safe == 0) {
                g_uart_in_safe = 1;
                bbb_log_info("SAFE (tick)");
            }
            apply_outputs(0u, 0);
        }
    }
}

static int bbb_send_pd_error_from_view(
    BbbCpswRaw*           net,
    const uint8_t*        dst_mac,
    const LeapFrameView*  view,
    uint16_t              message_type,
    uint16_t              status_code)
{
    LeapDeviceStackResult err_result;

    if (net == 0 || dst_mac == 0 || view == 0) {
        return -1;
    }

    memset(&err_result, 0, sizeof(err_result));
    err_result.frame.header = view->header;
    return send_error_reply(
        net,
        dst_mac,
        &err_result,
        (uint16_t)LEAP_SERVICE_PD,
        message_type,
        status_code);
}

static int bbb_try_pd_exchange_fast(
    BbbCpswRaw*    net,
    const uint8_t* src_mac,
    const uint8_t* payload,
    size_t         payload_length,
    uint64_t       now_us)
{
    LeapFrameView      view;
    LeapPdDeviceResult pd_result;
    LeapPdDeviceStatus pd_status;

    if (net == 0 || src_mac == 0 || payload == 0) {
        return 0;
    }

    if (leap_frame_parse(payload, payload_length, &view) != LEAP_FRAME_OK) {
        return 0;
    }

    if (view.header.service_id != (uint16_t)LEAP_SERVICE_PD ||
        view.header.message_type != LEAP_PD_EXCHANGE_ENDPOINTS) {
        return 0;
    }

    g_digital_inputs = bbb_gpio_inputs_read();

    memset(&pd_result, 0, sizeof(pd_result));
    pd_status = leap_pd_device_process_parsed_frame(
        &g_stack.mgmt,
        &g_stack.pd,
        g_stack.pd_io_bound != 0 ? &g_stack.pd_io : NULL,
        src_mac,
        now_us,
        &view,
        &pd_result);

    if (pd_status == LEAP_PD_DEVICE_OK && pd_result.reply_payload_length > 0u) {
        LeapDeviceStackResult reply_result;

        leap_device_stack_note_frame_rx(
            &g_stack,
            now_us,
            (uint16_t)LEAP_SERVICE_PD);

        memset(&reply_result, 0, sizeof(reply_result));
        reply_result.frame = pd_result.frame;

        if (send_reply(
                net,
                src_mac,
                &reply_result,
                (uint16_t)LEAP_SERVICE_PD,
                pd_result.reply_message_type,
                pd_result.reply_payload,
                pd_result.reply_payload_length) != 0) {
            (void)bbb_send_pd_error_from_view(
                net,
                src_mac,
                &view,
                view.header.message_type,
                (uint16_t)LEAP_STATUS_BUSY);
        }

        if ((pd_result.flags & LEAP_PD_DEVICE_FLAG_OUTPUTS_APPLIED) != 0u) {
            apply_outputs(pd_result.digital_outputs_applied, 1);
        }

        leap_diag_device_on_pd_result(&g_stack.diag, &pd_result, now_us);
        return 1;
    }

    if (pd_status == LEAP_PD_DEVICE_IGNORED_RESPONSE &&
        pd_result.error_code == (uint16_t)LEAP_STATUS_NOT_OWNER) {
        (void)bbb_send_pd_error_from_view(
            net,
            src_mac,
            &view,
            view.header.message_type,
            pd_result.error_code);
        return 1;
    }

    if (pd_status == LEAP_PD_DEVICE_REJECTED) {
        leap_diag_device_on_pd_result(&g_stack.diag, &pd_result, now_us);
        memset(&g_stack_result, 0, sizeof(g_stack_result));
        g_stack_result.frame = pd_result.frame;
        (void)send_error_reply(
            net,
            src_mac,
            &g_stack_result,
            (uint16_t)LEAP_SERVICE_PD,
            pd_result.frame.header.message_type,
            pd_result.error_code);
        return 1;
    }

    return 0;
}

static void bbb_process_one_frame(
    BbbCpswRaw*    net,
    const uint8_t* src_mac,
    const uint8_t* payload,
    size_t         payload_length)
{
    LeapDeviceStackStatus status;
    uint16_t              service_id;
    uint64_t              now_us = bbb_monotonic_us();

    bbb_cpsw_raw_note_peer(src_mac);

    if (bbb_try_pd_exchange_fast(
            net, src_mac, payload, payload_length, now_us) != 0) {
        bbb_stack_tick(now_us);
        return;
    }

    if (leap_device_frame_peek_service_id(payload, payload_length, &service_id) == 0 &&
        service_id == (uint16_t)LEAP_SERVICE_PD) {
        g_digital_inputs = bbb_gpio_inputs_read();
    }

    status = leap_device_stack_process_frame(
        &g_stack,
        src_mac,
        now_us,
        payload,
        payload_length,
        &g_stack_result);

    if (status == LEAP_DEVICE_STACK_OK) {
        bbb_handle_disc_locate(&g_stack_result);
        send_stack_reply(net, src_mac, &g_stack_result);

        if ((g_stack_result.flags &
             LEAP_DEVICE_STACK_FLAG_DIR_PROFILE_SELECTED) != 0u &&
            g_uart_profile_logged == 0) {
            g_uart_profile_logged = 1;
            bbb_log_info("profile ok");
        }
        if ((g_stack_result.flags &
             LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u) {
            if (g_uart_in_safe == 0) {
                g_uart_in_safe = 1;
                bbb_log_info("SAFE");
            }
            apply_outputs(0u, 0);
        }
        if ((g_stack_result.flags &
             LEAP_DEVICE_STACK_FLAG_STATE_CHANGED) != 0u) {
            LeapState_u16 st = g_stack_result.device_state;

            if ((int)st != g_uart_last_state) {
                g_uart_last_state = (int)st;
                if (st == LEAP_STATE_OP) {
                    g_uart_in_safe = 0;
                }
#if LEAP_DEVICE_HOST_TRACE_ENABLE
                bbb_uart_puts("state=0x");
                bbb_uart_put_hex16((uint16_t)st);
                bbb_uart_puts("\n");
#endif
            }
        }
        if ((g_stack_result.flags &
             LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) != 0u) {
            apply_outputs(g_stack_result.pd_outputs_applied, 1);
        }
    } else if (status == LEAP_DEVICE_STACK_PD_REJECTED) {
        (void)send_error_reply(
            net,
            src_mac,
            &g_stack_result,
            (uint16_t)LEAP_SERVICE_PD,
            g_stack_result.frame.header.message_type,
            g_stack_result.error_code);
        bbb_log_stack_error(status);
    } else if (status == LEAP_DEVICE_STACK_DIR_ERROR) {
        (void)send_error_reply(
            net,
            src_mac,
            &g_stack_result,
            (uint16_t)LEAP_SERVICE_DIR,
            g_stack_result.frame.header.message_type,
            g_stack_result.error_code);
        bbb_log_stack_error(status);
    } else if (status == LEAP_DEVICE_STACK_DIAG_ERROR) {
        (void)send_error_reply(
            net,
            src_mac,
            &g_stack_result,
            (uint16_t)LEAP_SERVICE_DIAG,
            g_stack_result.frame.header.message_type,
            g_stack_result.error_code);
        bbb_log_stack_error(status);
    } else if (status == LEAP_DEVICE_STACK_DISC_ERROR) {
        (void)send_error_reply(
            net,
            src_mac,
            &g_stack_result,
            (uint16_t)LEAP_SERVICE_DISC,
            g_stack_result.frame.header.message_type,
            g_stack_result.error_code);
        bbb_log_stack_error(status);
    } else if (status == LEAP_DEVICE_STACK_MGMT_ERROR) {
        if (g_stack_result.error_code != 0u &&
            g_stack_result.error_code != (uint16_t)LEAP_STATUS_OK) {
            (void)send_error_reply(
                net,
                src_mac,
                &g_stack_result,
                (uint16_t)LEAP_SERVICE_MGMT,
                g_stack_result.frame.header.message_type,
                g_stack_result.error_code);
        }
        bbb_log_stack_error(status);
    } else if (status == LEAP_DEVICE_STACK_FRAME_ERROR) {
        bbb_log_error("frame parse failed");
    } else {
        bbb_log_stack_error(status);
    }

    bbb_stack_tick(now_us);
}

static void send_stack_reply(
    BbbCpswRaw*                  net,
    const uint8_t*               src_mac,
    const LeapDeviceStackResult* result)
{
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_DISC,
            result->disc_message_type,
            result->disc_payload,
            result->disc_payload_length);
    }
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_DIR,
            result->dir_message_type,
            result->dir_payload,
            result->dir_payload_length);
    }
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_MGMT,
            result->mgmt_reply.message_type,
            result->mgmt_reply.payload,
            result->mgmt_reply.payload_length);
    }
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u ||
        result->pd_reply_payload_length > 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_PD,
            result->pd_reply_message_type,
            result->pd_reply_payload,
            result->pd_reply_payload_length);
    } else if (result->service_id == (LeapServiceId_u16)LEAP_SERVICE_PD &&
               result->frame.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS &&
               result->error_code != 0u &&
               result->error_code != (uint16_t)LEAP_STATUS_OK) {
        (void)send_error_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_PD,
            result->frame.header.message_type,
            result->error_code);
    }
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_DIAG,
            result->diag_message_type,
            result->diag_payload,
            result->diag_payload_length);
    }
}

static void apply_outputs(uint16_t outputs, int log_change)
{
    g_digital_outputs = outputs;
    bbb_gpio_outputs_apply(outputs);

    if (log_change == 0) {
        g_uart_last_outputs = outputs;
        return;
    }

    if (outputs != g_uart_last_outputs) {
        g_uart_last_outputs = outputs;
#if LEAP_DEVICE_HOST_TRACE_ENABLE
        bbb_uart_puts("out=0x");
        bbb_uart_put_hex16(outputs);
        bbb_uart_puts("\n");
#endif
    }
}

static void update_status_leds(BbbCpswRaw* net, uint64_t now_us)
{
    uint8_t leds = 0u;
    int connected = (g_stack.mgmt.owner_active != 0u) ? 1 : 0;
    int op_state = (g_stack.mgmt.device_state == LEAP_STATE_OP) ? 1 : 0;
    int safe_state = (g_uart_in_safe != 0) ? 1 : 0;

    /* USR0: locate / identify blink */
    if (g_locate_until_us != 0u) {
        if (now_us >= g_locate_until_us) {
            bbb_locate_stop(now_us);
            g_locate_until_us = 0u;
        } else if (g_locate_solid != 0) {
            g_locate_led_on = 1;
        } else if (g_locate_next_toggle_us == 0u ||
                   now_us >= g_locate_next_toggle_us) {
            g_locate_led_on = (g_locate_led_on == 0) ? 1 : 0;
            g_locate_next_toggle_us =
                now_us + bbb_locate_toggle_us(g_locate_pattern);
        }
    }
    if (g_locate_led_on != 0) {
        leds |= 0x01u;
    }

    /* USR1: connected/owned, USR2: operational, USR3: safe/fault */
    if (connected != 0) {
        leds |= 0x02u;
    }
    if (op_state != 0) {
        leds |= 0x04u;
    }
    if (safe_state != 0 || net->link_up == 0u) {
        leds |= 0x08u;
    }

    bbb_status_leds_apply(leds);
}

int main(void)
{
    BbbCpswRaw              net;
    LeapPdDeviceIoBinding   pd_io;
    uint8_t                 src_mac[6];
    size_t                  rx_len = 0u;
    static uint32_t         rx_drop_logged;

    bbb_leds_init();
    bbb_timer_init();
    (void)leap_crc16_xmodem_update(0u, 0, 0u);

    bbb_log_info("\nLEAP BBB LED Device");

    if (bbb_cpsw_raw_init(&net) != 0) {
        bbb_log_error("CPSW init failed");
        while (1) {
            bbb_status_leds_apply(0x08u);
            bbb_delay(1000000u);
            bbb_status_leds_apply(0x00u);
            bbb_delay(1000000u);
        }
    }

#if LEAP_DEVICE_HOST_TRACE_ENABLE
    bbb_uart_puts("MAC ");
    print_mac(net.mac);
    if (net.link_up != 0) {
        bbb_uart_puts(" link=up\n");
    } else {
        bbb_uart_puts(" link=unknown/down\n");
    }
#else
    if (net.link_up == 0) {
        bbb_log_warn("Ethernet link down");
        bbb_uart_puts("LEAP WRN:");
        bbb_cpsw_raw_print_phy_bsr();
        bbb_uart_puts("\n");
    }
#endif

    memset(&g_stack_config, 0, sizeof(g_stack_config));
    g_stack_config.mgmt.default_lease_us    = 5000000u;
    g_stack_config.mgmt.default_watchdog_us = 5000000u;
    g_stack_config.mgmt.max_lease_us        = 10000000u;
    g_stack_config.mgmt.max_watchdog_us     = 10000000u;
    (void)leap_dir_device_config_set_digital_io(
        &g_stack_config.dir,
        BBB_LEAP_PROFILE_ID,
        BBB_LEAP_DO_COUNT,
        BBB_LEAP_DI_COUNT);

    leap_device_stack_init_full(&g_stack, &g_stack_config);
    memcpy(g_stack.dir.config.identity.primary_mac, net.mac, 6u);
    g_stack.dir.config.identity.product_code      = BBB_LEAP_PRODUCT_CODE;
    g_stack.dir.config.identity.firmware_revision = BBB_LEAP_FIRMWARE_REVISION;
    memcpy(g_stack.disc.config.identity.primary_mac, net.mac, 6u);
    g_stack.disc.config.identity.product_code      = BBB_LEAP_PRODUCT_CODE;
    g_stack.disc.config.identity.firmware_revision = BBB_LEAP_FIRMWARE_REVISION;
    leap_dir_device_sync_disc(&g_stack.dir, &g_stack.disc);
    leap_mgmt_device_on_transport_ready(&g_stack.mgmt);

    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.digital_outputs = &g_digital_outputs;
    pd_io.digital_inputs  = &g_digital_inputs;
    pd_io.io_status       = &g_io_status;
    pd_io.outputs_dirty   = &g_outputs_dirty;
    leap_device_stack_bind_pd_io(&g_stack, &pd_io);

    g_io_status = LEAP_DIO_STATUS_OK;
    g_digital_inputs = bbb_gpio_inputs_read();
    g_locate_until_us = 0u;
    g_locate_next_toggle_us = 0u;
    g_locate_led_on = 0;
    g_locate_pattern = LEAP_LOCATE_PATTERN_DEFAULT;
    g_locate_solid = 0;
    bbb_log_info("waiting for LEAP master");

    while (1) {
        unsigned drained = 0u;
        uint32_t rx_drops;

        bbb_cpsw_raw_poll_rx(&net);

        while (drained < BBB_RX_DRAIN_LIMIT &&
               bbb_cpsw_raw_dequeue(
                   &net,
                   src_mac,
                   g_rx,
                   sizeof(g_rx),
                   &rx_len) == 0) {
            bbb_process_one_frame(&net, src_mac, g_rx, rx_len);
            drained++;
        }

        rx_drops = bbb_cpsw_raw_rx_queue_drops();
        if (rx_drops != 0u && rx_drop_logged != rx_drops) {
            rx_drop_logged = rx_drops;
            bbb_log_warn("rx queue drops=");
            bbb_uart_put_hex32(rx_drops);
            bbb_uart_puts("\n");
        }

        if (drained == 0u) {
            bbb_stack_tick(bbb_monotonic_us());
        }

        update_status_leds(&net, bbb_monotonic_us());
    }
}
