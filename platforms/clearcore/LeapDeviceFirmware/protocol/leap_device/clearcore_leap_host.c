/*

 * clearcore_leap_host.c

 *

 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

 * SPDX-License-Identifier: MIT

 */



#include <stdint.h>

#include <stddef.h>



#include "clearcore_leap_host.h"

#include "clearcore_leap_eth.h"

#include "clearcore_leap_io.h"

#include "clearcore_leap_trace.h"

#include "clearcore_leap_identity.h"

#include "clearcore_leap_locate.h"



#include "leap/leap_device_host_perf.h"

#include "leap/leap_device_stack.h"

#include "leap/leap_dir_device.h"

#include "leap/leap_disc_device.h"

#include "leap/leap_frame.h"

#include "leap/leap_mgmt_device.h"

#include "leap/leap_pd_device.h"

#include "leap/leap_protocol.h"



#include "lwip/netif.h"



#include <stdio.h>

#include <string.h>



extern uint32_t Microseconds(void);



typedef struct ClearcoreLeapRxSlot

{

    uint8_t  src_mac[6];

    uint8_t  payload[CLEARCORE_LEAP_HOST_MAX_FRAME];

    size_t   payload_length;

} ClearcoreLeapRxSlot;



static LeapDeviceStack       g_stack;

static ClearcoreLeapIoShadow g_io;

static LeapPdDeviceIoBinding g_pd_io;

static void clearcore_leap_pd_apply_outputs(uint16_t outputs, void *ctx)
{
    (void)ctx;
    clearcore_leap_io_apply_outputs(&g_io, outputs);
}

static struct netif *        g_netif;

static ClearcoreLeapRxSlot   g_rx_slots[CLEARCORE_LEAP_HOST_RX_DEPTH];

static uint8_t               g_rx_head;

static uint8_t               g_rx_tail;

static uint8_t               g_rx_count;

static ClearcoreLeapHostStats g_stats;



static uint64_t clearcore_leap_monotonic_us(void)

{

    return (uint64_t)Microseconds();

}



static void clearcore_leap_record_tx_result(int send_ok)

{

    if (send_ok == 0)

    {

        leap_device_stack_notify_tx_ok(&g_stack, clearcore_leap_monotonic_us());

        ++g_stats.tx_ok;

    }

    else

    {

        leap_device_stack_notify_tx_drop(&g_stack);

        ++g_stats.tx_drop;

    }

}



static void clearcore_leap_send_error_reply(

    struct netif *               netif,

    const uint8_t *              dst_mac,

    const LeapDeviceStackResult *result,

    uint16_t                     service_id,

    uint16_t                     message_type,

    uint16_t                     status_code)

{

    LeapErrorPayload err;

    uint8_t          tx[1600];

    size_t           tx_len = 0u;



    memset(&err, 0, sizeof(err));

    err.status_code = status_code;



    if (leap_frame_write(

            tx,

            sizeof(tx),

            &tx_len,

            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ERROR | LEAP_FLAG_ACK_REQUESTED),

            service_id,

            message_type,

            result->frame.header.session_id,

            result->frame.header.sequence,

            result->frame.header.ack_sequence,

            (const uint8_t*)&err,

            sizeof(err)) != 0)

    {

        leap_device_stack_notify_tx_drop(&g_stack);

        ++g_stats.tx_drop;

        return;

    }



    clearcore_leap_record_tx_result(

        clearcore_leap_eth_send(netif, dst_mac, tx, tx_len));

}



static void clearcore_leap_send_reply(

    struct netif *               netif,

    const uint8_t *            dst_mac,

    const LeapDeviceStackResult *result,

    uint16_t                   service_id,

    uint16_t                   message_type,

    const uint8_t *            payload,

    size_t                     payload_length)

{

    uint8_t tx[1600];

    size_t  tx_len = 0u;

    int     send_status;



    if (leap_frame_write(

            tx,

            sizeof(tx),

            &tx_len,

            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),

            service_id,

            message_type,

            result->frame.header.session_id,

            result->frame.header.sequence,

            result->frame.header.ack_sequence,

            payload,

            payload_length) != 0)

    {

        leap_device_stack_notify_tx_drop(&g_stack);

        ++g_stats.tx_drop;

        return;

    }



    send_status = clearcore_leap_eth_send(netif, dst_mac, tx, tx_len);

    clearcore_leap_record_tx_result(send_status);

}



