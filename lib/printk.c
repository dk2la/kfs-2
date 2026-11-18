#include "printk.h"
#include "stdarg.h"
#include "types.h"
#include "vga.h"

/* Enough for a 32-bit value in base 10. */
#define DIGITS_MAX	10

static int print_string(const char *s)
{
	int count = 0;

	if (!s)
		s = "(null)";
	while (*s) {
		vga_putchar(*s++);
		count++;
	}
	return count;
}

/* Print `value` in `base`, left-padded with zeros to `min_digits`. */
static int print_unsigned(uint32_t value, uint32_t base, int min_digits)
{
	char digits[DIGITS_MAX];
	int len = 0;
	int count = 0;

	do {
		digits[len++] = "0123456789abcdef"[value % base];
		value /= base;
	} while (value);
	for (; min_digits > len; min_digits--) {
		vga_putchar('0');
		count++;
	}
	while (len) {
		vga_putchar(digits[--len]);
		count++;
	}
	return count;
}

static int print_signed(int32_t value)
{
	if (value < 0) {
		vga_putchar('-');
		/* Negate as unsigned: -INT32_MIN does not fit in an int32_t. */
		return 1 + print_unsigned(-(uint32_t)value, 10, 0);
	}
	return print_unsigned((uint32_t)value, 10, 0);
}

static int print_conversion(char spec, va_list *args)
{
	switch (spec) {
	case 'c':
		vga_putchar((char)va_arg(*args, int));
		return 1;
	case 's':
		return print_string(va_arg(*args, const char *));
	case 'd':
		return print_signed(va_arg(*args, int32_t));
	case 'u':
		return print_unsigned(va_arg(*args, uint32_t), 10, 0);
	case 'x':
		return print_unsigned(va_arg(*args, uint32_t), 16, 0);
	case 'p':
		return print_string("0x") +
		       print_unsigned((uintptr_t)va_arg(*args, void *), 16, 8);
	case '%':
		vga_putchar('%');
		return 1;
	default:
		/* Unknown conversion: print it as written. */
		vga_putchar('%');
		vga_putchar(spec);
		return 2;
	}
}

int printk(const char *fmt, ...)
{
	va_list args;
	int count = 0;

	va_start(args, fmt);
	while (*fmt) {
		if (*fmt != '%') {
			vga_putchar(*fmt++);
			count++;
			continue;
		}
		fmt++;
		if (!*fmt) {
			/* A lone '%' at the end of the format. */
			vga_putchar('%');
			count++;
			break;
		}
		count += print_conversion(*fmt++, &args);
	}
	va_end(args);
	return count;
}
