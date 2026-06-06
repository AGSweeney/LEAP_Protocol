// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#include "eth_init.h"
#include "leap_host.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/netif.h"

static const char *TAG = "main";

#define LEAP_TASK_STACK_BYTES 24576u
#define LEAP_TASK_PRIORITY    5u

static const char *reset_reason_str(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "interrupt-wdt";
    case ESP_RST_TASK_WDT:  return "task-wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
    }
}

static void leap_task(void *arg)
{
    (void)arg;

    while (1) {
        leap_host_cyclic();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    esp_netif_t *esp_netif;
    struct netif *lwip_netif;

    ESP_LOGI(TAG, "LEAP device — Kincony KC868-A16");
    ESP_LOGI(TAG, "reset reason: %s",
             reset_reason_str(esp_reset_reason()));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(eth_init_kc868());

    esp_netif = eth_get_netif();
    if (esp_netif == NULL) {
        ESP_LOGE(TAG, "Ethernet netif unavailable");
        return;
    }

    lwip_netif = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    if (lwip_netif == NULL) {
        ESP_LOGE(TAG, "lwIP netif unavailable");
        return;
    }

    if (leap_host_init(lwip_netif) != 0) {
        ESP_LOGE(TAG, "LEAP host init failed");
        return;
    }

    xTaskCreate(leap_task, "leap_host", LEAP_TASK_STACK_BYTES, NULL,
                LEAP_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "waiting for LEAP master");
}
