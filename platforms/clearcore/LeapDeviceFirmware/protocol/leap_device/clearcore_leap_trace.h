/*
 * clearcore_leap_trace.h
 *
 * USB logging helpers — use ConnectorUsb from the main loop only (EtherCAT pattern).
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#ifndef CLEARCORE_LEAP_TRACE_H_
#define CLEARCORE_LEAP_TRACE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void clearcore_leap_trace_queue(const char *line);
void clearcore_leap_trace_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* CLEARCORE_LEAP_TRACE_H_ */
