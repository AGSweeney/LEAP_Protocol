/*
 * leap_time.h — Monotonic time for RTEMS LEAP device stack.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef LEAP_RTEMS_TIME_H
#define LEAP_RTEMS_TIME_H

#include <stdint.h>

uint64_t leap_rtems_monotonic_us(void);

/*
 * Returns a pointer to a static buffer holding the current uptime formatted as
 * "ssss.mmm" (seconds.milliseconds), right-aligned. Intended for single-threaded
 * boot/serial logging only.
 */
const char *leap_rtems_uptime_str(void);

#endif /* LEAP_RTEMS_TIME_H */
