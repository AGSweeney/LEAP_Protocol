/*
 * clearcore_leap_io.cpp
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "clearcore_leap_io.h"
#include "clearcore_leap_trace.h"

#include "ClearCore.h"
#include "leap/leap_protocol.h"

#include <stdio.h>

static const uint8_t k_io_count = 6U;

extern "C" void clearcore_leap_io_init(ClearcoreLeapIoShadow *io)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_outputs = 0U;
    io->digital_inputs  = 0x0003U;
    io->safe_outputs    = 0U;
    io->io_status       = LEAP_DIO_STATUS_OK;
    io->safe_active     = 1;

    ConnectorIO0.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO1.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO2.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO3.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO4.Mode(Connector::INPUT_DIGITAL);
    ConnectorIO5.Mode(Connector::INPUT_DIGITAL);
}

static void clearcore_leap_set_output_bit(uint8_t bit, int value)
{
    switch (bit)
    {
    case 0U:
        ConnectorIO0.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorIO0.State(value != 0);
        break;
    case 1U:
        ConnectorIO1.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorIO1.State(value != 0);
        break;
    case 2U:
        ConnectorIO2.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorIO2.State(value != 0);
        break;
    case 3U:
        ConnectorIO3.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorIO3.State(value != 0);
        break;
    case 4U:
        ConnectorIO4.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorIO4.State(value != 0);
        break;
    case 5U:
        ConnectorIO5.Mode(Connector::OUTPUT_DIGITAL);
        ConnectorIO5.State(value != 0);
        break;
    default:
        break;
    }
}

static void clearcore_leap_set_input_bit(uint8_t bit)
{
    switch (bit)
    {
    case 0U:
        ConnectorIO0.Mode(Connector::INPUT_DIGITAL);
        break;
    case 1U:
        ConnectorIO1.Mode(Connector::INPUT_DIGITAL);
        break;
    case 2U:
        ConnectorIO2.Mode(Connector::INPUT_DIGITAL);
        break;
    case 3U:
        ConnectorIO3.Mode(Connector::INPUT_DIGITAL);
        break;
    case 4U:
        ConnectorIO4.Mode(Connector::INPUT_DIGITAL);
        break;
    case 5U:
        ConnectorIO5.Mode(Connector::INPUT_DIGITAL);
        break;
    default:
        break;
    }
}

extern "C" void clearcore_leap_io_refresh_inputs(ClearcoreLeapIoShadow *io)
{
    uint16_t inputs = 0U;

    if (io == NULL)
    {
        return;
    }

    for (uint8_t bit = 0U; bit < k_io_count; ++bit)
    {
        int level = 0;

        clearcore_leap_set_input_bit(bit);
        switch (bit)
        {
        case 0U:
            level = ConnectorIO0.State();
            break;
        case 1U:
            level = ConnectorIO1.State();
            break;
        case 2U:
            level = ConnectorIO2.State();
            break;
        case 3U:
            level = ConnectorIO3.State();
            break;
        case 4U:
            level = ConnectorIO4.State();
            break;
        case 5U:
            level = ConnectorIO5.State();
            break;
        default:
            break;
        }

        if (level != 0)
        {
            inputs = (uint16_t)(inputs | (uint16_t)(1U << bit));
        }
    }

    io->digital_inputs = inputs;
}

extern "C" void clearcore_leap_io_apply_outputs(ClearcoreLeapIoShadow *io, uint16_t outputs)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 0;
    io->digital_outputs = outputs;
    clearcore_leap_io_refresh_inputs(io);
    io->io_status = LEAP_DIO_STATUS_OK;

    for (uint8_t bit = 0U; bit < k_io_count; ++bit)
    {
        clearcore_leap_set_output_bit(
            bit,
            ((outputs >> bit) & 1U) != 0U ? 1 : 0);
    }

    {
        static uint16_t last_logged_outputs = 0xFFFFU;

        if (outputs != last_logged_outputs)
        {
            char line[64];

            last_logged_outputs = outputs;
            (void)snprintf(
                line,
                sizeof(line),
                "LEAP I/O: outputs=0x%04X inputs=0x%04X",
                io->digital_outputs,
                io->digital_inputs);
            clearcore_leap_trace_queue(line);
        }
    }
}

extern "C" void clearcore_leap_io_enter_safe(ClearcoreLeapIoShadow *io)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 1;
    io->digital_outputs = io->safe_outputs;

    for (uint8_t bit = 0U; bit < k_io_count; ++bit)
    {
        clearcore_leap_set_output_bit(
            bit,
            ((io->safe_outputs >> bit) & 1U) != 0U ? 1 : 0);
    }

    char line[64];
    (void)snprintf(
        line,
        sizeof(line),
        "LEAP I/O: safe outputs active (0x%04X)",
        io->digital_outputs);
    clearcore_leap_trace_queue(line);
}
