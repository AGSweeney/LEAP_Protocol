/*
 * STM32F746G-Discovery simulated LEAP PD I/O.
 *
 * No physical digital I/O: the 8x8 profile is satisfied in software by
 * mirroring all eight output bits into the input shadow on every refresh.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_hw.h"

#include "main.h"
#include "stm32746g_discovery.h"

#include "leap/leap_protocol.h"

#include "stm32f7xx_hal.h"

void leap_hw_init(void)
{
    /* Discovery LED1 is used only for LEAP-DISC locate (not PD bits). */
}

void leap_hw_refresh_inputs(LeapStm746IoShadow *io)
{
    if (io == NULL)
    {
        return;
    }

    io->digital_inputs = (uint16_t)(io->digital_outputs & 0xFFu);
}

void leap_hw_apply_outputs(LeapStm746IoShadow *io, uint16_t outputs)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 0u;
    io->digital_outputs = (uint16_t)(outputs & 0xFFu);
    leap_hw_refresh_inputs(io);
    io->io_status = LEAP_DIO_STATUS_OK;
}

void leap_hw_enter_safe(LeapStm746IoShadow *io)
{
    if (io == NULL)
    {
        return;
    }

    io->safe_active     = 1u;
    io->digital_outputs = io->safe_outputs;
    leap_hw_refresh_inputs(io);
}

void leap_hw_set_locate_led(uint8_t on)
{
    if (on != 0u)
    {
        BSP_LED_On(LED1);
    }
    else
    {
        BSP_LED_Off(LED1);
    }
}

uint64_t leap_hw_monotonic_us(void)
{
    return (uint64_t)HAL_GetTick() * 1000u;
}
