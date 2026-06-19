/*
 * leap_pd_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_pd_common.h"

#include "leap/leap_dir_device.h"
#include "leap/leap_log.h"

#include "../../leap_wire.h"

#include <stdint.h>
#include <string.h>

static void leap_pd_parse_endpoint_header(
    const uint8_t*          wire,
    LeapEndpointDataHeader* out)
{
    out->endpoint_id             = leap_wire_read_le16(wire + 0);
    out->endpoint_offset         = leap_wire_read_le16(wire + 2);
    out->data_length             = leap_wire_read_le16(wire + 4);
    out->endpoint_flags          = leap_wire_read_le16(wire + 6);
    out->process_sequence        = leap_wire_read_le32(wire + 8);
    out->cycle_time_us           = leap_wire_read_le32(wire + 12);
    out->controller_timestamp_us = leap_wire_read_le64(wire + 16);
    out->max_frame_age_us        = leap_wire_read_le32(wire + 24);
    out->profile_id              = leap_wire_read_le32(wire + 28);
}

static void leap_pd_write_endpoint_header(
    uint8_t*                      wire,
    const LeapEndpointDataHeader* hdr)
{
    leap_wire_write_le16(wire + 0, hdr->endpoint_id);
    leap_wire_write_le16(wire + 2, hdr->endpoint_offset);
    leap_wire_write_le16(wire + 4, hdr->data_length);
    leap_wire_write_le16(wire + 6, hdr->endpoint_flags);
    leap_wire_write_le32(wire + 8, hdr->process_sequence);
    leap_wire_write_le32(wire + 12, hdr->cycle_time_us);
    leap_wire_write_le64(wire + 16, hdr->controller_timestamp_us);
    leap_wire_write_le32(wire + 24, hdr->max_frame_age_us);
    leap_wire_write_le32(wire + 28, hdr->profile_id);
}

static void leap_pd_parse_exchange_header(
    const uint8_t*        wire,
    LeapExchangeHeader* out)
{
    out->write_endpoint_id       = leap_wire_read_le16(wire + 0);
    out->read_endpoint_id        = leap_wire_read_le16(wire + 2);
    out->write_length            = leap_wire_read_le16(wire + 4);
    out->read_length             = leap_wire_read_le16(wire + 6);
    out->process_sequence        = leap_wire_read_le32(wire + 8);
    out->cycle_time_us           = leap_wire_read_le32(wire + 12);
    out->controller_timestamp_us = leap_wire_read_le64(wire + 16);
    out->max_frame_age_us        = leap_wire_read_le32(wire + 24);
    out->profile_id              = leap_wire_read_le32(wire + 28);
    out->exchange_flags          = leap_wire_read_le16(wire + 32);
    out->reserved                = leap_wire_read_le16(wire + 34);
}

static void leap_pd_write_exchange_header(
    uint8_t*                    wire,
    const LeapExchangeHeader* hdr)
{
    leap_wire_write_le16(wire + 0, hdr->write_endpoint_id);
    leap_wire_write_le16(wire + 2, hdr->read_endpoint_id);
    leap_wire_write_le16(wire + 4, hdr->write_length);
    leap_wire_write_le16(wire + 6, hdr->read_length);
    leap_wire_write_le32(wire + 8, hdr->process_sequence);
    leap_wire_write_le32(wire + 12, hdr->cycle_time_us);
    leap_wire_write_le64(wire + 16, hdr->controller_timestamp_us);
    leap_wire_write_le32(wire + 24, hdr->max_frame_age_us);
    leap_wire_write_le32(wire + 28, hdr->profile_id);
    leap_wire_write_le16(wire + 32, hdr->exchange_flags);
    leap_wire_write_le16(wire + 34, hdr->reserved);
}

static void leap_pd_parse_exchange_status(
    const uint8_t*        wire,
    LeapExchangeStatus* out)
{
    out->latest_process_sequence_consumed = leap_wire_read_le32(wire + 0);
    out->device_process_sequence          = leap_wire_read_le32(wire + 4);
    out->measured_cycle_time_us           = leap_wire_read_le32(wire + 8);
    out->device_timestamp_us_low          = leap_wire_read_le32(wire + 12);
    out->device_timestamp_us_high         = leap_wire_read_le32(wire + 16);
    out->status_code                      = leap_wire_read_le16(wire + 20);
    out->endpoint_status_flags            = leap_wire_read_le16(wire + 22);
}

static void leap_pd_write_exchange_status(
    uint8_t*                    wire,
    const LeapExchangeStatus* status)
{
    leap_wire_write_le32(wire + 0, status->latest_process_sequence_consumed);
    leap_wire_write_le32(wire + 4, status->device_process_sequence);
    leap_wire_write_le32(wire + 8, status->measured_cycle_time_us);
    leap_wire_write_le32(wire + 12, status->device_timestamp_us_low);
    leap_wire_write_le32(wire + 16, status->device_timestamp_us_high);
    leap_wire_write_le16(wire + 20, status->status_code);
    leap_wire_write_le16(wire + 22, status->endpoint_status_flags);
}

static void leap_pd_write_digital16x16(
    uint8_t*  wire,
    uint16_t  digital_inputs,
    uint16_t  digital_outputs,
    uint16_t  io_status)
{
    leap_wire_write_le16(wire + 0, digital_inputs);
    leap_wire_write_le16(wire + 2, digital_outputs);
    leap_wire_write_le16(wire + 4, io_status);
    wire[6] = 0u;
    wire[7] = 0u;
}

void leap_pd_profile_map_init_default(LeapPdProfileMap* out)
{
    if (out == NULL)
    {
        return;
    }

    out->profile_id             = LEAP_PROFILE_DIGITAL_IO_16X16;
    out->write_endpoint_id      = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    out->read_endpoint_id       = LEAP_ENDPOINT_DIGITAL_INPUTS;
    out->endpoint_payload_size  = sizeof(LeapProfileDigital16x16);
    out->valid                  = 1;
}

LeapPdCommonStatus leap_pd_profile_map_from_profile_id(
    uint32_t          profile_id,
    LeapPdProfileMap* out)
{
    if (out == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    leap_pd_profile_map_init_default(out);

    if (profile_id == LEAP_PROFILE_DIGITAL_IO_16X16 ||
        profile_id == LEAP_PROFILE_DIGITAL_IO_8X8 ||
        profile_id == LEAP_PROFILE_DIGITAL_IO_32X32)
    {
        out->profile_id = profile_id;
        return LEAP_PD_COMMON_OK;
    }

    out->valid = 0;
    return LEAP_PD_COMMON_PROFILE_MISMATCH;
}

LeapPdCommonStatus leap_pd_profile_map_from_dir(
    const LeapDirDeviceContext* dir,
    LeapPdProfileMap*           out)
{
    const LeapDirDeviceProfile* profile;
    uint32_t                    profile_id;
    size_t                      i;

    if (dir == NULL || out == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    profile_id = dir->config.active_profile_id;
    if (profile_id == 0u)
    {
        profile_id = dir->config.default_profile_id;
    }
    if (profile_id == 0u)
    {
        leap_pd_profile_map_init_default(out);
        return LEAP_PD_COMMON_OK;
    }

    profile = NULL;
    for (i = 0u; i < dir->config.profile_count; i++)
    {
        if (dir->config.profiles[i].descriptor.profile_id == profile_id)
        {
            profile = &dir->config.profiles[i];
            break;
        }
    }

    if (profile == NULL)
    {
        return leap_pd_profile_map_from_profile_id(profile_id, out);
    }

    memset(out, 0, sizeof(*out));
    out->profile_id = profile_id;
    out->valid      = 1;

    for (i = 0u; i < profile->endpoint_count; i++)
    {
        const LeapEndpointDescriptor* ep = &profile->endpoints[i];
        size_t                        payload_size;

        payload_size = leap_pd_endpoint_payload_size(profile_id, ep->endpoint_id);
        if (payload_size == 0u)
        {
            payload_size = (size_t)ep->byte_length;
        }

        if (ep->direction == (uint8_t)LEAP_ENDPOINT_DIR_CONTROLLER_TO_DEVICE)
        {
            out->write_endpoint_id = ep->endpoint_id;
            if (payload_size > 0u)
            {
                out->endpoint_payload_size = payload_size;
            }
        }
        else if (ep->direction == (uint8_t)LEAP_ENDPOINT_DIR_DEVICE_TO_CONTROLLER)
        {
            out->read_endpoint_id = ep->endpoint_id;
            if (out->endpoint_payload_size == 0u && payload_size > 0u)
            {
                out->endpoint_payload_size = payload_size;
            }
        }
    }

    if (out->write_endpoint_id == 0u || out->read_endpoint_id == 0u)
    {
        leap_pd_profile_map_init_default(out);
        out->profile_id = profile_id;
    }

    if (out->endpoint_payload_size == 0u)
    {
        out->endpoint_payload_size = sizeof(LeapProfileDigital16x16);
    }

    return LEAP_PD_COMMON_OK;
}

size_t leap_pd_endpoint_payload_size(uint32_t profile_id, uint16_t endpoint_id)
{
    if (profile_id == LEAP_PROFILE_DIGITAL_IO_16X16 ||
        profile_id == LEAP_PROFILE_DIGITAL_IO_8X8 ||
        profile_id == LEAP_PROFILE_DIGITAL_IO_32X32)
    {
        if (endpoint_id == LEAP_ENDPOINT_DIGITAL_OUTPUTS ||
            endpoint_id == LEAP_ENDPOINT_DIGITAL_INPUTS)
        {
            return sizeof(LeapProfileDigital16x16);
        }
    }

    return 0u;
}

LeapPdCommonStatus leap_pd_endpoint_view(
    const uint8_t*      payload,
    size_t              payload_length,
    LeapPdEndpointView* view)
{
    size_t expected;

    if (view == NULL || payload == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    memset(view, 0, sizeof(*view));

    if (payload_length < sizeof(LeapEndpointDataHeader))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    leap_pd_parse_endpoint_header(payload, &view->header_storage);
    view->header     = &view->header_storage;
    view->data       = payload + sizeof(LeapEndpointDataHeader);
    view->data_length = payload_length - sizeof(LeapEndpointDataHeader);

    if (view->header_storage.data_length > view->data_length)
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    expected = leap_pd_endpoint_payload_size(
        view->header_storage.profile_id,
        view->header_storage.endpoint_id);
    if (expected != 0u && view->header_storage.data_length != expected)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_exchange_view(
    const uint8_t*      payload,
    size_t              payload_length,
    LeapPdExchangeView* view)
{
    size_t total;

    if (view == NULL || payload == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    memset(view, 0, sizeof(*view));

    if (payload_length < sizeof(LeapExchangeHeader))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    leap_pd_parse_exchange_header(payload, &view->header_storage);
    view->header = &view->header_storage;
    total        = sizeof(LeapExchangeHeader) +
                   (size_t)view->header_storage.write_length +
                   (size_t)view->header_storage.read_length;

    if (payload_length < total)
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    view->write_data = payload + sizeof(LeapExchangeHeader);
    view->write_length = view->header_storage.write_length;
    view->read_reservation = view->write_data + view->write_length;
    view->read_length = view->header_storage.read_length;

    return LEAP_PD_COMMON_OK;
}

size_t leap_pd_build_write_endpoint(
    uint8_t*                 out,
    size_t                   out_capacity,
    const LeapPdBuildParams* params,
    const uint8_t*           endpoint_data,
    size_t                   endpoint_data_length)
{
    LeapEndpointDataHeader hdr;
    size_t                  total;

    if (out == NULL || params == NULL || endpoint_data == NULL)
    {
        return 0u;
    }

    total = sizeof(LeapEndpointDataHeader) + endpoint_data_length;
    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    memset(&hdr, 0, sizeof(hdr));
    hdr.endpoint_id             = params->endpoint_id;
    hdr.data_length             = (uint16_t)endpoint_data_length;
    hdr.endpoint_flags          = params->endpoint_flags;
    hdr.process_sequence        = params->process_sequence;
    hdr.cycle_time_us           = params->cycle_time_us;
    hdr.controller_timestamp_us = params->controller_timestamp_us;
    hdr.max_frame_age_us        = params->max_frame_age_us;
    hdr.profile_id              = params->profile_id;
    leap_pd_write_endpoint_header(out, &hdr);

    memcpy(out + sizeof(LeapEndpointDataHeader), endpoint_data, endpoint_data_length);
    return total;
}

size_t leap_pd_build_digital_write(
    uint8_t*                 out,
    size_t                   out_capacity,
    const LeapPdBuildParams* params,
    uint16_t                 digital_outputs)
{
    LeapPdBuildParams local;
    uint8_t           profile_wire[sizeof(LeapProfileDigital16x16)];

    if (params == NULL)
    {
        return 0u;
    }

    local = *params;
    if (local.endpoint_id == 0u)
    {
        local.endpoint_id = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    }
    if (local.profile_id == 0u)
    {
        local.profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    }

    leap_pd_write_digital16x16(
        profile_wire,
        0u,
        digital_outputs,
        LEAP_DIO_STATUS_OK);

    return leap_pd_build_write_endpoint(
        out,
        out_capacity,
        &local,
        profile_wire,
        sizeof(profile_wire));
}

size_t leap_pd_build_digital_exchange_mapped(
    uint8_t*                out,
    size_t                  out_capacity,
    uint32_t                process_sequence,
    uint32_t                cycle_time_us,
    const LeapPdProfileMap* profile,
    uint16_t                digital_outputs,
    uint64_t                controller_timestamp_us,
    uint32_t                max_frame_age_us)
{
    LeapPdProfileMap    local;
    LeapExchangeHeader  hdr;
    size_t              payload_size;
    size_t              total;

    if (profile == NULL || profile->valid == 0)
    {
        leap_pd_profile_map_init_default(&local);
        profile = &local;
    }

    payload_size = profile->endpoint_payload_size;
    if (payload_size == 0u)
    {
        payload_size = sizeof(LeapProfileDigital16x16);
    }

    total = sizeof(LeapExchangeHeader) + (payload_size * 2u);
    if (out == NULL || out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    memset(&hdr, 0, sizeof(hdr));
    hdr.write_endpoint_id  = profile->write_endpoint_id;
    hdr.read_endpoint_id   = profile->read_endpoint_id;
    hdr.write_length       = (uint16_t)payload_size;
    hdr.read_length        = (uint16_t)payload_size;
    hdr.process_sequence   = process_sequence;
    hdr.cycle_time_us      = cycle_time_us;
    hdr.profile_id         = profile->profile_id;
    hdr.exchange_flags     = (uint16_t)LEAP_PD_FLAG_APPLY_OUTPUTS;
    hdr.controller_timestamp_us = controller_timestamp_us;
    hdr.max_frame_age_us        = max_frame_age_us;
    leap_pd_write_exchange_header(out, &hdr);

    leap_pd_write_digital16x16(
        out + sizeof(LeapExchangeHeader),
        0u,
        digital_outputs,
        LEAP_DIO_STATUS_OK);

    return total;
}

size_t leap_pd_build_digital_exchange(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t process_sequence,
    uint32_t cycle_time_us,
    uint32_t profile_id,
    uint16_t digital_outputs)
{
    LeapPdProfileMap profile;

    if (leap_pd_profile_map_from_profile_id(profile_id, &profile) != LEAP_PD_COMMON_OK)
    {
        leap_pd_profile_map_init_default(&profile);
        if (profile_id != 0u)
        {
            profile.profile_id = profile_id;
        }
    }

    return leap_pd_build_digital_exchange_mapped(
        out,
        out_capacity,
        process_sequence,
        cycle_time_us,
        &profile,
        digital_outputs,
        0u,
        0u);
}

size_t leap_pd_build_exchange_reply(
    uint8_t*                  out,
    size_t                    out_capacity,
    const LeapExchangeHeader* request,
    const uint8_t*            write_echo,
    size_t                    write_length,
    const uint8_t*            read_data,
    size_t                    read_length,
    const LeapExchangeStatus* status)
{
    LeapExchangeHeader reply_hdr;
    size_t              total;

    if (out == NULL || request == NULL || status == NULL)
    {
        return 0u;
    }

    total = sizeof(LeapExchangeHeader) + write_length + read_length +
            sizeof(LeapExchangeStatus);
    if (out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    reply_hdr = *request;
    leap_pd_write_exchange_header(out, &reply_hdr);

    if (write_length > 0u && write_echo != NULL)
    {
        memcpy(out + sizeof(LeapExchangeHeader), write_echo, write_length);
    }

    if (read_length > 0u && read_data != NULL)
    {
        memcpy(
            out + sizeof(LeapExchangeHeader) + write_length,
            read_data,
            read_length);
    }

    leap_pd_write_exchange_status(
        out + sizeof(LeapExchangeHeader) + write_length + read_length,
        status);

    return total;
}

LeapPdCommonStatus leap_pd_profile_validate_write(
    const LeapPdProfileMap* profile,
    uint32_t                profile_id,
    uint16_t                endpoint_id,
    uint16_t                data_length)
{
    size_t expected;

    if (profile == NULL || profile->valid == 0)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    if (profile_id != profile->profile_id ||
        endpoint_id != profile->write_endpoint_id)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    expected = profile->endpoint_payload_size;
    if (expected == 0u)
    {
        expected = leap_pd_endpoint_payload_size(profile_id, endpoint_id);
    }

    if (expected != 0u && data_length != expected)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_profile_validate_exchange(
    const LeapPdProfileMap* profile,
    uint32_t                profile_id,
    uint16_t                write_endpoint_id,
    uint16_t                read_endpoint_id,
    uint16_t                write_length,
    uint16_t                read_length)
{
    size_t expected_payload;

    if (profile == NULL || profile->valid == 0)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    if (profile_id != profile->profile_id ||
        write_endpoint_id != profile->write_endpoint_id ||
        read_endpoint_id != profile->read_endpoint_id)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    expected_payload = profile->endpoint_payload_size;
    if (expected_payload == 0u)
    {
        expected_payload = sizeof(LeapProfileDigital16x16);
    }

    if (write_length != expected_payload || read_length != expected_payload)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    return LEAP_PD_COMMON_OK;
}

uint64_t leap_pd_frame_age_us(
    uint64_t now_us,
    uint64_t controller_timestamp_us)
{
    if (controller_timestamp_us > now_us)
    {
        /* Clock skew or future timestamp — treat as stale. */
        return UINT64_MAX;
    }

    return now_us - controller_timestamp_us;
}

