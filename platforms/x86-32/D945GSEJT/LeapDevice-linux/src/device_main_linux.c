/*
 * device_main_linux.c — LEAP device daemon for Alpine Linux on D945GSEJT.
 *
 * Profile: 8x8 digital I/O over LPT1 @ 0x378, raw L2 on eth0.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "device_net.h"
#include "leap_board.h"
#include "leap_config.h"
#include "leap_time.h"
#include "leap_transport.h"

#include "leap/leap_build_info.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_frame.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEAP_TRACE_VERBOSE 0
#define LEAP_DEVICE_IF_WAIT_S 30

static LeapRtemsTransport g_transport;
static LeapRtemsBoardIo   g_board_io;

static void
leapdevice_host_enter_safe(void* ctx)
{
    leap_rtems_board_enter_safe((LeapRtemsBoardIo*)ctx);
}

static void
device_send_reply(
    LeapDeviceStack*              stack,
    const uint8_t*                dst_mac,
    const LeapDeviceStackResult*  result,
    uint16_t                      service_id,
    uint16_t                      message_type,
    const uint8_t*                payload,
    size_t                        payload_length)
{
    if (leap_rtems_transport_send_leap(
            &g_transport,
            dst_mac,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            payload,
            payload_length) != 0)
    {
        leap_device_stack_notify_tx_drop(stack);
        return;
    }

    leap_device_stack_notify_tx_ok(stack, leap_rtems_monotonic_us());
}

static void
device_send_error_reply(
    LeapDeviceStack*              stack,
    const uint8_t*                dst_mac,
    const LeapDeviceStackResult*  result,
    uint16_t                      service_id,
    uint16_t                      message_type,
    uint16_t                      status_code)
{
    LeapErrorPayload err;

    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

    if (leap_rtems_transport_send_leap(
            &g_transport,
            dst_mac,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR | LEAP_FLAG_ACK_REQUESTED),
            service_id,
            message_type,
            result->frame.header.session_id,
            result->frame.header.sequence,
            result->frame.header.ack_sequence,
            (const uint8_t*)&err,
            sizeof(err)) != 0)
    {
        leap_device_stack_notify_tx_drop(stack);
        return;
    }

    leap_device_stack_notify_tx_ok(stack, leap_rtems_monotonic_us());
}

static void
device_apply_pd_result(const LeapDeviceStackResult* result)
{
    if ((result->flags & LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) == 0u)
    {
        return;
    }

    leap_rtems_board_apply_outputs(&g_board_io, result->pd_outputs_applied);
    leap_rtems_board_sample_inputs(&g_board_io);
}

static void
device_prepare_network(void)
{
    static const char* const candidates[] = { "eth0", "eth1", "eth2", NULL };
    size_t                   i;

    for (i = 0; candidates[i] != NULL; ++i)
    {
        if (device_net_wait_for_iface(candidates[i], LEAP_DEVICE_IF_WAIT_S) == 0 &&
            device_net_bring_up(candidates[i]) == 0)
        {
            return;
        }
    }
}

int
main(void)
{
    LeapDeviceStack       stack;
    LeapDeviceStackResult result;
    LeapDeviceStackConfig stack_config;
    LeapPdDeviceIoBinding pd_io;
    uint8_t               rx_payload[LEAP_RTEMS_RX_BUF_SIZE];
    uint8_t               src_mac[LEAP_RTEMS_MAC_LEN];
    size_t                rx_len = 0u;
    uint64_t              now_us;
    uint64_t              last_tick_us = 0u;

    printf("\n" LEAP_TS_FMT LEAP_ANSI_BANNER
           "=== LeapOS booting (D945GSEJT / Atom N270, Alpine Linux) ===" LEAP_ANSI_RESET "\n",
           leap_rtems_uptime_str());
    printf(LEAP_TS_FMT LEAP_ANSI_BANNER
           "*** LeapOS LEAP device (D945GSEJT, %s) ***" LEAP_ANSI_RESET "\n",
           leap_rtems_uptime_str(), leap_board_description());
    leap_build_info_print(stdout, "device");
    fflush(stdout);

    device_prepare_network();
    sleep(1);

    leap_rtems_board_init(&g_board_io);
    if (leap_board_port_io_ready() == 0)
    {
        printf(LEAP_TS_FMT LEAP_ANSI_WARN
               "%s unavailable (need root / PCI card / MCC_DIO24_IO_BASE) — digital I/O disabled"
               LEAP_ANSI_RESET "\n",
               leap_rtems_uptime_str(), leap_board_description());
    }
    else if (leap_board_pci_address() != NULL)
    {
        printf(LEAP_TS_FMT LEAP_ANSI_OK
               "%s ready at PCI %s" LEAP_ANSI_RESET "\n",
               leap_rtems_uptime_str(),
               leap_board_description(),
               leap_board_pci_address());
    }

    memset(&stack_config, 0, sizeof(stack_config));
    stack_config.mgmt.default_lease_us = 5000000u;
    stack_config.mgmt.default_watchdog_us = 500000u;
    stack_config.mgmt.max_lease_us = 10000000u;
    stack_config.mgmt.max_watchdog_us = 1000000u;
    (void)leap_dir_device_config_set_digital_io(
        &stack_config.dir,
        LEAP_RTEMS_PROFILE_ID,
        LEAP_RTEMS_DIGITAL_OUTPUTS,
        LEAP_RTEMS_DIGITAL_INPUTS);

    leap_device_stack_init_full(&stack, &stack_config);
    memset(&pd_io, 0, sizeof(pd_io));
    pd_io.digital_outputs = &g_board_io.digital_outputs;
    pd_io.digital_inputs = &g_board_io.digital_inputs;
    pd_io.io_status = &g_board_io.io_status;
    leap_device_stack_bind_pd_io(&stack, &pd_io);

#if LEAP_RTEMS_IFNAME_AUTO
    if (leap_rtems_transport_init_auto(&g_transport, LEAP_RTEMS_ETHERTYPE) != 0)
#else
    if (leap_rtems_transport_init(
            &g_transport, LEAP_RTEMS_IFNAME, LEAP_RTEMS_ETHERTYPE) != 0)
#endif
    {
        printf(LEAP_TS_FMT LEAP_ANSI_ERR "E LEAP transport init failed" LEAP_ANSI_RESET "\n",
               leap_rtems_uptime_str());
        return 1;
    }

    memcpy(stack.dir.config.identity.primary_mac, g_transport.local_mac, LEAP_RTEMS_MAC_LEN);
    memcpy(stack.disc.config.identity.primary_mac, g_transport.local_mac, LEAP_RTEMS_MAC_LEN);
    leap_dir_device_sync_disc(&stack.dir, &stack.disc);
    leap_mgmt_device_on_transport_ready(&stack.mgmt);

    printf(LEAP_TS_FMT LEAP_ANSI_OK
           "LEAP full stack listening on %s (DISC/DIR/MGMT/PD/DIAG)" LEAP_ANSI_RESET "\n",
           leap_rtems_uptime_str(), g_transport.ifname);
    fflush(stdout);

    for (;;)
    {
        int recv_rc;

        now_us = leap_rtems_monotonic_us();

        recv_rc = leap_rtems_transport_recv(
            &g_transport,
            src_mac,
            rx_payload,
            sizeof(rx_payload),
            &rx_len,
            LEAP_RTEMS_RECV_TIMEOUT_MS);

        if (recv_rc == 0)
        {
            LeapDeviceStackStatus status;

            status = leap_device_stack_process_frame(
                &stack, src_mac, now_us, rx_payload, rx_len, &result);

            if (status == LEAP_DEVICE_STACK_OK)
            {
                device_apply_pd_result(&result);
                leap_device_stack_apply_safe_on_flags(
                    result.flags, leapdevice_host_enter_safe, &g_board_io);

                if (result.service_id == LEAP_SERVICE_DISC &&
                    (result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u &&
                    result.disc_message_type != 0u)
                {
                    device_send_reply(
                        &stack,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_DISC,
                        result.disc_message_type,
                        result.disc_payload,
                        result.disc_payload_length);
                }
                else if (result.service_id == LEAP_SERVICE_PD &&
                         (result.flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u &&
                         result.pd_reply_message_type != 0u)
                {
                    device_send_reply(
                        &stack,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_PD,
                        result.pd_reply_message_type,
                        result.pd_reply_payload,
                        result.pd_reply_payload_length);
                }
                else if (result.service_id == LEAP_SERVICE_MGMT &&
                         (result.flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u &&
                         result.mgmt_reply.message_type != 0u)
                {
                    device_send_reply(
                        &stack,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_MGMT,
                        result.mgmt_reply.message_type,
                        result.mgmt_reply.payload,
                        result.mgmt_reply.payload_length);
                }
                else if (result.service_id == LEAP_SERVICE_DIR &&
                         (result.flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u &&
                         result.dir_message_type != 0u)
                {
                    device_send_reply(
                        &stack,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_DIR,
                        result.dir_message_type,
                        result.dir_payload,
                        result.dir_payload_length);
                }
                else if (result.service_id == LEAP_SERVICE_DIAG &&
                         (result.flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u &&
                         result.diag_message_type != 0u)
                {
                    device_send_reply(
                        &stack,
                        src_mac,
                        &result,
                        (uint16_t)LEAP_SERVICE_DIAG,
                        result.diag_message_type,
                        result.diag_payload,
                        result.diag_payload_length);
                }
            }
            else if (status == LEAP_DEVICE_STACK_MGMT_ERROR &&
                     result.service_id == (uint16_t)LEAP_SERVICE_MGMT &&
                     result.frame.header.message_type == LEAP_MGMT_OWNER_RELEASE &&
                     stack.mgmt.owner_active == 0u)
            {
                /* Idempotent RELEASE with no owner — no error reply. */
            }
            else if (status == LEAP_DEVICE_STACK_PD_REJECTED ||
                     status == LEAP_DEVICE_STACK_DIR_ERROR ||
                     status == LEAP_DEVICE_STACK_DIAG_ERROR ||
                     status == LEAP_DEVICE_STACK_MGMT_ERROR)
            {
                printf(LEAP_TS_FMT LEAP_ANSI_ERR
                       "LEAP service error: status=%d svc=0x%04X msg=0x%04X err=0x%04X state=0x%04X"
                       LEAP_ANSI_RESET "\n",
                       leap_rtems_uptime_str(),
                       (int)status,
                       (unsigned)result.service_id,
                       (unsigned)result.frame.header.message_type,
                       (unsigned)result.error_code,
                       (unsigned)result.device_state);
                device_send_error_reply(
                    &stack,
                    src_mac,
                    &result,
                    (uint16_t)result.service_id,
                    result.frame.header.message_type,
                    result.error_code);
            }
            else if (status != LEAP_DEVICE_STACK_OK)
            {
                printf(LEAP_TS_FMT LEAP_ANSI_WARN
                       "LEAP frame ignored: status=%d svc=0x%04X msg=0x%04X parse=%s len=%u"
                       LEAP_ANSI_RESET "\n",
                       leap_rtems_uptime_str(),
                       (int)status,
                       (unsigned)result.service_id,
                       (unsigned)result.frame.header.message_type,
                       leap_frame_parse_result_string(result.frame_error),
                       (unsigned)rx_len);
            }
        }
        else if (recv_rc > 0 && recv_rc != EAGAIN)
        {
            printf(LEAP_TS_FMT LEAP_ANSI_ERR "LEAP RX transport error: %d" LEAP_ANSI_RESET "\n",
                   leap_rtems_uptime_str(), recv_rc);
        }

        if (last_tick_us == 0u || now_us >= last_tick_us + LEAP_RTEMS_TICK_PERIOD_US)
        {
            uint32_t tick_flags = 0u;

            (void)leap_device_stack_tick(&stack, now_us, &tick_flags);
            leap_device_stack_apply_safe_on_flags(
                tick_flags, leapdevice_host_enter_safe, &g_board_io);
            last_tick_us = now_us;
        }
    }

    return 0;
}
