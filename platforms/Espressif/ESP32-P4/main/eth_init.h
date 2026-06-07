#pragma once

#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t leap_eth_init(void);
esp_netif_t *eth_get_netif(void);

#ifdef __cplusplus
}
#endif
