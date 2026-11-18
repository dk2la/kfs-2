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
 * Widths, flags and length modifiers (%5d, %08x, %ld) are not supported.
 *
 * Returns the number of characters written.
 */
int	printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
