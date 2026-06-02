#ifndef BBB_EXC_H
#define BBB_EXC_H

#include <stdint.h>

void bbb_exc_hang(char code, uint32_t aux0, uint32_t aux1);

#endif
