#ifndef PRINTK_H
#define PRINTK_H

/*
 * Formatted print to the active screen.
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
 * A minimum field width may be given, with the flags '0' (pad numbers with
 * zeros) and '-' (pad on the right): %08x, %4u, %-10s. Length modifiers
 * (%ld) are not supported.
 *
 * Every character is also written to I/O port 0xE9, QEMU's debug console.
 *
 * Returns the number of characters written.
 */
int	printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
