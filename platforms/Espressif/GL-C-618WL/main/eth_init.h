// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>

#pragma once

#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t eth_init_gledopto(void);
esp_netif_t *eth_get_netif(void);

#ifdef __cplusplus
}
#endif
