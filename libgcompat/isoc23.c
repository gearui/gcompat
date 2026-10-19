/* GearOS downstream glibc C23 integer conversion compatibility.
 * SPDX-License-Identifier: NCSA
 * Keep the legacy entry points unchanged: only C23 accepts the 0b prefix.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

static const char *binary_digits(const char *text, int base, int *negative)
{
	const unsigned char *p = (const unsigned char *)text;
	if (base != 0 && base != 2) return NULL;
	while (isspace(*p)) ++p;
	*negative = *p == '-';
	if (*p == '-' || *p == '+') ++p;
	if (p[0] != '0' || (p[1] != 'b' && p[1] != 'B')) return NULL;
	if (p[2] != '0' && p[2] != '1') return NULL;
	return (const char *)p + 2;
}

static unsigned long long binary_value(const char *p, char **end,
                                      unsigned long long limit, int *overflow)
{
	unsigned long long value = 0;
	*overflow = 0;
	while (*p == '0' || *p == '1') {
		unsigned int digit = (unsigned int)(*p++ - '0');
		if (value > (limit - digit) / 2) *overflow = 1;
		if (!*overflow) value = value * 2 + digit;
	}
	if (end) *end = (char *)p;
	if (*overflow) { errno = ERANGE; return limit; }
	return value;
}

/* musl may report EINVAL for no conversion; glibc preserves errno in that
 * case. Invalid bases still use the underlying implementation's EINVAL. */
#define LEGACY_CALL(type, function) \
	if (base != 0 && (base < 2 || base > 36)) { errno = EINVAL; return 0; } \
	char *parsed = (char *)text; \
	int saved_errno = errno; \
	type result = function(text, &parsed, base); \
	if ((base == 0 || (base >= 2 && base <= 36)) && parsed == text && errno == EINVAL) \
		errno = saved_errno; \
	if (end) *end = parsed; \
	return result

#define SIGNED_C23(type, name, legacy, maximum, minimum) \
 type name(const char *text, char **end, int base) { \
	int negative, overflow; \
	const char *p = binary_digits(text, base, &negative); \
	if (!p) { LEGACY_CALL(type, legacy); } \
	unsigned long long limit = (unsigned long long)(maximum) + (negative ? 1ULL : 0ULL); \
	unsigned long long value = binary_value(p, end, limit, &overflow); \
	if (overflow) return negative ? (minimum) : (maximum); \
	if (negative && value == (unsigned long long)(maximum) + 1ULL) return (minimum); \
	return negative ? -(type)value : (type)value; \
 }

#define UNSIGNED_C23(type, name, legacy, maximum) \
 type name(const char *text, char **end, int base) { \
	int negative, overflow; \
	const char *p = binary_digits(text, base, &negative); \
	if (!p) { LEGACY_CALL(type, legacy); } \
	unsigned long long value = binary_value(p, end, (maximum), &overflow); \
	if (overflow) return (maximum); \
	return negative ? (type)(0 - (type)value) : (type)value; \
 }

SIGNED_C23(long, __isoc23_strtol, strtol, LONG_MAX, LONG_MIN)
SIGNED_C23(long long, __isoc23_strtoll, strtoll, LLONG_MAX, LLONG_MIN)
UNSIGNED_C23(unsigned long, __isoc23_strtoul, strtoul, ULONG_MAX)
UNSIGNED_C23(unsigned long long, __isoc23_strtoull, strtoull, ULLONG_MAX)

/* Locale objects are produced by this process's musl locale API. */
#include <locale.h>
#define LOCALE_C23(type, name, plain) \
 type name(const char *text, char **end, int base, locale_t locale) { \
    locale_t previous = uselocale(locale); \
    if (!previous) return 0; \
    type result = plain(text, end, base); \
    int error = errno; \
    uselocale(previous); errno = error; return result; \
 }
LOCALE_C23(long, __isoc23_strtol_l, __isoc23_strtol)
LOCALE_C23(unsigned long, __isoc23_strtoul_l, __isoc23_strtoul)
LOCALE_C23(long long, __isoc23_strtoll_l, __isoc23_strtoll)
LOCALE_C23(unsigned long long, __isoc23_strtoull_l, __isoc23_strtoull)

/* C23 size/alignment arguments are caller guarantees, not allocator metadata.
 * musl free already accepts every allocation from the matching allocator. */
void free_sized(void *pointer, size_t size) { (void)size; free(pointer); }
void free_aligned_sized(void *pointer, size_t alignment, size_t size)
{ (void)alignment; (void)size; free(pointer); }

#include <unistd.h>
#include <sys/syscall.h>
int close_range(unsigned int first, unsigned int last, int flags)
{
#ifdef SYS_close_range
    return (int)syscall(SYS_close_range, first, last, flags);
#else
    errno = ENOSYS; return -1;
#endif
}

