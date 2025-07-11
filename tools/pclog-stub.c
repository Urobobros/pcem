#include <stdio.h>
#include <stdarg.h>

void pclog(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}
