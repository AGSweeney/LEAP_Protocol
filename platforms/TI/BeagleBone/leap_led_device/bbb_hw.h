#ifndef BBB_HW_H
#define BBB_HW_H

#include "leap/leap_protocol.h"

#include <stddef.h>
#include <stdint.h>

#define BBB_LEAP_DO_COUNT 8u
#define BBB_LEAP_DI_COUNT 8u
#define BBB_LEAP_PROFILE_ID LEAP_PROFILE_DIGITAL_IO_8X8

void bbb_wdt_disable_all(void);
void bbb_uart_putc(uint8_t ch);
void bbb_uart_puts(const char* text);
void bbb_uart_put_hex8(uint8_t value);
void bbb_uart_put_hex16(uint16_t value);
void bbb_uart_put_hex32(uint32_t value);
void bbb_leds_init(void);
void bbb_status_leds_apply(uint8_t status_mask);
void bbb_gpio_outputs_apply(uint16_t outputs);
uint16_t bbb_gpio_inputs_read(void);
void bbb_leds_apply(uint16_t outputs);
void bbb_delay(volatile uint32_t count);
void bbb_timer_init(void);
uint64_t bbb_monotonic_us(void);

#endif