static void clearcore_leap_log_rx(const LeapDeviceStackResult *result)

{

#if LEAP_DEVICE_HOST_TRACE_ENABLE
    char line[96];



    switch (result->service_id)

    {

    case LEAP_SERVICE_DISC:

        if (result->frame.header.message_type == LEAP_DISC_HELLO)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received HELLO");

            clearcore_leap_trace_queue(line);

        }

        break;

    case LEAP_SERVICE_DIR:

        if (result->frame.header.message_type == LEAP_DIR_SELECT_PROFILE)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received SELECT_PROFILE");

            clearcore_leap_trace_queue(line);

        }

        break;

    case LEAP_SERVICE_MGMT:

        if (result->frame.header.message_type == LEAP_MGMT_OPEN_SESSION)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received OPEN_SESSION");

            clearcore_leap_trace_queue(line);

        }

        else if (result->frame.header.message_type == LEAP_MGMT_SET_STATE)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received SET_STATE");

            clearcore_leap_trace_queue(line);

        }

        else if (result->frame.header.message_type == LEAP_MGMT_HEARTBEAT)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received HEARTBEAT");

            clearcore_leap_trace_queue(line);

        }

        break;

    case LEAP_SERVICE_PD:

        if (result->frame.header.message_type == LEAP_PD_WRITE_ENDPOINT)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received PD WRITE");

            clearcore_leap_trace_queue(line);

        }

        else if (result->frame.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS)

        {

            (void)snprintf(line, sizeof(line), "LEAP: received PD EXCHANGE");

            clearcore_leap_trace_queue(line);

        }

        break;

    default:

        break;

    }
#else
    (void)result;
#endif

}



static void clearcore_leap_apply_result(const LeapDeviceStackResult *result)

{

    (void)result;

    /* GPIO apply runs in leap_pd_apply_digital_outputs via g_pd_io.apply_outputs. */

}



static void clearcore_leap_log_status(
    LeapDeviceStackStatus        status,
    const LeapDeviceStackResult* result)
{
#if LEAP_DEVICE_HOST_TRACE_ENABLE
    char line[96];

    switch (status)
    {
    case LEAP_DEVICE_STACK_FRAME_ERROR:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: frame error parse=%d",
            (int)result->frame_error);
        clearcore_leap_trace_queue(line);
        break;
    case LEAP_DEVICE_STACK_PD_REJECTED:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: PD rejected msg=0x%04X status=0x%04X",
            result->frame.header.message_type,
            result->error_code);
        clearcore_leap_trace_queue(line);
        break;
    case LEAP_DEVICE_STACK_DIR_ERROR:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: DIR error msg=0x%04X status=0x%04X",
            result->frame.header.message_type,
            result->error_code);
        clearcore_leap_trace_queue(line);
        break;
    case LEAP_DEVICE_STACK_DIAG_ERROR:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: DIAG error msg=0x%04X status=0x%04X",
            result->frame.header.message_type,
            result->error_code);
        clearcore_leap_trace_queue(line);
        break;
    case LEAP_DEVICE_STACK_DISC_ERROR:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: DISC error msg=0x%04X status=0x%04X",
            result->frame.header.message_type,
            result->error_code);
        clearcore_leap_trace_queue(line);
        break;
    case LEAP_DEVICE_STACK_MGMT_ERROR:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: MGMT error msg=0x%04X status=0x%04X",
            result->frame.header.message_type,
            result->error_code);
        clearcore_leap_trace_queue(line);
        break;
    case LEAP_DEVICE_STACK_UNSUPPORTED_SERVICE:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: unsupported service=0x%04X",
            (unsigned)result->service_id);
        clearcore_leap_trace_queue(line);
        break;
    default:
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP: stack status=%d svc=0x%04X msg=0x%04X",
            (int)status,
            (unsigned)result->service_id,
            result->frame.header.message_type);
        clearcore_leap_trace_queue(line);
        break;
    }
