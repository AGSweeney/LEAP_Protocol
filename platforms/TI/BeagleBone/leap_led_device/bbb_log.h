#ifndef BBB_LOG_H
#define BBB_LOG_H

/*
 * BeagleBone UART logging — warnings and errors only by default.
 *
 * Build with -DLEAP_DEVICE_HOST_TRACE_FORCE to enable BBB_LOG_INFO trace
 * (boot banner, MAC, state changes, PD outputs, SAFE transitions).
 */

#ifndef LEAP_DEVICE_HOST_TRACE_FORCE
#define LEAP_DEVICE_HOST_TRACE_ENABLE 0
#endif

#include "leap/leap_device_host_perf.h"

#include "bbb_hw.h"

static inline void bbb_log_error(const char* msg)
{
    bbb_uart_puts("LEAP ERR: ");
    bbb_uart_puts(msg);
    bbb_uart_puts("\n");
}

static inline void bbb_log_warn(const char* msg)
{
    bbb_uart_puts("LEAP WRN: ");
    bbb_uart_puts(msg);
    bbb_uart_puts("\n");
}

static inline void bbb_log_warn_u16(const char* prefix, uint16_t value)
{
    bbb_uart_puts("LEAP WRN: ");
    bbb_uart_puts(prefix);
    bbb_uart_put_hex16(value);
    bbb_uart_puts("\n");
}

#if LEAP_DEVICE_HOST_TRACE_ENABLE
static inline void bbb_log_info(const char* msg)
{
    bbb_uart_puts(msg);
    bbb_uart_puts("\n");
}
#else
static inline void bbb_log_info(const char* msg)
{
    (void)msg;
}
#endif

#endif /* BBB_LOG_H */