LeapPdCommonStatus leap_pd_check_frame_age(
    uint64_t now_us,
    uint64_t controller_timestamp_us,
    uint32_t max_frame_age_us,
    uint32_t jitter_margin_us)
{
    uint64_t age;
    uint64_t limit;

    if (max_frame_age_us == 0u)
    {
        return LEAP_PD_COMMON_OK;
    }

    age   = leap_pd_frame_age_us(now_us, controller_timestamp_us);
    limit = (uint64_t)max_frame_age_us + (uint64_t)jitter_margin_us;
    if (age > limit)
    {
        leap_log_security(
            LEAP_LOG_SEC_PD_STALE_FRAME,
            "age_us=%llu limit_us=%llu",
            (unsigned long long)age,
            (unsigned long long)limit);
        return LEAP_PD_COMMON_STALE_FRAME;
    }

    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_validate_exchange_reply(
    const uint8_t*          payload,
    size_t                  payload_length,
    const LeapPdProfileMap* profile,
    uint32_t                expected_process_sequence,
    LeapPdExchangeView*     view_out,
    LeapExchangeStatus*     status_out)
{
    return leap_pd_validate_exchange_reply_at(
        payload,
        payload_length,
        profile,
        expected_process_sequence,
        0u,
        0u,
        view_out,
        status_out);
}

LeapPdCommonStatus leap_pd_validate_exchange_reply_at(
    const uint8_t*          payload,
    size_t                  payload_length,
    const LeapPdProfileMap* profile,
    uint32_t                expected_process_sequence,
    uint64_t                recv_now_us,
    uint32_t                jitter_margin_us,
    LeapPdExchangeView*     view_out,
    LeapExchangeStatus*     status_out)
{
    LeapPdExchangeView        view;
    LeapPdCommonStatus        status;
    LeapExchangeStatus        reply_status;
    size_t                    status_offset;

    if (payload == NULL || profile == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    status = leap_pd_exchange_view(payload, payload_length, &view);
    if (status != LEAP_PD_COMMON_OK)
    {
        return status;
    }

    status = leap_pd_profile_validate_exchange(
        profile,
        view.header_storage.profile_id,
        view.header_storage.write_endpoint_id,
        view.header_storage.read_endpoint_id,
        view.header_storage.write_length,
        view.header_storage.read_length);
    if (status != LEAP_PD_COMMON_OK)
    {
        return status;
    }

    if (recv_now_us != 0u)
    {
        status = leap_pd_check_frame_age(
            recv_now_us,
            view.header_storage.controller_timestamp_us,
            view.header_storage.max_frame_age_us,
            jitter_margin_us);
        if (status != LEAP_PD_COMMON_OK)
        {
            return status;
        }
    }

    status_offset = sizeof(LeapExchangeHeader) +
                    (size_t)view.header_storage.write_length +
                    (size_t)view.header_storage.read_length;
    if (payload_length < status_offset + sizeof(LeapExchangeStatus))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    leap_pd_parse_exchange_status(payload + status_offset, &reply_status);
    if (reply_status.status_code != (uint16_t)LEAP_STATUS_OK)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    if (expected_process_sequence != 0u &&
        reply_status.latest_process_sequence_consumed !=
            expected_process_sequence)
    {
        return LEAP_PD_COMMON_SEQUENCE_MISMATCH;
    }

    if (view_out != NULL)
    {
        *view_out = view;
    }

    if (status_out != NULL)
    {
        *status_out = reply_status;
    }

    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_unpack_digital16x16_outputs(
    const LeapPdEndpointView* view,
    uint16_t*                 digital_outputs)
{
    if (view == NULL || digital_outputs == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    if (view->header_storage.endpoint_id != LEAP_ENDPOINT_DIGITAL_OUTPUTS)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    if (view->data_length < sizeof(LeapProfileDigital16x16))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    *digital_outputs = leap_wire_read_le16(view->data + 2);
    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_unpack_digital16x16_inputs(
    const LeapPdEndpointView* view,
    uint16_t*                 digital_inputs)
{
    if (view == NULL || digital_inputs == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    if (view->header_storage.endpoint_id != LEAP_ENDPOINT_DIGITAL_INPUTS)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    if (view->data_length < sizeof(LeapProfileDigital16x16))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    *digital_inputs = leap_wire_read_le16(view->data + 0);
    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_pack_digital16x16(
    uint8_t* out,
    size_t   out_capacity,
    uint16_t digital_outputs,
    uint16_t digital_inputs,
    uint16_t io_status)
{
    if (out == NULL || out_capacity < sizeof(LeapProfileDigital16x16))
    {
        return LEAP_PD_COMMON_BUFFER_TOO_SMALL;
    }

    leap_pd_write_digital16x16(out, digital_inputs, digital_outputs, io_status);
    return LEAP_PD_COMMON_OK;
}