#else
    (void)status;
    (void)result;
#endif
}

static void clearcore_leap_host_enter_safe(void *ctx)
{
    (void)ctx;
    clearcore_leap_io_enter_safe(&g_io);
}

static void clearcore_leap_handle_result(

    struct netif *               netif,

    const uint8_t *              src_mac,

    LeapDeviceStackStatus        status,

    const LeapDeviceStackResult *result)

{

    if (status == LEAP_DEVICE_STACK_OK)

    {

        clearcore_leap_log_rx(result);

        clearcore_leap_apply_result(result);

        leap_device_stack_apply_safe_on_flags(
            result->flags,
            clearcore_leap_host_enter_safe,
            NULL);

        if (result->service_id == (uint16_t)LEAP_SERVICE_DISC &&
            result->frame.header.message_type == LEAP_DISC_LOCATE_DEVICE &&
            result->frame.payload_length >= sizeof(LeapLocateDeviceRequest))
        {
            const LeapLocateDeviceRequest *req =
                (const LeapLocateDeviceRequest *)result->frame.payload;

            if ((req->flags & LEAP_LOCATE_FLAG_CANCEL) != 0u)
            {
                clearcore_leap_locate_start(0u, 0u, 1);
            }
            else
            {
                uint16_t accepted_ms =
                    leap_disc_clamp_locate_duration_ms(req->duration_ms);

                clearcore_leap_locate_start(
                    (uint32_t)accepted_ms * 1000u,
                    req->pattern,
                    0);
            }
        }

        if ((result->flags & LEAP_DEVICE_STACK_FLAG_DISC_HAS_REPLY) != 0u)

        {

            clearcore_leap_send_reply(

                netif,

                src_mac,

                result,

                (uint16_t)LEAP_SERVICE_DISC,

                result->disc_message_type,

                result->disc_payload,

                result->disc_payload_length);

        }

        else if ((result->flags & LEAP_DEVICE_STACK_FLAG_MGMT_HAS_REPLY) != 0u)

        {

            if (result->mgmt_reply.message_type == LEAP_MGMT_STATE_REPLY &&

                result->device_state == (uint16_t)LEAP_STATE_OP)

            {

                clearcore_leap_trace_queue("LEAP: entered OP");

            }



            clearcore_leap_send_reply(

                netif,

                src_mac,

                result,

                (uint16_t)LEAP_SERVICE_MGMT,

                result->mgmt_reply.message_type,

                result->mgmt_reply.payload,

                result->mgmt_reply.payload_length);

        }

        else if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIR_HAS_REPLY) != 0u)

        {

            if ((result->flags & LEAP_DEVICE_STACK_FLAG_DIR_PROFILE_SELECTED) != 0u)

            {

                clearcore_leap_trace_queue("LEAP: profile selected -> CONFIGURED");

            }



            clearcore_leap_send_reply(

                netif,

                src_mac,

                result,

                (uint16_t)LEAP_SERVICE_DIR,

                result->dir_message_type,

                result->dir_payload,

                result->dir_payload_length);

        }

        if ((result->flags & LEAP_DEVICE_STACK_FLAG_PD_HAS_REPLY) != 0u ||
            result->pd_reply_payload_length > 0u)

        {

#if LEAP_DEVICE_HOST_TRACE_ENABLE
            if (result->pd_reply_message_type == LEAP_PD_EXCHANGE_REPLY)

            {

                clearcore_leap_trace_queue("LEAP: sending PD EXCHANGE_REPLY");

            }
#endif



            clearcore_leap_send_reply(

                netif,

                src_mac,

                result,

                (uint16_t)LEAP_SERVICE_PD,

                result->pd_reply_message_type,

                result->pd_reply_payload,

                result->pd_reply_payload_length);

        }

        else if (result->service_id == (uint16_t)LEAP_SERVICE_DIAG &&

                 (result->flags & LEAP_DEVICE_STACK_FLAG_DIAG_HAS_REPLY) != 0u)

        {

            clearcore_leap_send_reply(

                netif,

                src_mac,

                result,

                (uint16_t)LEAP_SERVICE_DIAG,

                result->diag_message_type,

                result->diag_payload,

                result->diag_payload_length);

        }

    }

    else if (status == LEAP_DEVICE_STACK_PD_REJECTED)

    {

        clearcore_leap_send_error_reply(

            netif,

            src_mac,

            result,

            (uint16_t)LEAP_SERVICE_PD,

            result->frame.header.message_type,

            result->error_code);

        clearcore_leap_log_status(status, result);

    }

    else if (status == LEAP_DEVICE_STACK_DIR_ERROR)

    {

        clearcore_leap_send_error_reply(

            netif,

            src_mac,

            result,

            (uint16_t)LEAP_SERVICE_DIR,

            result->frame.header.message_type,

            result->error_code);

        clearcore_leap_log_status(status, result);

    }

    else if (status == LEAP_DEVICE_STACK_DIAG_ERROR)

    {

        clearcore_leap_send_error_reply(

            netif,

            src_mac,

            result,

            (uint16_t)LEAP_SERVICE_DIAG,

            result->frame.header.message_type,

            result->error_code);

        clearcore_leap_log_status(status, result);

    }

    else if (status == LEAP_DEVICE_STACK_DISC_ERROR)

    {

        clearcore_leap_send_error_reply(

            netif,

            src_mac,

            result,

            (uint16_t)LEAP_SERVICE_DISC,

            result->frame.header.message_type,

            result->error_code);

        clearcore_leap_log_status(status, result);

    }

    else if (status == LEAP_DEVICE_STACK_MGMT_ERROR)

    {

        if (result->service_id == (uint16_t)LEAP_SERVICE_MGMT &&
            result->frame.header.message_type == LEAP_MGMT_OWNER_RELEASE &&
            g_stack.mgmt.owner_active == 0u)
        {
            /* Idempotent RELEASE with no active owner. */
        }
        else if (result->error_code != 0u &&
                 result->error_code != (uint16_t)LEAP_STATUS_OK)
        {
            clearcore_leap_send_error_reply(

                netif,

                src_mac,

                result,

                (uint16_t)LEAP_SERVICE_MGMT,

                result->frame.header.message_type,

                result->error_code);
        }

        clearcore_leap_log_status(status, result);

    }

    else

    {

        clearcore_leap_log_status(status, result);

    }

}



