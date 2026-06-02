#include "bbb_exc.h"
#include "bbb_hw.h"

void bbb_exc_hang(char code, uint32_t aux0, uint32_t aux1)
{
    bbb_uart_putc(code);
    bbb_uart_putc(':');
    bbb_uart_put_hex32(aux0);
    bbb_uart_putc(':');
    bbb_uart_put_hex32(aux1);
    bbb_uart_puts("\n");

    while (1) {
        __asm__ volatile("wfi");
    }
}
