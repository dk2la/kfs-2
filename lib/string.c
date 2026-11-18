#include "string.h"

size_t strlen(const char *s)
{
	size_t len = 0;

	while (s[len])
		len++;
	return len;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (uint8_t)*a - (uint8_t)*b;
}

void *memset(void *dst, int c, size_t n)
{
	uint8_t *d = dst;

	while (n--)
		*d++ = (uint8_t)c;
	return dst;
}

/* The regions must not overlap. */
void *memcpy(void *dst, const void *src, size_t n)
{
	uint8_t *d = dst;
	const uint8_t *s = src;

	while (n--)
		*d++ = *s++;
	return dst;
}
