/*
 * clearcore_leap_locate.h
 *
 * LEAP DISC LOCATE_DEVICE — blink the ClearCore built-in LED (ConnectorLed).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CLEARCORE_LEAP_LOCATE_H_
#define CLEARCORE_LEAP_LOCATE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void clearcore_leap_locate_init(void);

void clearcore_leap_locate_start(uint32_t duration_us, uint8_t pattern, int cancel);

void clearcore_leap_locate_update(uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif /* CLEARCORE_LEAP_LOCATE_H_ */