#include <stdint.h>
/* Use kernel entropy; never fall back to a predictable PRNG. */
void arc4random_buf(void *buffer, size_t length)
{
    unsigned char *p = buffer;
    while (length) {
        size_t chunk = length > 256 ? 256 : length;
        if (getentropy(p, chunk) != 0) abort();
        p += chunk; length -= chunk;
    }
}
uint32_t arc4random(void)
{ uint32_t value; arc4random_buf(&value, sizeof value); return value; }
uint32_t arc4random_uniform(uint32_t upper)
{
    if (upper < 2) return 0;
    uint32_t threshold = -upper % upper, value;
    do { value = arc4random(); } while (value < threshold);
    return value % upper;
}

ssize_t __readlinkat_chk(int fd, const char *path, char *buffer,
                       size_t length, size_t capacity)
{
    if (length > capacity) abort();
    return readlinkat(fd, path, buffer, length);
}

#include <fcntl.h>
int __openat64_2(int fd, const char *path, int flags)
{
    if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) abort();
    return openat(fd, path, flags);
}
#include <wchar.h>
#include <wctype.h>
#include <string.h>
size_t __mbsrtowcs_chk(wchar_t *dest, const char **src, size_t length,
                      mbstate_t *state, size_t capacity)
{
    if (dest && length > capacity) abort();
    return mbsrtowcs(dest, src, length, state);
}
size_t __strlcpy_chk(char *dest, const char *src, size_t length, size_t capacity)
{
    if (length > capacity) abort();
    return strlcpy(dest, src, length);
}
long __isoc23_wcstol(const wchar_t *text, wchar_t **end, int base)
{
    const wchar_t *p = text;
    while (iswspace(*p)) ++p;
    int negative = *p == L'-';
    if (*p == L'+' || *p == L'-') ++p;
    if ((base == 0 || base == 2) && p[0] == L'0' &&
        (p[1] == L'b' || p[1] == L'B') && (p[2] == L'0' || p[2] == L'1')) {
        p += 2;
        unsigned long value = 0, limit = (unsigned long)LONG_MAX + negative;
        int overflow = 0;
        while (*p == L'0' || *p == L'1') {
            unsigned int digit = *p++ - L'0';
            if (value > (limit - digit) / 2) overflow = 1;
            if (!overflow) value = value * 2 + digit;
        }
        if (end) *end = (wchar_t *)p;
        if (overflow) { errno = ERANGE; return negative ? LONG_MIN : LONG_MAX; }
        if (negative && value == (unsigned long)LONG_MAX + 1) return LONG_MIN;
        return negative ? -(long)value : (long)value;
    }
    if (base != 0 && (base < 2 || base > 36)) { errno = EINVAL; return 0; }
    int saved = errno;
    wchar_t *parsed;
    long value = wcstol(text, &parsed, base);
    if (parsed == text && errno == EINVAL) errno = saved;
    if (end) *end = parsed;
    return value;
}

/* AArch64 uses identical long/long-long ranges, but keep the unsigned parser
 * explicit: signed conversion would incorrectly saturate above LONG_MAX. */
static unsigned long long wide_unsigned(const wchar_t *text, wchar_t **end, int base)
{
    const wchar_t *p = text;
    while (iswspace(*p)) ++p;
    int negative = *p == L'-';
    if (*p == L'+' || *p == L'-') ++p;
    if ((base == 0 || base == 2) && p[0] == L'0' &&
        (p[1] == L'b' || p[1] == L'B') && (p[2] == L'0' || p[2] == L'1')) {
        p += 2;
        unsigned long long value = 0;
        int overflow = 0;
        while (*p == L'0' || *p == L'1') {
            unsigned digit = *p++ - L'0';
            if (value > (ULLONG_MAX - digit) / 2) overflow = 1;
            if (!overflow) value = value * 2 + digit;
        }
        if (end) *end = (wchar_t *)p;
        if (overflow) { errno = ERANGE; return ULLONG_MAX; }
        return negative ? 0ULL - value : value;
    }
    if (base != 0 && (base < 2 || base > 36)) { errno = EINVAL; return 0; }
    int saved = errno;
    wchar_t *parsed;
    unsigned long long value = wcstoull(text, &parsed, base);
    if (parsed == text && errno == EINVAL) errno = saved;
    if (end) *end = parsed;
    return value;
}
_Static_assert(ULONG_MAX == ULLONG_MAX && LONG_MAX == LLONG_MAX,
               "GearOS wide conversion bridge requires LP64");
unsigned long __isoc23_wcstoul(const wchar_t *text, wchar_t **end, int base)
{ return (unsigned long)wide_unsigned(text, end, base); }
unsigned long long __isoc23_wcstoull(const wchar_t *text, wchar_t **end, int base)
{ return wide_unsigned(text, end, base); }
long long __isoc23_wcstoll(const wchar_t *text, wchar_t **end, int base)
{ return (long long)__isoc23_wcstol(text, end, base); }
int __openat_2(int fd, const char *path, int flags)
{ return __openat64_2(fd, path, flags); }
