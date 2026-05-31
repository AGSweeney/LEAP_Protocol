/*
 * leap_pd_common.c
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap/leap_pd_common.h"

#include <string.h>

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

    view->header = (const LeapEndpointDataHeader*)payload;
    view->data   = payload + sizeof(LeapEndpointDataHeader);
    view->data_length = payload_length - sizeof(LeapEndpointDataHeader);

    if (view->header->data_length > view->data_length)
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    expected = leap_pd_endpoint_payload_size(
        view->header->profile_id,
        view->header->endpoint_id);
    if (expected != 0u && view->header->data_length != expected)
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

    view->header = (const LeapExchangeHeader*)payload;
    total        = sizeof(LeapExchangeHeader) +
                   (size_t)view->header->write_length +
                   (size_t)view->header->read_length;

    if (payload_length < total)
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    view->write_data = payload + sizeof(LeapExchangeHeader);
    view->write_length = view->header->write_length;
    view->read_reservation = view->write_data + view->write_length;
    view->read_length = view->header->read_length;

    return LEAP_PD_COMMON_OK;
}

size_t leap_pd_build_write_endpoint(
    uint8_t*                 out,
    size_t                   out_capacity,
    const LeapPdBuildParams* params,
    const uint8_t*           endpoint_data,
    size_t                   endpoint_data_length)
{
    LeapEndpointDataHeader* hdr;
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
    hdr = (LeapEndpointDataHeader*)out;
    hdr->endpoint_id             = params->endpoint_id;
    hdr->data_length             = (uint16_t)endpoint_data_length;
    hdr->endpoint_flags          = params->endpoint_flags;
    hdr->process_sequence        = params->process_sequence;
    hdr->cycle_time_us           = params->cycle_time_us;
    hdr->controller_timestamp_us = params->controller_timestamp_us;
    hdr->max_frame_age_us        = params->max_frame_age_us;
    hdr->profile_id              = params->profile_id;

    memcpy(out + sizeof(LeapEndpointDataHeader), endpoint_data, endpoint_data_length);
    return total;
}

size_t leap_pd_build_digital_write(
    uint8_t*                 out,
    size_t                   out_capacity,
    const LeapPdBuildParams* params,
    uint16_t                 digital_outputs)
{
    LeapProfileDigital16x16 profile;
    LeapPdBuildParams         local;

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

    memset(&profile, 0, sizeof(profile));
    profile.digital_outputs = digital_outputs;
    profile.io_status       = LEAP_DIO_STATUS_OK;

    return leap_pd_build_write_endpoint(
        out,
        out_capacity,
        &local,
        (const uint8_t*)&profile,
        sizeof(profile));
}

size_t leap_pd_build_digital_exchange(
    uint8_t* out,
    size_t   out_capacity,
    uint32_t process_sequence,
    uint32_t cycle_time_us,
    uint32_t profile_id,
    uint16_t digital_outputs)
{
    LeapExchangeHeader*     hdr;
    LeapProfileDigital16x16 write_profile;
    size_t                    total;

    if (profile_id == 0u)
    {
        profile_id = LEAP_PROFILE_DIGITAL_IO_16X16;
    }

    total = sizeof(LeapExchangeHeader) + (sizeof(LeapProfileDigital16x16) * 2u);
    if (out == NULL || out_capacity < total)
    {
        return 0u;
    }

    memset(out, 0, total);
    hdr = (LeapExchangeHeader*)out;
    hdr->write_endpoint_id         = LEAP_ENDPOINT_DIGITAL_OUTPUTS;
    hdr->read_endpoint_id          = LEAP_ENDPOINT_DIGITAL_INPUTS;
    hdr->write_length              = (uint16_t)sizeof(LeapProfileDigital16x16);
    hdr->read_length               = (uint16_t)sizeof(LeapProfileDigital16x16);
    hdr->process_sequence          = process_sequence;
    hdr->cycle_time_us             = cycle_time_us;
    hdr->profile_id                = profile_id;
    hdr->exchange_flags            = (uint16_t)LEAP_PD_FLAG_APPLY_OUTPUTS;

    memset(&write_profile, 0, sizeof(write_profile));
    write_profile.digital_outputs = digital_outputs;
    write_profile.io_status       = LEAP_DIO_STATUS_OK;
    memcpy(out + sizeof(LeapExchangeHeader), &write_profile, sizeof(write_profile));

    return total;
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
    LeapExchangeHeader* reply_hdr;
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
    reply_hdr = (LeapExchangeHeader*)out;
    *reply_hdr = *request;

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

    memcpy(
        out + sizeof(LeapExchangeHeader) + write_length + read_length,
        status,
        sizeof(LeapExchangeStatus));

    return total;
}

LeapPdCommonStatus leap_pd_unpack_digital16x16_outputs(
    const LeapPdEndpointView* view,
    uint16_t*                 digital_outputs)
{
    const LeapProfileDigital16x16* profile;

    if (view == NULL || digital_outputs == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    if (view->header->endpoint_id != LEAP_ENDPOINT_DIGITAL_OUTPUTS)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    if (view->data_length < sizeof(LeapProfileDigital16x16))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    profile = (const LeapProfileDigital16x16*)view->data;
    *digital_outputs = profile->digital_outputs;
    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_unpack_digital16x16_inputs(
    const LeapPdEndpointView* view,
    uint16_t*                 digital_inputs)
{
    const LeapProfileDigital16x16* profile;

    if (view == NULL || digital_inputs == NULL)
    {
        return LEAP_PD_COMMON_ERROR;
    }

    if (view->header->endpoint_id != LEAP_ENDPOINT_DIGITAL_INPUTS)
    {
        return LEAP_PD_COMMON_PROFILE_MISMATCH;
    }

    if (view->data_length < sizeof(LeapProfileDigital16x16))
    {
        return LEAP_PD_COMMON_BAD_LENGTH;
    }

    profile = (const LeapProfileDigital16x16*)view->data;
    *digital_inputs = profile->digital_inputs;
    return LEAP_PD_COMMON_OK;
}

LeapPdCommonStatus leap_pd_pack_digital16x16(
    uint8_t* out,
    size_t   out_capacity,
    uint16_t digital_outputs,
    uint16_t digital_inputs,
    uint16_t io_status)
{
    LeapProfileDigital16x16 profile;

    if (out == NULL || out_capacity < sizeof(profile))
    {
        return LEAP_PD_COMMON_BUFFER_TOO_SMALL;
    }

    memset(&profile, 0, sizeof(profile));
    profile.digital_outputs = digital_outputs;
    profile.digital_inputs  = digital_inputs;
    profile.io_status       = io_status;

    memcpy(out, &profile, sizeof(profile));
    return LEAP_PD_COMMON_OK;
}
