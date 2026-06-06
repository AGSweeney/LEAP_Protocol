// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "i2c_manager.h"

#include "esp_log.h"

static const char *TAG = "i2c_manager";

static i2c_master_bus_handle_t s_bus_handle = NULL;
static bool s_initialized = false;
static uint32_t s_bus_freq_hz = 0;

esp_err_t i2c_manager_init(int sda_gpio, int scl_gpio, uint32_t freq_hz)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "I2C manager already initialized");
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    s_bus_freq_hz = freq_hz;
    s_initialized = true;
    ESP_LOGI(TAG, "I2C ready (SDA GPIO%d, SCL GPIO%d, %lu Hz)",
             sda_gpio, scl_gpio, (unsigned long)freq_hz);

    return ESP_OK;
}

esp_err_t i2c_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = i2c_del_master_bus(s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    s_bus_handle = NULL;
    s_bus_freq_hz = 0;
    s_initialized = false;
    return ESP_OK;
}

esp_err_t i2c_manager_get_bus(i2c_master_bus_handle_t *bus_handle)
{
    if (!s_initialized || s_bus_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *bus_handle = s_bus_handle;
    return ESP_OK;
}

bool i2c_manager_is_initialized(void)
{
    return s_initialized;
}

esp_err_t i2c_manager_get_freq(uint32_t *freq_hz)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (freq_hz == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *freq_hz = s_bus_freq_hz;
    return ESP_OK;
}