/*
 * M2a: PD EXCHANGE fast path — single parse, bypass full device-stack dispatch.
 * See docs/LEAP_DEVICE_PERFORMANCE.md.
 */
static int clearcore_leap_send_pd_exchange_reply(
    struct netif *               netif,
    const uint8_t *              dst_mac,
    const LeapFrameView *        request,
    uint16_t                     message_type,
    const uint8_t *              payload,
    size_t                       payload_length)
{
    uint8_t tx[CLEARCORE_LEAP_PD_TX_BUF_MAX];
    size_t  tx_len = 0u;
    int     send_status;

    if (netif == NULL || dst_mac == NULL || request == NULL)
    {
        return -1;
    }

    if (leap_frame_write(
            tx,
            sizeof(tx),
            &tx_len,
            (uint8_t)(LEAP_FLAG_RESPONSE | LEAP_FLAG_ACK_REQUESTED),
            (uint16_t)LEAP_SERVICE_PD,
            message_type,
            request->header.session_id,
            request->header.sequence,
            request->header.ack_sequence,
            payload,
            payload_length) != 0)
    {
        leap_device_stack_notify_tx_drop(&g_stack);
        ++g_stats.tx_drop;
        return -1;
    }

    send_status = clearcore_leap_eth_send(netif, dst_mac, tx, tx_len);
    clearcore_leap_record_tx_result(send_status);
    return send_status;
}

