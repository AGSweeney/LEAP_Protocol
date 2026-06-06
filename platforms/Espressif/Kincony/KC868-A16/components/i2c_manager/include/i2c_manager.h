// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_manager_init(int sda_gpio, int scl_gpio, uint32_t freq_hz);
esp_err_t i2c_manager_deinit(void);
esp_err_t i2c_manager_get_bus(i2c_master_bus_handle_t *bus_handle);
bool i2c_manager_is_initialized(void);
esp_err_t i2c_manager_get_freq(uint32_t *freq_hz);

#ifdef __cplusplus
}
#endif
