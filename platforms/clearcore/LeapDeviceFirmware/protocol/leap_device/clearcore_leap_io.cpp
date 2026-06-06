/*
 * clearcore_leap_io.cpp
 *
 * ClearCore ConnectorIO0..5 driver for LEAP PD outputs.
 * Pattern matches ClearLink clearcore_wrapper / ApplyDopOutput:
 *   ON  -> Mode(OUTPUT_DIGITAL) then State(true)
 *   OFF -> Mode(INPUT_DIGITAL)
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "clearcore_leap_io.h"
#include "clearcore_leap_trace.h"

#include "leap/leap_device_host_perf.h"

#include "ClearCore.h"
#include "leap/leap_protocol.h"

#include <stdio.h>

static const uint8_t k_io_count = 6U;
static const uint8_t k_all_pins = (uint8_t)((1U << 6U) - 1U);

static void clearcore_leap_io_set_pin_mode_output(uint8_t bit)
{
    switch (bit)
    {
    case 0U:
        (void)ConnectorIO0.Mode(Connector::OUTPUT_DIGITAL);
        break;
    case 1U:
        (void)ConnectorIO1.Mode(Connector::OUTPUT_DIGITAL);
        break;
    case 2U:
        (void)ConnectorIO2.Mode(Connector::OUTPUT_DIGITAL);
        break;
    case 3U:
        (void)ConnectorIO3.Mode(Connector::OUTPUT_DIGITAL);
        break;
    case 4U:
        (void)ConnectorIO4.Mode(Connector::OUTPUT_DIGITAL);
        break;
    case 5U:
        (void)ConnectorIO5.Mode(Connector::OUTPUT_DIGITAL);
        break;
    default:
        break;
    }
}

static void clearcore_leap_io_set_pin_mode_input(uint8_t bit)
{
    switch (bit)
    {
    case 0U:
        (void)ConnectorIO0.Mode(Connector::INPUT_DIGITAL);
        break;
    case 1U:
        (void)ConnectorIO1.Mode(Connector::INPUT_DIGITAL);
        break;
    case 2U:
        (void)ConnectorIO2.Mode(Connector::INPUT_DIGITAL);
        break;
    case 3U:
        (void)ConnectorIO3.Mode(Connector::INPUT_DIGITAL);
        break;
    case 4U:
        (void)ConnectorIO4.Mode(Connector::INPUT_DIGITAL);
        break;
    case 5U:
        (void)ConnectorIO5.Mode(Connector::INPUT_DIGITAL);
        break;
    default:
        break;
    }
}

static void clearcore_leap_io_drive_pin(uint8_t bit, int output_on)
{
    if (output_on != 0)
    {
        clearcore_leap_io_set_pin_mode_output(bit);
        switch (bit)
        {
        case 0U:
            (void)ConnectorIO0.State(true);
            break;
        case 1U:
            (void)ConnectorIO1.State(true);
            break;
        case 2U:
            (void)ConnectorIO2.State(true);
            break;
        case 3U:
            (void)ConnectorIO3.State(true);
            break;
        case 4U:
            (void)ConnectorIO4.State(true);
            break;
        case 5U:
            (void)ConnectorIO5.State(true);
            break;
        default:
            break;
        }
    }
    else
    {
        clearcore_leap_io_set_pin_mode_input(bit);
    }
}

static int clearcore_leap_io_read_pin(uint8_t bit)
{
    switch (bit)
    {
    case 0U:
        return ConnectorIO0.State();
    case 1U:
        return ConnectorIO1.State();
    case 2U:
        return ConnectorIO2.State();
    case 3U:
        return ConnectorIO3.State();
    case 4U:
        return ConnectorIO4.State();
    case 5U:
        return ConnectorIO5.State();
    default:
        return 0;
    }
}

extern "C" void clearcore_leap_io_init(ClearcoreLeapIoShadow *io)
{
    uint8_t bit;

    if (io == NULL)
    {
        return;
    }

    io->digital_outputs  = 0U;
    io->digital_inputs   = 0U;
    io->safe_outputs     = 0U;
    io->io_status        = LEAP_DIO_STATUS_OK;
    io->safe_active      = 1;
    io->outputs_dirty    = 0;
    io->pin_output_mask  = 0U;
    io->pin_state_mask   = 0U;

    /* Match ClearLink ApplicationInitialization: IO-0..IO-5 as outputs. */
    for (bit = 0U; bit < k_io_count; ++bit)
    {
        clearcore_leap_io_set_pin_mode_output(bit);
    }
    io->pin_output_mask = k_all_pins;

    clearcore_leap_io_refresh_inputs(io);
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
        const uint16_t bit_mask = (uint16_t)(1U << bit);

        if ((io->pin_output_mask & (uint8_t)bit_mask) != 0U)
        {
            if ((io->digital_outputs & bit_mask) != 0U)
            {
                inputs = (uint16_t)(inputs | bit_mask);
            }
            continue;
        }

        if (clearcore_leap_io_read_pin(bit) != 0)
        {
            inputs = (uint16_t)(inputs | bit_mask);
        }
    }

    io->digital_inputs = inputs;
}

extern "C" void clearcore_leap_io_apply_outputs(ClearcoreLeapIoShadow *io, uint16_t outputs)
{
    uint8_t pin_outputs = 0U;
    uint8_t pin_states  = 0U;

    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 0;
    io->digital_outputs = outputs;
    io->io_status       = LEAP_DIO_STATUS_OK;
    io->outputs_dirty   = 0;

    for (uint8_t bit = 0U; bit < k_io_count; ++bit)
    {
        const uint8_t  bit_mask = (uint8_t)(1U << bit);
        const int      on       = ((outputs >> bit) & 1U) != 0U;

        clearcore_leap_io_drive_pin(bit, on);

        if (on != 0)
        {
            pin_outputs = (uint8_t)(pin_outputs | bit_mask);
            pin_states  = (uint8_t)(pin_states | bit_mask);
        }
    }

    io->pin_output_mask = pin_outputs;
    io->pin_state_mask  = pin_states;
    io->digital_inputs  = outputs & k_all_pins;

    {
        static uint16_t last_usb_outputs = 0xFFFFU;

        if (outputs != last_usb_outputs)
        {
            char line[56];

            last_usb_outputs = outputs;
            (void)snprintf(
                line,
                sizeof(line),
                "LEAP GPIO apply: 0x%04X",
                (unsigned)outputs);
            ConnectorUsb.SendLine(line);
        }
    }

#if LEAP_DEVICE_HOST_TRACE_ENABLE
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
#endif
}

extern "C" void clearcore_leap_io_enter_safe(ClearcoreLeapIoShadow *io)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 1;
    io->digital_outputs = io->safe_outputs;
    io->outputs_dirty   = 1;

    clearcore_leap_io_apply_outputs(io, io->safe_outputs);

#if LEAP_DEVICE_HOST_TRACE_ENABLE
    {
        char line[64];
        (void)snprintf(
            line,
            sizeof(line),
            "LEAP I/O: safe outputs active (0x%04X)",
            io->digital_outputs);
        clearcore_leap_trace_queue(line);
    }
#endif
}