static int clearcore_leap_handle_pd_exchange_fast(
    const ClearcoreLeapRxSlot *slot,
    const LeapFrameView *      view,
    uint64_t                   now_us)
{
    LeapPdDeviceResult      pd_result;
    LeapDeviceStackResult   stack_result;
    LeapPdDeviceStatus      pd_status;

    if (slot == NULL || view == NULL || g_netif == NULL)
    {
        return 0;
    }

    pd_status = leap_pd_device_process_parsed_frame(
        &g_stack.mgmt,
        &g_stack.pd,
        &g_pd_io,
        slot->src_mac,
        now_us,
        view,
        &pd_result);

    if (pd_status == LEAP_PD_DEVICE_OK &&
        pd_result.reply_payload_length > 0u)
    {
        /* M2b: skip per-exchange diag bookkeeping on the hot success path. */
        leap_device_stack_note_frame_rx(
            &g_stack,
            now_us,
            (uint16_t)LEAP_SERVICE_PD);

        if (clearcore_leap_send_pd_exchange_reply(
                g_netif,
                slot->src_mac,
                view,
                pd_result.reply_message_type,
                pd_result.reply_payload,
                pd_result.reply_payload_length) != 0)
        {
            return 1;
        }

        return 1;
    }

    if (pd_status == LEAP_PD_DEVICE_REJECTED)
    {
        leap_diag_device_on_pd_result(&g_stack.diag, &pd_result, now_us);
        memset(&stack_result, 0, sizeof(stack_result));
        stack_result.frame = pd_result.frame;
        clearcore_leap_send_error_reply(
            g_netif,
            slot->src_mac,
            &stack_result,
            (uint16_t)LEAP_SERVICE_PD,
            pd_result.frame.header.message_type,
            pd_result.error_code);
        clearcore_leap_log_status(LEAP_DEVICE_STACK_PD_REJECTED, &stack_result);
        return 1;
    }

    return 0;
}

static int clearcore_leap_slot_needs_input_refresh(const ClearcoreLeapRxSlot *slot)

{

    uint16_t service_id;



    if (slot == NULL)

    {

        return 0;

    }



    if (leap_device_frame_peek_service_id(
            slot->payload,
            slot->payload_length,
            &service_id) != 0)

    {

        return 0;

    }



    return (service_id == (uint16_t)LEAP_SERVICE_PD) ? 1 : 0;

}



static void clearcore_leap_process_slot(const ClearcoreLeapRxSlot *slot)

{

    LeapDeviceStackResult result;

    LeapDeviceStackStatus status;

    uint64_t              now_us;



    if (slot == NULL || g_netif == NULL)

    {

        return;

    }



    if (clearcore_leap_slot_needs_input_refresh(slot) != 0)

    {

        clearcore_leap_io_refresh_inputs(&g_io);

    }



    now_us = clearcore_leap_monotonic_us();

    {
        LeapFrameView        view;
        LeapFrameParseResult parse_result;

        parse_result =
            leap_frame_parse(slot->payload, slot->payload_length, &view);
        if (parse_result == LEAP_FRAME_OK &&
            view.header.service_id == (uint16_t)LEAP_SERVICE_PD &&
            view.header.message_type == LEAP_PD_EXCHANGE_ENDPOINTS &&
            clearcore_leap_handle_pd_exchange_fast(slot, &view, now_us) != 0)
        {
            ++g_stats.rx_ok;
            return;
        }
    }

    status = leap_device_stack_process_frame(

        &g_stack,

        slot->src_mac,

        now_us,

        slot->payload,

        slot->payload_length,

        &result);



    clearcore_leap_handle_result(g_netif, slot->src_mac, status, &result);

    ++g_stats.rx_ok;

}



int clearcore_leap_host_init(struct netif *netif)

