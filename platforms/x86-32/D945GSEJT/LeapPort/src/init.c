/*
 * init.c — LeapOS Init task: full LEAP device stack on D945GSEJT (re0).
 *
 * Profile: 8x8 digital I/O over the LPT1 parallel port.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include <rtems.h>
#include <rtems/bsd/bsd.h>
#include <rtems/bspIo.h>

#include "leap_board.h"
#include "leap_config.h"
#include "leap_time.h"
#include "leap_transport.h"

#include "leap/leap_build_info.h"
#include "leap/leap_device_stack.h"
#include "leap/leap_dir_device.h"
#include "leap/leap_mgmt_device.h"
#include "leap/leap_protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEAP_TRACE_VERBOSE 0

static LeapRtemsTransport g_transport;
static LeapRtemsBoardIo   g_board_io;

static void
device_send_reply(
    LeapDeviceStack *stack,
    const uint8_t *dst_mac,
    const LeapDeviceStackResult *result,
    uint16_t service_id,
    uint16_t message_type,
    const uint8_t *payload,
    size_t payload_length)
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
    LeapDeviceStack *stack,
    const uint8_t *dst_mac,
    const LeapDeviceStackResult *result,
    uint16_t service_id,
    uint16_t message_type,
    uint16_t status_code)
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
		(const uint8_t *)&err,
		sizeof(err)) != 0)
	{
		leap_device_stack_notify_tx_drop(stack);
		return;
	}

	leap_device_stack_notify_tx_ok(stack, leap_rtems_monotonic_us());
}

static void
device_apply_pd_result(const LeapDeviceStackResult *result)
{
	if ((result->flags & LEAP_DEVICE_STACK_FLAG_OUTPUTS_APPLIED) == 0u)
	{
		return;
	}

	leap_rtems_board_apply_outputs(&g_board_io, result->pd_outputs_applied);
	leap_rtems_board_sample_inputs(&g_board_io);
}

rtems_task
Init(rtems_task_argument ignored)
{
	LeapDeviceStack stack;
	LeapDeviceStackResult result;
	LeapDeviceStackConfig stack_config;
	LeapPdDeviceIoBinding pd_io;
	rtems_status_code sc;
	uint8_t rx_payload[LEAP_RTEMS_RX_BUF_SIZE];
	uint8_t src_mac[LEAP_RTEMS_MAC_LEN];
	size_t rx_len = 0u;
	uint64_t now_us;
	uint64_t last_tick_us = 0u;

	(void)ignored;

	printf("\n" LEAP_TS_FMT LEAP_ANSI_BANNER
	    "=== LeapOS booting (D945GSEJT / Atom N270) ===" LEAP_ANSI_RESET "\n",
	    leap_rtems_uptime_str());
	printf(LEAP_TS_FMT LEAP_ANSI_BANNER
	    "*** LeapOS LEAP device (D945GSEJT, LPT 8x8 I/O) ***" LEAP_ANSI_RESET "\n",
	    leap_rtems_uptime_str());
	leap_build_info_print(stdout, "device");
	fflush(stdout);

	sc = rtems_bsd_initialize();
	if (sc != RTEMS_SUCCESSFUL)
	{
		printf(LEAP_TS_FMT LEAP_ANSI_ERR "network init failed: %s" LEAP_ANSI_RESET "\n",
		    leap_rtems_uptime_str(), rtems_status_text(sc));
		exit(1);
	}

	(void)rtems_task_wake_after(2);
	sleep(2);

	leap_rtems_board_init(&g_board_io);

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
	if (leap_rtems_transport_init_auto(
		&g_transport, LEAP_RTEMS_ETHERTYPE) != 0)
#else
	if (leap_rtems_transport_init(
		&g_transport, LEAP_RTEMS_IFNAME, LEAP_RTEMS_ETHERTYPE) != 0)
#endif
	{
		printf(LEAP_TS_FMT LEAP_ANSI_ERR "E LEAP transport init failed" LEAP_ANSI_RESET "\n",
		    leap_rtems_uptime_str());
		rtems_task_suspend(RTEMS_SELF);
	}

	memcpy(
	    stack.dir.config.identity.primary_mac,
	    g_transport.local_mac,
	    LEAP_RTEMS_MAC_LEN);
	memcpy(
	    stack.disc.config.identity.primary_mac,
	    g_transport.local_mac,
	    LEAP_RTEMS_MAC_LEN);
	leap_dir_device_sync_disc(&stack.dir, &stack.disc);
	leap_mgmt_device_on_transport_ready(&stack.mgmt);

	/* Let asynchronous libbsd link-state messages drain before our banner so
	 * the kernel printk output does not interleave with this line. */
	fflush(stdout);
	usleep(150000);

	printf(
	    LEAP_TS_FMT LEAP_ANSI_OK "LEAP full stack listening on %s (DISC/DIR/MGMT/PD/DIAG)"
	    LEAP_ANSI_RESET "\n",
	    leap_rtems_uptime_str(), g_transport.ifname);
	fflush(stdout);

	for (;;)
	{
		now_us = leap_rtems_monotonic_us();
		int recv_rc;

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
			    &stack,
			    src_mac,
			    now_us,
			    rx_payload,
			    rx_len,
			    &result);

			if (status == LEAP_DEVICE_STACK_OK)
			{
				device_apply_pd_result(&result);
#if LEAP_TRACE_VERBOSE
				printf(
				    "LEAP RX ok: svc=0x%04X msg=0x%04X flags=0x%08X state=0x%04X\n",
				    (unsigned)result.service_id,
				    (unsigned)result.frame.header.message_type,
				    (unsigned)result.flags,
				    (unsigned)result.device_state);
#endif

				if (result.service_id == LEAP_SERVICE_DISC &&
				    (result.flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u &&
				    result.disc_message_type != 0u)
				{
#if LEAP_TRACE_VERBOSE
					printf(
					    "LEAP TX DISC: msg=0x%04X len=%u\n",
					    (unsigned)result.disc_message_type,
					    (unsigned)result.disc_payload_length);
#endif
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
#if LEAP_TRACE_VERBOSE
					printf(
					    "LEAP TX PD: msg=0x%04X len=%u\n",
					    (unsigned)result.pd_reply_message_type,
					    (unsigned)result.pd_reply_payload_length);
#endif
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
#if LEAP_TRACE_VERBOSE
					printf(
					    "LEAP TX MGMT: msg=0x%04X len=%u\n",
					    (unsigned)result.mgmt_reply.message_type,
					    (unsigned)result.mgmt_reply.payload_length);
#endif
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
#if LEAP_TRACE_VERBOSE
					printf(
					    "LEAP TX DIR: msg=0x%04X len=%u\n",
					    (unsigned)result.dir_message_type,
					    (unsigned)result.dir_payload_length);
#endif
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
#if LEAP_TRACE_VERBOSE
					printf(
					    "LEAP TX DIAG: msg=0x%04X len=%u\n",
					    (unsigned)result.diag_message_type,
					    (unsigned)result.diag_payload_length);
#endif
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
			else if (status == LEAP_DEVICE_STACK_PD_REJECTED ||
			         status == LEAP_DEVICE_STACK_DIR_ERROR ||
			         status == LEAP_DEVICE_STACK_DIAG_ERROR)
			{
				printf(
				    LEAP_TS_FMT LEAP_ANSI_ERR "LEAP service error: status=%d svc=0x%04X msg=0x%04X err=0x%04X state=0x%04X" LEAP_ANSI_RESET "\n",
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
				printf(
				    LEAP_TS_FMT LEAP_ANSI_WARN "LEAP frame ignored: status=%d svc=0x%04X msg=0x%04X parse=%s len=%u" LEAP_ANSI_RESET "\n",
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

		if (last_tick_us == 0u ||
		    now_us >= last_tick_us + LEAP_RTEMS_TICK_PERIOD_US)
		{
			(void)leap_device_stack_tick(&stack, now_us, NULL);
			last_tick_us = now_us;
		}
	}
}

#define RTEMS_BSD_CONFIG_DOMAIN_PAGE_MBUFS_SIZE (128 * 1024 * 1024)
#define RTEMS_BSD_CONFIG_DOMAIN_BIO_SIZE (16 * 1024 * 1024)
#define RTEMS_BSD_CONFIG_BSP_CONFIG
#define RTEMS_BSD_CONFIG_INIT

#include <machine/rtems-bsd-config.h>

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 256
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1
#define CONFIGURE_UNLIMITED_ALLOCATION_SIZE 32
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE (64 * 1024)
#define CONFIGURE_BDBUF_MAX_READ_AHEAD_BLOCKS 4
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (1 * 1024 * 1024)

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT_TASK_STACK_SIZE (128 * 1024)
#define CONFIGURE_INIT_TASK_INITIAL_MODES RTEMS_DEFAULT_MODES
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_DEFAULT_ATTRIBUTES
#define CONFIGURE_INIT

#include <rtems/confdefs.h>
