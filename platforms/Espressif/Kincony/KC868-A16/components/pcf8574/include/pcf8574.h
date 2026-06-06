// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pcf8574_handle *pcf8574_handle_t;

typedef struct {
    uint8_t address;
    uint32_t freq_hz;
} pcf8574_config_t;

esp_err_t pcf8574_init(const pcf8574_config_t *config, pcf8574_handle_t *handle);
esp_err_t pcf8574_deinit(pcf8574_handle_t handle);
esp_err_t pcf8574_read(pcf8574_handle_t handle, uint8_t *value);
esp_err_t pcf8574_write(pcf8574_handle_t handle, uint8_t value);
esp_err_t pcf8574_scan(const uint8_t *expected_addresses, size_t num_addresses,
                       uint8_t *found_addresses, size_t *num_found);

#ifdef __cplusplus
}
#endif
