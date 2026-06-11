/*
 * clearcore_leap_identity.h
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CLEARCORE_LEAP_IDENTITY_H_
#define CLEARCORE_LEAP_IDENTITY_H_

#include <stdint.h>

/* Teknic ClearCore LEAP device identity (HELLO / IDENTIFY / DIR). */
#define CLEARCORE_LEAP_VENDOR_ID         0x544Bu /* 'T','K' */
#define CLEARCORE_LEAP_PRODUCT_CODE      0x434C4301u /* CLC1 */
#define CLEARCORE_LEAP_FIRMWARE_REVISION 1u

#ifdef __cplusplus
extern "C" {
#endif

struct LeapDeviceStack;
struct netif;

void clearcore_leap_identity_apply(
    struct LeapDeviceStack *stack,
    struct netif *          netif);

#ifdef __cplusplus
}
#endif

#endif /* CLEARCORE_LEAP_IDENTITY_H_ */
