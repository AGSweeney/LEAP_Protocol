#pragma once

#include <stdint.h>

#include "leap_config.h"

struct LeapRxFrame
{
    uint8_t src_mac[6];
    int32_t interface_number;
    uint16_t ethertype;
    uint16_t payload_len;
    uint8_t payload[LEAP_MAX_FRAME_BYTES];
};

struct LeapTransport
{
    bool initialized;
    bool custom_rx_registered;
    int32_t interface_number;
    uint16_t ethertype_filter;
};

bool leap_transport_init(LeapTransport *transport, uint16_t ethertype_filter);
bool leap_transport_receive(LeapTransport *transport, LeapRxFrame *frame_out);
bool leap_transport_send(LeapTransport *transport,
                         const uint8_t dst_mac[6],
                         uint16_t ethertype,
                         const uint8_t *payload,
                         uint16_t payload_len);
