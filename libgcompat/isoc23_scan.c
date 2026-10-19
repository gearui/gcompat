/* GearOS transitional C23 scanning compatibility. SPDX-License-Identifier: NCSA
 * Only formats with unchanged C99/C23 semantics are delegated to musl.
 * Binary/autodetected integers and C23 width modifiers are rejected explicitly
 * until their C23 parsing semantics are implemented. Never silently alias %i.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

static int supported_format(const char *p)
{
    for (; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == '%') continue;
        if (!*p) return 0;
        for (; *p; ++p) {
            if (*p == 'i' || *p == 'b' || *p == 'B' || *p == 'w') return 0;
            if (*p == '[') {
                ++p;
                if (*p == '^') ++p;
                if (*p == ']') ++p;
                while (*p && *p != ']') ++p;
                if (!*p) return 0;
                break;
            }
            if ((*p >= '0' && *p <= '9') || *p == '*' || *p == '$' ||
                *p == 'h' || *p == 'l' || *p == 'j' || *p == 'z' ||
                *p == 't' || *p == 'L' || *p == 'm') continue;
            if (*p == 'd' || *p == 'u' || *p == 'o' || *p == 'x' || *p == 'X' ||
                *p == 'a' || *p == 'A' || *p == 'e' || *p == 'E' || *p == 'f' ||
                *p == 'F' || *p == 'g' || *p == 'G' || *p == 'c' || *p == 's' ||
                *p == 'p' || *p == 'n') break;
            return 0;
        }
        if (!*p) return 0;
    }
    return 1;
}
int __isoc23_vsscanf(const char *s, const char *format, va_list ap)
{
    if (!supported_format(format)) { errno = ENOTSUP; return EOF; }
    return vsscanf(s, format, ap);
}
int __isoc23_sscanf(const char *s, const char *format, ...)
{
    va_list ap; va_start(ap, format);
    int result = __isoc23_vsscanf(s, format, ap);
    va_end(ap); return result;
}
int __isoc23_vfscanf(FILE *stream, const char *format, va_list ap)
{
    if (!supported_format(format)) { errno = ENOTSUP; return EOF; }
    return vfscanf(stream, format, ap);
}
int __isoc23_fscanf(FILE *stream, const char *format, ...)
{
    va_list ap; va_start(ap, format);
    int result = __isoc23_vfscanf(stream, format, ap);
    va_end(ap); return result;
}
int __isoc23_vscanf(const char *format, va_list ap)
{ return __isoc23_vfscanf(stdin, format, ap); }
int __isoc23_scanf(const char *format, ...)
{
    va_list ap; va_start(ap, format);
    int result = __isoc23_vscanf(format, ap);
    va_end(ap); return result;
}
