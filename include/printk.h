#ifndef PRINTK_H
#define PRINTK_H

#include "stdarg.h"

/*
 * Formatted print to the active screen. Every character is also written to
 * I/O port 0xE9 (QEMU debugcon).
 *
 * Supported conversions:
 *   %c  character
 *   %s  string ("(null)" for a NULL pointer)
 *   %d  signed decimal
 *   %u  unsigned decimal
 *   %x  unsigned hexadecimal, lower case
 *   %p  pointer, as 0x followed by eight hexadecimal digits
 *   %%  a literal percent sign
 *
 * Each conversion takes an optional field width and the flags:
 *   -   pad on the right instead of the left (%-10s)
 *   0   pad numbers with zeros instead of spaces (%08x); the zeros go after
 *       the sign or the 0x prefix
 * Precision, '*' widths and length modifiers (%.3s, %*d, %ld) are not
 * supported.
 *
 * Returns the number of characters written.
 */
int	printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* The same, with the arguments already collected in a va_list. */
int	vprintk(const char *fmt, va_list args);

#endif
