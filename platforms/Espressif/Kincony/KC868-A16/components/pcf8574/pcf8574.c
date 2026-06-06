// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "pcf8574.h"

#include "i2c_manager.h"

#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "pcf8574";
#define PCF8574_TIMEOUT_MS 100

struct pcf8574_handle {
    i2c_master_dev_handle_t dev_handle;
    uint8_t address;
};

esp_err_t pcf8574_init(const pcf8574_config_t *config, pcf8574_handle_t *handle)
{
    if (config == NULL || handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!i2c_manager_is_initialized()) {
        ESP_LOGE(TAG, "I2C manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_manager_get_bus(&bus_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    pcf8574_handle_t dev = (pcf8574_handle_t)malloc(sizeof(struct pcf8574_handle));
    if (dev == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->address,
        .scl_speed_hz = config->freq_hz,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add PCF8574 at 0x%02X: %s",
                 config->address, esp_err_to_name(ret));
        free(dev);
        return ret;
    }

    dev->address = config->address;
    *handle = dev;
    ESP_LOGI(TAG, "PCF8574 ready at 0x%02X", config->address);
    return ESP_OK;
}

esp_err_t pcf8574_deinit(pcf8574_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_handle_t bus_handle;
    if (i2c_manager_get_bus(&bus_handle) == ESP_OK) {
        i2c_master_bus_rm_device(handle->dev_handle);
    }

    free(handle);
    return ESP_OK;
}

esp_err_t pcf8574_read(pcf8574_handle_t handle, uint8_t *value)
{
    if (handle == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_receive(handle->dev_handle, value, 1, PCF8574_TIMEOUT_MS);
}

esp_err_t pcf8574_write(pcf8574_handle_t handle, uint8_t value)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_transmit(handle->dev_handle, &value, 1, PCF8574_TIMEOUT_MS);
}

esp_err_t pcf8574_scan(const uint8_t *expected_addresses, size_t num_addresses,
                       uint8_t *found_addresses, size_t *num_found)
{
    if (expected_addresses == NULL || num_addresses == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!i2c_manager_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_manager_get_bus(&bus_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_log_level_set("i2c.master", ESP_LOG_WARN);

    size_t found = 0;
    uint32_t bus_freq_hz = 400000u;
    (void)i2c_manager_get_freq(&bus_freq_hz);

    for (size_t i = 0; i < num_addresses; ++i) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = expected_addresses[i],
            .scl_speed_hz = bus_freq_hz,
        };
        i2c_master_dev_handle_t temp_handle = NULL;

        ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &temp_handle);
        if (ret == ESP_OK) {
            uint8_t test_value = 0xFFu;
            ret = i2c_master_transmit(temp_handle, &test_value, 1, 50);
            if (ret == ESP_OK) {
                if (found_addresses != NULL && found < num_addresses) {
                    found_addresses[found] = expected_addresses[i];
                }
                ++found;
            }
            i2c_master_bus_rm_device(temp_handle);
        }
    }

    esp_log_level_set("i2c.master", ESP_LOG_INFO);

    if (num_found != NULL) {
        *num_found = found;
    }

    return ESP_OK;
}
