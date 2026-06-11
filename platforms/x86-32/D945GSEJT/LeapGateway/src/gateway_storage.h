/*
 * gateway_storage.h — Boot media mount for persisted gateway config.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_GATEWAY_STORAGE_H
#define LEAP_GATEWAY_STORAGE_H

int  leap_gateway_storage_init(void);
int  leap_gateway_storage_retry_after_pci(void);
int  leap_gateway_storage_ready(void);
const char* leap_gateway_storage_mount_point(void);

#endif /* LEAP_GATEWAY_STORAGE_H */
