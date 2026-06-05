// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t led_output_init_channel(uint8_t channel, int gpio_num, uint16_t num_leds);
esp_err_t led_output_show_channel(uint8_t channel, const uint8_t *pixels, uint16_t num_leds);
esp_err_t led_output_clear_channel(uint8_t channel, uint16_t num_leds);
esp_err_t led_output_clear_all(uint16_t num_leds);

#ifdef __cplusplus
}
#endif
