#include "bbb_cpsw_raw.h"
#include "bbb_hw.h"

#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_crc.h"
#include "leap/leap_disc_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <string.h>

#define RX_BUF_SIZE 1536u
#define TX_BUF_SIZE 1536u

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

static LeapDeviceStack       g_stack;
static LeapDeviceStackConfig g_stack_config;
static LeapDeviceStackResult g_stack_result;
static LeapDiscDeviceResult  g_disc_result;

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
    bbb_uart_puts(" err=");
    bbb_uart_puts(bbb_stack_status_string(status));
    bbb_uart_puts(" ec=0x");
    bbb_uart_put_hex16(g_stack_result.error_code);
    bbb_uart_puts(" st=0x");
    bbb_uart_put_hex16((uint16_t)g_stack.mgmt.device_state);
    bbb_uart_puts("\n");
}

static int bbb_handle_disc_only(
    BbbCpswRaw*     net,
    const uint8_t*  src_mac,
    const uint8_t*  data,
    size_t          length)
{
    size_t tx_len = 0u;

    if (leap_disc_device_process_frame(
            &g_stack.disc,
            &g_stack.mgmt,
            src_mac,
            data,
            length,
            &g_disc_result) != LEAP_DISC_DEVICE_OK) {
        return -1;
    }

    if (leap_frame_write(
            g_tx,
            sizeof(g_tx),
            &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_DISC,
            g_disc_result.message_type,
            g_disc_result.frame.header.session_id,
            g_disc_result.frame.header.sequence,
            g_disc_result.frame.header.ack_sequence,
            g_disc_result.payload,
            g_disc_result.payload_length) != 0) {
        bbb_uart_puts(" wr-err");
        return -1;
    }

    if (bbb_cpsw_raw_send(net, src_mac, g_tx, tx_len) != 0) {
        bbb_uart_puts("disc tx-stall\n");
        return -1;
    }

    return 0;
}

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
        bbb_uart_puts("reply build failed\n");
        return -1;
    }

    if (bbb_cpsw_raw_send(net, dst_mac, g_tx, tx_len) != 0) {
        leap_device_stack_notify_tx_drop(&g_stack);
        bbb_uart_puts("tx stall\n");
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
        bbb_uart_puts("error reply build failed\n");
        return -1;
    }

    if (bbb_cpsw_raw_send(net, dst_mac, g_tx, tx_len) != 0) {
        leap_device_stack_notify_tx_drop(&g_stack);
        bbb_uart_puts("error tx stall\n");
        return -1;
    }

    leap_device_stack_notify_tx_ok(&g_stack, bbb_monotonic_us());
    return 0;
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
    } else if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_DIR,
            result->dir_message_type,
            result->dir_payload,
            result->dir_payload_length);
    } else if ((result->flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_MGMT,
            result->mgmt_reply.message_type,
            result->mgmt_reply.payload,
            result->mgmt_reply.payload_length);
    } else if ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u) {
        (void)send_reply(
            net,
            src_mac,
            result,
            (uint16_t)LEAP_SERVICE_PD,
            result->pd_reply_message_type,
            result->pd_reply_payload,
            result->pd_reply_payload_length);
    } else if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u) {
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
        bbb_uart_puts("out=0x");
        bbb_uart_put_hex16(outputs);
        bbb_uart_puts("\n");
    }
}

