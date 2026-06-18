/*
 * leap_time.h - Monotonic time for NetBurner LEAP Gateway.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_TIME_H
#define LEAP_RTEMS_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t leap_rtems_monotonic_us(void);
const char* leap_rtems_uptime_str(void);

#ifdef __cplusplus
}
#endif

#endif /* LEAP_RTEMS_TIME_H */
