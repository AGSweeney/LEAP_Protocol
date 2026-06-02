#include <stddef.h>

void* memset(void* dest, int value, size_t count)
{
    unsigned char* out = (unsigned char*)dest;

    while (count-- != 0u) {
        *out++ = (unsigned char)value;
    }

    return dest;
}

void* memcpy(void* dest, const void* src, size_t count)
{
    unsigned char*       out = (unsigned char*)dest;
    const unsigned char* in  = (const unsigned char*)src;

    while (count-- != 0u) {
        *out++ = *in++;
    }

    return dest;
}

int memcmp(const void* lhs, const void* rhs, size_t count)
{
    const unsigned char* a = (const unsigned char*)lhs;
    const unsigned char* b = (const unsigned char*)rhs;

    while (count-- != 0u) {
        if (*a != *b) {
            return (int)*a - (int)*b;
        }
        a++;
        b++;
    }

    return 0;
}