static void update_status_leds(BbbCpswRaw* net, uint64_t now_us)
{
    uint8_t leds = 0u;
    int connected = (g_stack.mgmt.owner_active != 0u) ? 1 : 0;
    int op_state = (g_stack.mgmt.device_state == LEAP_STATE_OP) ? 1 : 0;
    int safe_state = (g_uart_in_safe != 0) ? 1 : 0;

    /* USR0: locate blink */
    if (now_us < g_locate_until_us) {
        if (now_us >= g_locate_next_toggle_us) {
            g_locate_led_on = (g_locate_led_on == 0) ? 1 : 0;
            g_locate_next_toggle_us = now_us + 250000u;
        }
    } else {
        g_locate_led_on = 0;
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
    uint32_t                tick_flags = 0u;

    bbb_leds_init();
    bbb_timer_init();
    (void)leap_crc16_xmodem_update(0u, 0, 0u);

    bbb_uart_puts("\nLEAP BBB LED Device\n");

    if (bbb_cpsw_raw_init(&net) != 0) {
        bbb_uart_puts("CPSW init failed\n");
        while (1) {
            bbb_status_leds_apply(0x08u);
            bbb_delay(1000000u);
            bbb_status_leds_apply(0x00u);
            bbb_delay(1000000u);
        }
    }

    bbb_uart_puts("MAC ");
    print_mac(net.mac);
    if (net.link_up != 0) {
        bbb_uart_puts(" link=up\n");
    } else {
        bbb_uart_puts(" link=unknown/down");
        bbb_cpsw_raw_print_phy_bsr();
        bbb_uart_puts("\n");
    }

    memset(&g_stack_config, 0, sizeof(g_stack_config));
    g_stack_config.mgmt.default_lease_us    = 5000000u;
    g_stack_config.mgmt.default_watchdog_us = 500000u;
    g_stack_config.mgmt.max_lease_us        = 10000000u;
    g_stack_config.mgmt.max_watchdog_us     = 10000000u;

    leap_device_stack_init_full(&g_stack, &g_stack_config);
    memcpy(g_stack.dir.config.identity.primary_mac, net.mac, 6u);
    memcpy(g_stack.disc.config.identity.primary_mac, net.mac, 6u);
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
    bbb_uart_puts("waiting for LEAP master\n");

    while (1) {
        g_digital_inputs = bbb_gpio_inputs_read();

        if (bbb_cpsw_raw_recv(
                &net,
                src_mac,
                g_rx,
                sizeof(g_rx),
                &rx_len) == 0) {
            LeapDeviceStackStatus status;

            LeapFrameView view;

            bbb_cpsw_raw_note_peer(src_mac);

            if (leap_frame_parse(g_rx, rx_len, &view) != LEAP_FRAME_OK) {
                bbb_uart_puts("parse-err\n");
                continue;
            }

            if (view.header.service_id == (uint16_t)LEAP_SERVICE_DISC) {
                if (view.header.message_type == LEAP_DISC_IDENTIFY) {
                    g_locate_until_us = bbb_monotonic_us() + 5000000u;
                    g_locate_next_toggle_us = 0u;
                }
                if (bbb_handle_disc_only(&net, src_mac, g_rx, rx_len) != 0) {
                    bbb_uart_puts("disc-err\n");
                }
                continue;
            }

            status = leap_device_stack_process_frame(
                &g_stack,
                src_mac,
                bbb_monotonic_us(),
                g_rx,
                rx_len,
                &g_stack_result);

            if (status == LEAP_DEVICE_STACK_OK) {
                send_stack_reply(&net, src_mac, &g_stack_result);

                if ((g_stack_result.flags &
                     LEAP_DEVICE_STACK_FLAG_DIR_PROFILE_SELECTED) != 0u &&
                    g_uart_profile_logged == 0) {
                    g_uart_profile_logged = 1;
                    bbb_uart_puts("profile ok\n");
                }
                if ((g_stack_result.flags &
                     LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u) {
                    if (g_uart_in_safe == 0) {
                        g_uart_in_safe = 1;
                        bbb_uart_puts("SAFE\n");
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
                        bbb_uart_puts("state=0x");
                        bbb_uart_put_hex16((uint16_t)st);
                        bbb_uart_puts("\n");
                    }
                }
                if ((g_stack_result.flags &
                     LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) != 0u) {
                    apply_outputs(g_stack_result.pd_outputs_applied, 1);
                }
            } else if (status == LEAP_DEVICE_STACK_PD_REJECTED) {
                (void)send_error_reply(
                    &net,
                    src_mac,
                    &g_stack_result,
                    (uint16_t)LEAP_SERVICE_PD,
                    g_stack_result.frame.header.message_type,
                    g_stack_result.error_code);
                bbb_log_stack_error(status);
            } else if (status == LEAP_DEVICE_STACK_DIR_ERROR) {
                (void)send_error_reply(
                    &net,
                    src_mac,
                    &g_stack_result,
                    (uint16_t)LEAP_SERVICE_DIR,
                    g_stack_result.frame.header.message_type,
                    g_stack_result.error_code);
                bbb_log_stack_error(status);
            } else if (status == LEAP_DEVICE_STACK_DIAG_ERROR) {
                (void)send_error_reply(
                    &net,
                    src_mac,
                    &g_stack_result,
                    (uint16_t)LEAP_SERVICE_DIAG,
                    g_stack_result.frame.header.message_type,
                    g_stack_result.error_code);
                bbb_log_stack_error(status);
            } else {
                bbb_log_stack_error(status);
            }
        }

        tick_flags = 0u;
        if (leap_device_stack_tick(&g_stack, bbb_monotonic_us(), &tick_flags) ==
            LEAP_DEVICE_STACK_OK) {
            if ((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u) {
                if (g_uart_in_safe == 0) {
                    g_uart_in_safe = 1;
                    bbb_uart_puts("SAFE (tick)\n");
                }
                apply_outputs(0u, 0);
            }
        }

        update_status_leds(&net, bbb_monotonic_us());
    }
}
