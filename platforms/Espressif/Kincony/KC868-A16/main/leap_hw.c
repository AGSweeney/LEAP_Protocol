// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "leap_hw.h"
#include "board_config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "i2c_manager.h"
#include "leap/leap_protocol.h"
#include "pcf8574.h"

#include <string.h>

static const char *TAG = "leap_hw";

static pcf8574_handle_t s_inputs_1_8   = NULL;
static pcf8574_handle_t s_inputs_9_16  = NULL;
static pcf8574_handle_t s_outputs_1_8  = NULL;
static pcf8574_handle_t s_outputs_9_16 = NULL;
static int                s_io_ready   = 0;

static void locate_led_init(void)
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << LOCATE_LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&led_cfg);
    leap_hw_set_locate_led(0);
    ESP_LOGI(TAG, "locate LED on GPIO%d (IR transmitter)", LOCATE_LED_PIN);
}

static int pcf8574_bringup(void)
{
    const uint8_t expected[] = {
        PCF8574_ADDR_INPUTS_1_8,
        PCF8574_ADDR_INPUTS_9_16,
        PCF8574_ADDR_OUTPUTS_1_8,
        PCF8574_ADDR_OUTPUTS_9_16,
    };
    const char *names[] = {
        "inputs X01-X08",
        "inputs X09-X16",
        "outputs Y01-Y08",
        "outputs Y09-Y16",
    };
    pcf8574_config_t cfg = {
        .freq_hz = I2C_FREQ_HZ,
    };
    size_t found = 0;
    uint8_t found_addrs[4] = {0};

    if (i2c_manager_init(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ) != ESP_OK) {
        ESP_LOGE(TAG, "I2C manager init failed");
        return -1;
    }

    if (pcf8574_scan(expected, sizeof(expected), found_addrs, &found) == ESP_OK) {
        for (size_t i = 0; i < sizeof(expected); ++i) {
            int present = 0;

            for (size_t j = 0; j < found; ++j) {
                if (found_addrs[j] == expected[i]) {
                    present = 1;
                    break;
                }
            }

            ESP_LOGI(TAG, "PCF8574 0x%02X %s: %s",
                     expected[i], names[i], present ? "OK" : "missing");
        }
    }

    cfg.address = PCF8574_ADDR_INPUTS_1_8;
    if (pcf8574_init(&cfg, &s_inputs_1_8) != ESP_OK) {
        return -1;
    }

    cfg.address = PCF8574_ADDR_INPUTS_9_16;
    if (pcf8574_init(&cfg, &s_inputs_9_16) != ESP_OK) {
        return -1;
    }

    cfg.address = PCF8574_ADDR_OUTPUTS_1_8;
    if (pcf8574_init(&cfg, &s_outputs_1_8) != ESP_OK) {
        return -1;
    }

    cfg.address = PCF8574_ADDR_OUTPUTS_9_16;
    if (pcf8574_init(&cfg, &s_outputs_9_16) != ESP_OK) {
        return -1;
    }

    return 0;
}

static void relays_all_off(void)
{
    const uint8_t off_value = 0xFFu;

    if (s_outputs_1_8 != NULL) {
        (void)pcf8574_write(s_outputs_1_8, off_value);
    }
    if (s_outputs_9_16 != NULL) {
        (void)pcf8574_write(s_outputs_9_16, off_value);
    }
}

static void write_relays(uint16_t outputs)
{
    uint8_t bank_lo;
    uint8_t bank_hi;

    if (!s_io_ready) {
        return;
    }

    bank_lo = (uint8_t)(outputs & 0xFFu);
    bank_hi = (uint8_t)((outputs >> 8) & 0xFFu);

#if PCF8574_INVERTED
    bank_lo = (uint8_t)~bank_lo;
    bank_hi = (uint8_t)~bank_hi;
#endif

    if (pcf8574_write(s_outputs_1_8, bank_lo) != ESP_OK) {
        ESP_LOGW(TAG, "failed to write outputs Y01-Y08");
    }
    if (pcf8574_write(s_outputs_9_16, bank_hi) != ESP_OK) {
        ESP_LOGW(TAG, "failed to write outputs Y09-Y16");
    }
}

void leap_hw_init(void)
{
    locate_led_init();

    if (pcf8574_bringup() == 0) {
        s_io_ready = 1;
        relays_all_off();
        ESP_LOGI(TAG, "16 relay outputs + 16 digital inputs ready (I2C GPIO%d/%d)",
                 I2C_SDA_PIN, I2C_SCL_PIN);
    } else {
        ESP_LOGW(TAG, "PCF8574 init incomplete — PD I/O unavailable");
    }
}

void leap_hw_refresh_inputs(LeapKc868IoShadow *io)
{
    uint8_t bank_lo = 0xFFu;
    uint8_t bank_hi = 0xFFu;
    uint16_t inputs = 0u;

    if (io == NULL) {
        return;
    }

    if (!s_io_ready) {
        io->digital_inputs = 0u;
        return;
    }

    if (pcf8574_read(s_inputs_1_8, &bank_lo) != ESP_OK) {
        bank_lo = 0xFFu;
    }
    if (pcf8574_read(s_inputs_9_16, &bank_hi) != ESP_OK) {
        bank_hi = 0xFFu;
    }

#if PCF8574_INVERTED
    bank_lo = (uint8_t)~bank_lo;
    bank_hi = (uint8_t)~bank_hi;
#endif

    inputs = (uint16_t)bank_lo | ((uint16_t)bank_hi << 8);
    io->digital_inputs = inputs;
}

void leap_hw_apply_outputs(LeapKc868IoShadow *io, uint16_t outputs)
{
    uint16_t prev;

    if (io == NULL) {
        return;
    }

    prev = io->digital_outputs;
    io->safe_active     = 0;
    io->digital_outputs = outputs;
    leap_hw_refresh_inputs(io);
    io->io_status = LEAP_DIO_STATUS_OK;

    write_relays(outputs);

    if (prev != outputs) {
        ESP_LOGI(TAG, "relay outputs=0x%04X inputs=0x%04X",
                 outputs, io->digital_inputs);
    }
}

void leap_hw_enter_safe(LeapKc868IoShadow *io)
{
    if (io == NULL) {
        return;
    }

    io->safe_active     = 1;
    io->digital_outputs = io->safe_outputs;
    relays_all_off();
    leap_hw_refresh_inputs(io);

    ESP_LOGW(TAG, "safe state: relays off, PD outputs=0x%04X", io->digital_outputs);
}

void leap_hw_set_locate_led(uint8_t on)
{
#if LOCATE_LED_INVERT
    gpio_set_level(LOCATE_LED_PIN, on != 0u ? 0 : 1);
#else
    gpio_set_level(LOCATE_LED_PIN, on != 0u ? 1 : 0);
#endif
}
