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
