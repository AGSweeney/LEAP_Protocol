/*
 * gateway_storage.h - Persisted gateway config in NetBurner NNDK config storage.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_STORAGE_H
#define LEAP_GATEWAY_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

int leap_gateway_storage_init(void);
int leap_gateway_storage_retry_after_pci(void);
int leap_gateway_storage_ready(void);
const char* leap_gateway_storage_mount_point(void);

struct LeapGatewayConfig;
int leap_gateway_storage_load_config(struct LeapGatewayConfig* config);
int leap_gateway_storage_save_config(const struct LeapGatewayConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_GATEWAY_STORAGE_H */