{

    LeapDeviceStackConfig stack_config;

    char                  line[96];



    if (netif == NULL)

    {

        return -1;

    }



    g_netif = netif;

    g_rx_head  = 0u;

    g_rx_tail  = 0u;

    g_rx_count = 0u;

    memset(&g_stats, 0, sizeof(g_stats));



    clearcore_leap_io_init(&g_io);



    memset(&stack_config, 0, sizeof(stack_config));

    stack_config.mgmt.default_lease_us    = 5000000u;

    stack_config.mgmt.default_watchdog_us = 5000000u;

    stack_config.mgmt.max_lease_us        = 10000000u;

    stack_config.mgmt.max_watchdog_us     = 10000000u;

    (void)leap_dir_device_config_set_digital_io(
        &stack_config.dir,
        CLEARCORE_LEAP_PROFILE_ID,
        CLEARCORE_LEAP_DO_COUNT,
        CLEARCORE_LEAP_DI_COUNT);

    leap_device_stack_init_full(&g_stack, &stack_config);



    memset(&g_pd_io, 0, sizeof(g_pd_io));

    g_pd_io.digital_outputs = &g_io.digital_outputs;

    g_pd_io.digital_inputs  = &g_io.digital_inputs;

    g_pd_io.io_status       = &g_io.io_status;

    g_pd_io.outputs_dirty      = &g_io.outputs_dirty;
    g_pd_io.apply_outputs      = clearcore_leap_pd_apply_outputs;
    g_pd_io.apply_outputs_ctx  = NULL;

    leap_device_stack_bind_pd_io(&g_stack, &g_pd_io);



    if (clearcore_leap_eth_init() != 0)

    {

        return -1;

    }



    clearcore_leap_identity_apply(&g_stack, netif);

    clearcore_leap_locate_init();

    leap_mgmt_device_on_transport_ready(&g_stack.mgmt);



    (void)snprintf(

        line,

        sizeof(line),

        "LEAP device ready  MAC=%02x:%02x:%02x:%02x:%02x:%02x",

        netif->hwaddr[0],

        netif->hwaddr[1],

        netif->hwaddr[2],

        netif->hwaddr[3],

        netif->hwaddr[4],

        netif->hwaddr[5]);

    clearcore_leap_trace_queue(line);



    return 0;

}



int clearcore_leap_host_queue_frame(

    struct netif *  netif,

    const uint8_t * src_mac,

    const uint8_t * payload,

    size_t          payload_length)

{

    ClearcoreLeapRxSlot *slot;



    if (netif == NULL || src_mac == NULL || payload == NULL ||

        payload_length == 0u || payload_length > CLEARCORE_LEAP_HOST_MAX_FRAME)

    {

        ++g_stats.rx_drop;

        return -1;

    }



    if (g_rx_count >= CLEARCORE_LEAP_HOST_RX_DEPTH)

    {

        ++g_stats.rx_drop;

        return -1;

    }



    slot = &g_rx_slots[g_rx_head];

    memcpy(slot->src_mac, src_mac, 6);

    memcpy(slot->payload, payload, payload_length);

    slot->payload_length = payload_length;



    g_rx_head = (uint8_t)((g_rx_head + 1u) % CLEARCORE_LEAP_HOST_RX_DEPTH);

    ++g_rx_count;

    ++g_stats.rx_queued;



    if (g_netif == NULL)

    {

        g_netif = netif;

    }



    return 0;

}



int clearcore_leap_host_rx_pending(void)

{

    return g_rx_count > 0u ? 1 : 0;

}



void clearcore_leap_host_cyclic(void)

{

    uint32_t tick_flags = 0u;

    uint64_t now_us;



    if (g_netif == NULL)

    {

        return;

    }



    while (g_rx_count > 0u)

    {

        const ClearcoreLeapRxSlot *slot = &g_rx_slots[g_rx_tail];

        clearcore_leap_process_slot(slot);

        g_rx_tail = (uint8_t)((g_rx_tail + 1u) % CLEARCORE_LEAP_HOST_RX_DEPTH);

        --g_rx_count;

    }



    now_us = clearcore_leap_monotonic_us();

    (void)leap_device_stack_tick(&g_stack, now_us, &tick_flags);

    clearcore_leap_locate_update(now_us);

    if ((tick_flags & LEAP_DEVICE_STACK_FLAG_SAFE_STATE_ENTERED) != 0u)
    {
        clearcore_leap_trace_queue("LEAP: lease/watchdog expired -> SAFE");
    }

    leap_device_stack_apply_safe_on_flags(
        tick_flags,
        clearcore_leap_host_enter_safe,
        NULL);



#if LEAP_DEVICE_HOST_TRACE_ENABLE
    clearcore_leap_trace_flush();
#endif

}



const ClearcoreLeapHostStats *clearcore_leap_host_stats(void)

{

    return &g_stats;

}

