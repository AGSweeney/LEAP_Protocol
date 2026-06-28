#ifndef OPENER_NB_NNDK_IO_H_
#define OPENER_NB_NNDK_IO_H_

#include <stdarg.h>

/* Declared in nbrtos/source/nbiprintf.cpp (C++ linkage). */
int iprintf(const char *format, ...);
int viprintf(const char *format, va_list arg);

#endif /* OPENER_NB_NNDK_IO_H_ */
