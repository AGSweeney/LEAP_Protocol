/*
 * examples/win_l2/identify_main.c
 *
 * Unicast IDENTIFY or LOCATE_DEVICE to one LEAP peer.
 *
 * Usage:
 *   leap_win_identify.exe [options] [adapter]
 *   leap_win_identify.exe --peer-mac 94:51:dc:21:f0:2f '\Device\NPF_{GUID}'
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "../win_smoke/leap_win_io.h"
#include "leap_win_common.h"

#include "leap/leap_build_info.h"
#include "leap/leap_controller_peer.h"
#include "leap/leap_controller_stack.h"
#include "leap/leap_disc_controller.h"
#include "leap/leap_frame.h"
#include "leap/leap_log.h"
#include "leap/leap_protocol.h"
#include "leap/leap_raw_winpcap.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef struct LeapWinIdentifyOptions
{
    const char* adapter;
    const char* peer_mac_text;
    uint8_t     peer_mac[6];
    int         has_peer_mac;
    int         locate;
    unsigned    duration_ms;
    unsigned    pattern;
    int         scan_ms;
    int         promiscuous;
    int         list_adapters;
    int         recv_timeout_ms;
    int         release_first;
} LeapWinIdentifyOptions;

static void identify_unbuffer_stdout(void)
{
#if defined(_WIN32)
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);
#endif
}

static void identify_print_usage(const char* prog)
{
    printf(
        "Usage: %s [options] [adapter]\n"
        "\n"
        "  adapter            Npcap device\n"
        "  --list             List Npcap adapters\n"
        "  --peer-mac MAC     Target device MAC (or discover one peer)\n"
        "  --scan-ms MS       HELLO scan when --peer-mac omitted (default 2000)\n"
        "  --locate           Send LOCATE_DEVICE (GPIO13 blink) instead of IDENTIFY\n"
        "  --duration-ms MS   Locate duration (default 3000)\n"
        "  --pattern N        Locate pattern 0-4 (default 1 slow blink)\n"
        "  --no-release-first Skip bootstrap+OWNER_RELEASE before DISC request\n"
        "  --timeout-ms MS    Reply wait (default 1000)\n"
        "  --promisc          Promiscuous capture (default on)\n"
        "  --no-promisc       Disable promiscuous capture\n",
        prog != NULL ? prog : "leap_win_identify");
}

static void identify_parse_args(int argc, char** argv, LeapWinIdentifyOptions* options)
{
    unsigned i;

    if (options == NULL)
    {
        return;
    }

    memset(options, 0, sizeof(*options));
    options->scan_ms         = 2000;
    options->duration_ms     = 3000u;
    options->pattern           = LEAP_LOCATE_PATTERN_SLOW_BLINK;
    options->promiscuous       = 1;
    options->recv_timeout_ms   = 1000;
    options->release_first     = 1;

    for (i = 1u; i < (unsigned)argc; i++)
    {
        if (strcmp(argv[i], "--list") == 0)
        {
            options->list_adapters = 1;
        }
        else if (strcmp(argv[i], "--peer-mac") == 0 && (i + 1u) < (unsigned)argc)
        {
            options->peer_mac_text = argv[++i];
        }
        else if (strcmp(argv[i], "--scan-ms") == 0 && (i + 1u) < (unsigned)argc)
        {
            options->scan_ms = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--locate") == 0)
        {
            options->locate = 1;
        }
        else if (strcmp(argv[i], "--duration-ms") == 0 && (i + 1u) < (unsigned)argc)
        {
            options->duration_ms = (unsigned)strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--pattern") == 0 && (i + 1u) < (unsigned)argc)
        {
            options->pattern = (unsigned)strtoul(argv[++i], NULL, 10);
        }
        else if (strcmp(argv[i], "--timeout-ms") == 0 && (i + 1u) < (unsigned)argc)
        {
            options->recv_timeout_ms = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--promisc") == 0)
        {
            options->promiscuous = 1;
        }
        else if (strcmp(argv[i], "--no-promisc") == 0)
        {
            options->promiscuous = 0;
        }
        else if (strcmp(argv[i], "--no-release-first") == 0)
        {
            options->release_first = 0;
        }
        else if (argv[i][0] != '-' && options->adapter == NULL)
        {
            options->adapter = argv[i];
        }
    }
}

static void identify_hello_from_peer_entry(
    const LeapControllerPeerEntry* entry,
    LeapHelloReply*                hello)
{
    if (entry == NULL || hello == NULL)
    {
        return;
    }

    memset(hello, 0, sizeof(*hello));
    hello->current_state      = entry->device_state;
    hello->active_profile_id  = entry->active_profile_id;
    hello->default_profile_id = entry->default_profile_id;
    memcpy(hello->active_owner_mac, entry->active_owner_mac, 6);
}

static void identify_release_stale_owner(
    LeapControllerStack*           stack,
    const LeapControllerStackIo*   io,
    LeapRawWinpcapSocket*          transport,
    const uint8_t*                 peer_mac,
    const LeapHelloReply*          hello_reply)
{
    LeapControllerStackStatus status;

    if (stack == NULL || io == NULL || transport == NULL || peer_mac == NULL)
    {
        return;
    }

    (void)leap_raw_winpcap_drain_rx(transport);

    status = leap_controller_stack_bootstrap_peer(
        stack, io, peer_mac, hello_reply);
    if (status == LEAP_CTRL_STACK_OK)
    {
        (void)leap_controller_stack_release(stack, io);
        leap_log_printf("released stale owner session before DISC\n");
        (void)leap_raw_winpcap_drain_rx(transport);
    }
    else
    {
        leap_log_printf(
            "release-first skipped (bootstrap status=%d phase=%u)\n",
            (int)status,
            (unsigned)leap_controller_stack_get_phase(stack));
        leap_controller_stack_reset(stack);
    }
}

static int identify_wait_for_reply(
    const LeapControllerStackIo* io,
    const uint8_t*               peer_mac,
    uint16_t                     expect_message_type,
    int                          timeout_ms,
    uint8_t*                     reply_payload,
    size_t                       reply_capacity,
    size_t*                      reply_length)
{
    uint64_t deadline_us = 0u;
    uint64_t start_us;

    if (io == NULL || peer_mac == NULL || reply_payload == NULL ||
        reply_length == NULL || io->recv_frame == NULL ||
        io->monotonic_us == NULL)
    {
        return -1;
    }

    start_us = io->monotonic_us(io->user_ctx);
    if (start_us != 0u)
    {
        deadline_us = start_us + ((uint64_t)timeout_ms * 1000u);
    }

    for (;;)
    {
        uint8_t       src_mac[6];
        uint8_t       payload_buf[1600];
        size_t        payload_length = 0u;
        LeapFrameView view;
        int           recv_timeout_ms = timeout_ms;

        if (deadline_us != 0u)
        {
            uint64_t now_us = io->monotonic_us(io->user_ctx);

            if (now_us >= deadline_us)
            {
                return -1;
            }

            recv_timeout_ms =
                (int)((deadline_us - now_us + 999u) / 1000u);
            if (recv_timeout_ms <= 0)
            {
                recv_timeout_ms = 1;
            }
        }

        if (io->recv_frame(
                io->user_ctx,
                src_mac,
                payload_buf,
                sizeof(payload_buf),
                &payload_length,
                &view,
                recv_timeout_ms) != 0)
        {
            return -1;
        }

        if (memcmp(src_mac, peer_mac, 6) != 0)
        {
            continue;
        }

        if (leap_frame_parse(payload_buf, payload_length, &view) !=
                LEAP_FRAME_OK)
        {
            continue;
        }

        if (view.header.service_id != (uint16_t)LEAP_SERVICE_DISC ||
            view.header.message_type != expect_message_type)
        {
            continue;
        }

        if (view.payload_length > reply_capacity)
        {
            return -1;
        }

        if (view.payload_length > 0u && view.payload != NULL)
        {
            memcpy(reply_payload, view.payload, view.payload_length);
        }

        *reply_length = view.payload_length;
        return 0;
    }
}

int main(int argc, char** argv)
{
    LeapWinIdentifyOptions    options;
    LeapRawWinpcapSocket      transport;
    LeapRawWinpcapOpenOptions open_options;
    LeapControllerStackIo     io;
    LeapControllerStack       stack;
    LeapControllerStackConfig stack_config;
    LeapControllerPeerTable   table;
    uint8_t                   peer_mac[6];
    uint8_t                   payload[64];
    size_t                    payload_length = 0u;
    uint8_t                   reply_payload[128];
    size_t                    reply_length = 0u;
    uint16_t                  message_type;
    uint16_t                  expect_reply_type;
    const char*               adapter_label;

#if !defined(_WIN32)
    (void)argc;
    (void)argv;
    fprintf(stderr, "leap_win_identify requires Windows and Npcap.\n");
    return 1;
#else
    identify_unbuffer_stdout();
    leap_log_reset_origin();
    identify_parse_args(argc, argv, &options);

    if (leap_win_handle_version_arg(argc, argv, "leap_win_identify") != 0)
    {
        return 0;
    }

    leap_build_info_print(stdout, "leap_win_identify");

    if (options.list_adapters != 0)
    {
        leap_raw_winpcap_list_devices();
        return 0;
    }

    for (unsigned i = 1u; i < (unsigned)argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            identify_print_usage(argv[0]);
            return 0;
        }
    }

    memset(&open_options, 0, sizeof(open_options));
    open_options.promiscuous           = options.promiscuous;
    open_options.filter_leap_ethertype = 1;

    if (leap_raw_winpcap_open(
            &transport,
            options.adapter,
            LEAP_ETHERTYPE_DEVELOPMENT,
            &open_options) != 0)
    {
        leap_win_print_transport_error("open");
        return 1;
    }

    leap_controller_peer_table_init(&table);
    leap_win_controller_io_init(&io, &transport);
    (void)leap_raw_winpcap_drain_rx(&transport);

    if (options.peer_mac_text != NULL)
    {
        if (leap_controller_peer_parse_mac(options.peer_mac_text, peer_mac) == 0)
        {
            leap_log_eprintf("invalid --peer-mac\n");
            leap_raw_winpcap_close(&transport);
            return 1;
        }
    }
    else
    {
        LeapControllerPeerDiscoverConfig disc_config;

        memset(&disc_config, 0, sizeof(disc_config));
        disc_config.scan_duration_ms = options.scan_ms;
        disc_config.min_peers        = 1u;

        if (leap_controller_peer_table_discover_ex(&table, &io, &disc_config) !=
                LEAP_CTRL_PEER_OK ||
            table.count == 0u)
        {
            leap_log_eprintf("discovery failed (no peer)\n");
            leap_raw_winpcap_close(&transport);
            return 1;
        }

        memcpy(peer_mac, table.peers[0].mac, 6);
    }

    adapter_label =
        (options.adapter != NULL) ? options.adapter : transport.device_name;

    if (options.release_first != 0)
    {
        const LeapControllerPeerEntry* entry;
        int                            peer_index;

        leap_controller_peer_table_init(&table);
        if (leap_controller_peer_table_probe_peer(
                &table, &io, peer_mac, options.recv_timeout_ms) ==
                LEAP_CTRL_PEER_OK)
        {
            peer_index = leap_controller_peer_table_find(&table, peer_mac);
            entry      = (peer_index >= 0)
                             ? leap_controller_peer_table_get(
                                   &table, (unsigned)peer_index)
                             : NULL;

            if (entry != NULL &&
                entry->device_state == (uint16_t)LEAP_STATE_OP &&
                leap_controller_peer_owned_by_other(
                    entry, transport.local_mac) == 0)
            {
                LeapHelloReply hello;

                identify_hello_from_peer_entry(entry, &hello);
                memset(&stack_config, 0, sizeof(stack_config));
                memcpy(stack_config.mgmt.controller_mac,
                       transport.local_mac,
                       6);
                memcpy(stack_config.target_peer_mac, peer_mac, 6);
                if (options.recv_timeout_ms < 2000)
                {
                    stack_config.recv_timeout_ms = 2000;
                }
                else
                {
                    stack_config.recv_timeout_ms = options.recv_timeout_ms;
                }
                leap_controller_stack_init(&stack, &stack_config);
                identify_release_stale_owner(
                    &stack, &io, &transport, peer_mac, &hello);
            }
            else if (entry != NULL)
            {
                leap_log_printf(
                    "release-first skipped (peer state=%u)\n",
                    (unsigned)entry->device_state);
            }
        }
    }

    if (options.locate != 0)
    {
        payload_length = leap_disc_controller_build_locate_device(
            (uint16_t)options.duration_ms,
            (uint8_t)options.pattern,
            LEAP_LOCATE_FLAG_LED,
            payload,
            sizeof(payload));
        message_type       = LEAP_DISC_LOCATE_DEVICE;
        expect_reply_type  = LEAP_DISC_LOCATE_DEVICE_REPLY;
    }
    else
    {
        payload_length = leap_disc_controller_build_identify(
            NULL,
            0u,
            payload,
            sizeof(payload));
        message_type       = LEAP_DISC_IDENTIFY;
        expect_reply_type  = LEAP_DISC_IDENTIFY_REPLY;
    }

    if (payload_length == 0u)
    {
        leap_log_eprintf("request build failed\n");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    leap_log_printf(
        "LEAP %s on %s peer=",
        options.locate != 0 ? "LOCATE_DEVICE" : "IDENTIFY",
        adapter_label);
    leap_win_print_mac(NULL, peer_mac);

    if (io.send_frame(
            io.user_ctx,
            peer_mac,
            0u,
            (uint16_t)LEAP_SERVICE_DISC,
            message_type,
            0u,
            1u,
            0u,
            payload,
            payload_length) != 0)
    {
        leap_log_eprintf("send failed\n");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    if (identify_wait_for_reply(
            &io,
            peer_mac,
            expect_reply_type,
            options.recv_timeout_ms,
            reply_payload,
            sizeof(reply_payload),
            &reply_length) != 0)
    {
        leap_log_eprintf("reply timeout\n");
        leap_raw_winpcap_close(&transport);
        return 1;
    }

    if (options.locate != 0)
    {
        LeapLocateDeviceReply reply;

        if (leap_disc_controller_on_locate_device_reply(
                reply_payload,
                reply_length,
                &reply) != LEAP_DISC_CTRL_OK)
        {
            leap_log_eprintf("LOCATE_DEVICE_REPLY parse failed\n");
            leap_raw_winpcap_close(&transport);
            return 1;
        }

        leap_log_printf(
            "LOCATE_DEVICE_REPLY supported=%u active=%u remaining_ms=%u\n",
            (unsigned)reply.supported,
            (unsigned)reply.active,
            (unsigned)reply.remaining_ms);
    }
    else
    {
        LeapIdentifyReply reply;

        if (leap_disc_controller_on_identify_reply(
                reply_payload,
                reply_length,
                &reply) != LEAP_DISC_CTRL_OK)
        {
            leap_log_eprintf("IDENTIFY_REPLY parse failed\n");
            leap_raw_winpcap_close(&transport);
            return 1;
        }

        leap_log_printf("IDENTIFY_REPLY state=%u locate_caps=0x%04X mac=",
                        (unsigned)reply.current_state,
                        (unsigned)reply.locate_capability_flags);
        leap_win_print_mac(NULL, reply.identity.primary_mac);
        leap_log_printf("\n");
    }

    leap_raw_winpcap_close(&transport);
    return 0;
#endif
}
