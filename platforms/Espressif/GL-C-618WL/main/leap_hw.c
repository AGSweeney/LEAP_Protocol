// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "leap_hw.h"
#include "board_config.h"
#include "led_output.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "leap/leap_protocol.h"

#include <string.h>

static const char *TAG = "leap_hw";

#define LEAP_LED_LEVEL      LEAP_LED_MAX_LEVEL
#define LEAP_LED_WHITE_LVL  LEAP_LED_WHITE_MAX

static uint16_t s_num_leds = DEFAULT_NUM_LEDS;
static uint8_t  s_pixels[DEFAULT_NUM_LEDS * 3u];
static uint8_t  s_ch_active[LED_CHANNEL_COUNT];

static char leap_hw_bit_char(uint16_t mask, uint8_t bit)
{
    return ((mask & (1u << bit)) != 0u) ? '1' : '-';
}

static void leap_hw_log_output_mask(uint16_t outputs)
{
    ESP_LOGI(TAG,
             "strip outputs=0x%04X  CH0[RGWw]=%c%c%c%c  CH1[RGWw]=%c%c%c%c  "
             "(max level=0x%02X)",
             outputs,
             leap_hw_bit_char(outputs, LEAP_DO_CH0_RED_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH0_GREEN_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH0_BLUE_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH0_WHITE_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH1_RED_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH1_GREEN_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH1_BLUE_BIT),
             leap_hw_bit_char(outputs, LEAP_DO_CH1_WHITE_BIT),
             (unsigned)LEAP_LED_MAX_LEVEL);
}

static void status_led_init(void)
{
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << STATUS_LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&led_cfg);
    leap_hw_set_status_led(0);
    ESP_LOGI(TAG, "status LED on GPIO%d (locate / identify)", STATUS_LED_PIN);
}

static void relay_power_rail_enable(void)
{
    gpio_config_t relay_cfg = {
        .pin_bit_mask = (1ULL << RELAY_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    gpio_config(&relay_cfg);

#if RELAY_INVERT
    gpio_set_level(RELAY_PIN, 0);
#else
    gpio_set_level(RELAY_PIN, 1);
#endif

    ESP_LOGI(TAG, "LED power rail enabled (relay GPIO%d)", RELAY_PIN);
}

static void fill_strip_color(uint16_t num_leds, uint8_t g, uint8_t r, uint8_t b)
{
    uint16_t i;

    for (i = 0; i < num_leds; ++i) {
        s_pixels[(i * 3u) + 0u] = g;
        s_pixels[(i * 3u) + 1u] = r;
        s_pixels[(i * 3u) + 2u] = b;
    }
}

static void apply_strip_presets(uint8_t channel, uint16_t outputs,
                                uint8_t red_bit, uint8_t green_bit,
                                uint8_t blue_bit, uint8_t white_bit)
{
    uint8_t g = 0;
    uint8_t r = 0;
    uint8_t b = 0;
    int     has_color = 0;

    if ((outputs & (1u << red_bit)) != 0u) {
        r = LEAP_LED_LEVEL;
        has_color = 1;
    }
    if ((outputs & (1u << green_bit)) != 0u) {
        g = LEAP_LED_LEVEL;
        has_color = 1;
    }
    if ((outputs & (1u << blue_bit)) != 0u) {
        b = LEAP_LED_LEVEL;
        has_color = 1;
    }
    if ((outputs & (1u << white_bit)) != 0u) {
        g = LEAP_LED_WHITE_LVL;
        r = LEAP_LED_WHITE_LVL;
        b = LEAP_LED_WHITE_LVL;
        has_color = 1;
    }

    if (!has_color) {
        if (s_ch_active[channel] != 0u) {
            s_ch_active[channel] = 0u;
            (void)led_output_clear_channel(channel, s_num_leds);
        }
        return;
    }

    s_ch_active[channel] = 1u;
    fill_strip_color(s_num_leds, g, r, b);
    (void)led_output_show_channel(channel, s_pixels, s_num_leds);
}

void leap_hw_init(uint16_t num_leds)
{
    if (num_leds > DEFAULT_NUM_LEDS) {
        num_leds = DEFAULT_NUM_LEDS;
    }

    s_num_leds = num_leds;
    memset(s_ch_active, 0, sizeof(s_ch_active));
    status_led_init();
    relay_power_rail_enable();

    ESP_ERROR_CHECK(led_output_init_channel(0, LED_DATA_PIN_0, num_leds));
    ESP_ERROR_CHECK(led_output_init_channel(1, LED_DATA_PIN_1, num_leds));

    (void)led_output_clear_all(s_num_leds);

    ESP_LOGI(TAG, "exposed outputs: CH0 GPIO%d, CH1 GPIO%d (%u LEDs, master PD only)",
             LED_DATA_PIN_0, LED_DATA_PIN_1, (unsigned)s_num_leds);
}

void leap_hw_refresh_inputs(LeapGlc618wlIoShadow *io)
{
    if (io == NULL) {
        return;
    }

    io->digital_inputs = 0u;
}

void leap_hw_apply_outputs(LeapGlc618wlIoShadow *io, uint16_t outputs)
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

    if (outputs == 0u) {
        if (prev != 0u) {
            (void)led_output_clear_all(s_num_leds);
            ESP_LOGI(TAG, "strip cleared (outputs=0x0000)");
        }
        return;
    }

    apply_strip_presets(0, outputs,
                        LEAP_DO_CH0_RED_BIT, LEAP_DO_CH0_GREEN_BIT,
                        LEAP_DO_CH0_BLUE_BIT, LEAP_DO_CH0_WHITE_BIT);
    apply_strip_presets(1, outputs,
                        LEAP_DO_CH1_RED_BIT, LEAP_DO_CH1_GREEN_BIT,
                        LEAP_DO_CH1_BLUE_BIT, LEAP_DO_CH1_WHITE_BIT);

    if (prev != outputs) {
        leap_hw_log_output_mask(io->digital_outputs);
    }
}

void leap_hw_enter_safe(LeapGlc618wlIoShadow *io)
{
    if (io == NULL) {
        return;
    }

    io->safe_active     = 1;
    io->digital_outputs = io->safe_outputs;
    memset(s_ch_active, 0, sizeof(s_ch_active));
    (void)led_output_clear_all(s_num_leds);

    ESP_LOGW(TAG, "safe state: strips off, PD outputs=0x%04X", io->digital_outputs);
}

void leap_hw_set_status_led(uint8_t on)
{
#if STATUS_LED_INVERT
    gpio_set_level(STATUS_LED_PIN, on != 0u ? 0 : 1);
#else
    gpio_set_level(STATUS_LED_PIN, on != 0u ? 1 : 0);
#endif
}
