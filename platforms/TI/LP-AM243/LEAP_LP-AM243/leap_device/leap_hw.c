/*
 * LP-AM243 LaunchPad hardware abstraction for LEAP PD I/O.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_hw.h"
#include "ti_drivers_config.h"

#include "leap/leap_device_host_perf.h"

#include <drivers/gpio.h>
#include <kernel/dpl/AddrTranslateP.h>
#include <kernel/dpl/ClockP.h>

#include "leap_am243_log.h"

typedef struct LeapAm243GpioPin
{
    uint32_t base_addr;
    uint32_t pin;
} LeapAm243GpioPin;

static LeapAm243GpioPin g_outputs[8];
static LeapAm243GpioPin g_inputs[8];
static uint16_t         g_applied_outputs = 0xFFFFu;

static void leap_hw_pin_init(LeapAm243GpioPin *out, uint32_t base_addr, uint32_t pin)
{
    out->base_addr = (uint32_t)AddrTranslateP_getLocalAddr(base_addr);
    out->pin       = pin;
}

static void leap_hw_gpio_write(const LeapAm243GpioPin *gpio, uint8_t value)
{
    if (value != 0u) {
        GPIO_pinWriteHigh(gpio->base_addr, gpio->pin);
    } else {
        GPIO_pinWriteLow(gpio->base_addr, gpio->pin);
    }
}

void leap_hw_eth_assert_icssg_path(void)
{
    /* Intentionally no-op: Ethernet mux/reset pins are left to the board defaults. */
}

void leap_hw_eth_bringup(void)
{
    /* Intentionally no-op: LEAP GPIOs are header I/O only, never Ethernet control. */
}

void leap_hw_init(void)
{
    leap_hw_pin_init(&g_outputs[0], LEAP_GPIO_DO0_BASE_ADDR, LEAP_GPIO_DO0_PIN);
    leap_hw_pin_init(&g_outputs[1], LEAP_GPIO_DO1_BASE_ADDR, LEAP_GPIO_DO1_PIN);
    leap_hw_pin_init(&g_outputs[2], LEAP_GPIO_DO2_BASE_ADDR, LEAP_GPIO_DO2_PIN);
    leap_hw_pin_init(&g_outputs[3], LEAP_GPIO_DO3_BASE_ADDR, LEAP_GPIO_DO3_PIN);
    leap_hw_pin_init(&g_outputs[4], LEAP_GPIO_DO4_BASE_ADDR, LEAP_GPIO_DO4_PIN);
    leap_hw_pin_init(&g_outputs[5], LEAP_GPIO_DO5_BASE_ADDR, LEAP_GPIO_DO5_PIN);
    leap_hw_pin_init(&g_outputs[6], LEAP_GPIO_DO6_BASE_ADDR, LEAP_GPIO_DO6_PIN);
    leap_hw_pin_init(&g_outputs[7], LEAP_GPIO_DO7_BASE_ADDR, LEAP_GPIO_DO7_PIN);

    leap_hw_pin_init(&g_inputs[0], LEAP_GPIO_DI0_BASE_ADDR, LEAP_GPIO_DI0_PIN);
    leap_hw_pin_init(&g_inputs[1], LEAP_GPIO_DI1_BASE_ADDR, LEAP_GPIO_DI1_PIN);
    leap_hw_pin_init(&g_inputs[2], LEAP_GPIO_DI2_BASE_ADDR, LEAP_GPIO_DI2_PIN);
    leap_hw_pin_init(&g_inputs[3], LEAP_GPIO_DI3_BASE_ADDR, LEAP_GPIO_DI3_PIN);
    leap_hw_pin_init(&g_inputs[4], LEAP_GPIO_DI4_BASE_ADDR, LEAP_GPIO_DI4_PIN);
    leap_hw_pin_init(&g_inputs[5], LEAP_GPIO_DI5_BASE_ADDR, LEAP_GPIO_DI5_PIN);
    leap_hw_pin_init(&g_inputs[6], LEAP_GPIO_DI6_BASE_ADDR, LEAP_GPIO_DI6_PIN);
    leap_hw_pin_init(&g_inputs[7], LEAP_GPIO_DI7_BASE_ADDR, LEAP_GPIO_DI7_PIN);

    for (uint32_t i = 0U; i < 8U; i++) {
        leap_hw_gpio_write(&g_outputs[i], 0U);
    }

    g_applied_outputs = 0U;
    LEAP_AM243_LOG_INFO("LEAP AM243 GPIO: 8 header outputs, 8 header inputs\r\n");
}

void leap_hw_refresh_inputs(LeapAm243IoShadow *io)
{
    uint16_t inputs = 0U;

    if (io == NULL) {
        return;
    }

    for (uint32_t i = 0U; i < 8U; i++) {
        if (GPIO_pinRead(g_inputs[i].base_addr, g_inputs[i].pin) != 0U) {
            inputs |= (uint16_t)(1U << i);
        }
    }

    io->digital_inputs = inputs;
}

void leap_hw_apply_outputs(LeapAm243IoShadow *io, uint16_t outputs)
{
    if (io == NULL) {
        return;
    }

    io->digital_outputs = outputs;
    if (outputs == g_applied_outputs) {
        return;
    }

    for (uint32_t i = 0U; i < 8U; i++) {
        leap_hw_gpio_write(&g_outputs[i],
                           ((outputs & (uint16_t)(1U << i)) != 0U) ? 1U : 0U);
    }

    g_applied_outputs = outputs;
#if LEAP_DEVICE_HOST_TRACE_ENABLE
    LEAP_AM243_LOG_INFO("PD outputs=0x%04X\r\n", (unsigned)outputs);
#endif
}

void leap_hw_enter_safe(LeapAm243IoShadow *io)
{
    if (io == NULL) { return; }
    io->safe_active = 1u;
    leap_hw_apply_outputs(io, 0u);
}

void leap_hw_set_status_led(uint8_t on)
{
    leap_hw_gpio_write(&g_outputs[0], on);
}

uint64_t leap_hw_monotonic_us(void)
{
    return ClockP_getTimeUsec();
}
