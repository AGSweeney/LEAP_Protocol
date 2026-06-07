#pragma once

#include <stdint.h>

#include "leap_transport.h"

struct LeapRuntime
{
    LeapTransport transport;
    uint32_t next_sequence;
    uint32_t rx_frames;
    uint32_t tx_frames;
    uint32_t dropped_frames;
};

bool leap_runtime_init(LeapRuntime *runtime);
void leap_runtime_poll(LeapRuntime *runtime);
