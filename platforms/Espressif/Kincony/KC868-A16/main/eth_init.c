// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "eth_init.h"
#include "board_config.h"

#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "eth_init";

static esp_netif_t      *s_eth_netif  = NULL;
static esp_eth_handle_t  s_eth_handle = NULL;

static esp_err_t eth_apply_l2_only_ip(esp_netif_t *netif)
{
    uint8_t mac[6] = {0};
    esp_netif_ip_info_t ip = {0};
    esp_err_t ret;

    if (netif == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_netif_get_mac(netif, mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAC read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ip.ip.addr      = ESP_IP4TOADDR(169, 254, mac[4], mac[5]);
    ip.netmask.addr = ESP_IP4TOADDR(255, 255, 0, 0);
    ip.gw.addr      = 0;

    ret = esp_netif_set_ip_info(netif, &ip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "link-local IP set failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "L2-only link-local IP: " IPSTR, IP2STR(&ip.ip));
    return ESP_OK;
}

static void on_eth_event(void *arg, esp_event_base_t event_base,
                         int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet link up");
        if (s_eth_netif != NULL) {
            (void)eth_apply_l2_only_ip(s_eth_netif);
        }
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet link down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet driver started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet driver stopped");
        break;
    default:
        break;
    }
}

esp_err_t eth_init_kc868(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Waiting for PHY power/clock stabilization...");
    vTaskDelay(pdMS_TO_TICKS(200));

    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_cfg.smi_gpio.mdc_num  = ETH_MDC_PIN;
    emac_cfg.smi_gpio.mdio_num = ETH_MDIO_PIN;
    emac_cfg.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    emac_cfg.clock_config.rmii.clock_gpio = (emac_rmii_clock_gpio_t)ETH_CLK_PIN;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create ESP32 EMAC");
        return ESP_FAIL;
    }

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.phy_addr         = ETH_PHY_ADDR;
    phy_cfg.reset_gpio_num   = -1;
    phy_cfg.reset_timeout_ms = 500;

    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_cfg);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create LAN8720 PHY");
        return ESP_FAIL;
    }

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&eth_cfg, &s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    if (!s_eth_netif) {
        ESP_LOGE(TAG, "Failed to create default ethernet netif");
        return ESP_FAIL;
    }

    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(s_eth_handle);
    ret = esp_netif_attach(s_eth_netif, glue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Netif attach failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_dhcpc_stop(s_eth_netif);
    if (ret != ESP_OK && ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "DHCP client stop failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                     &on_eth_event, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG,
             "LAN8720 Ethernet initialized (PHY addr %d, CLK out GPIO%d, L2 only)",
             ETH_PHY_ADDR, ETH_CLK_PIN);
    return ESP_OK;
}

esp_netif_t *eth_get_netif(void)
{
    return s_eth_netif;
}
