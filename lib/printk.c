#include "printk.h"
#include "io.h"
#include "stdarg.h"
#include "string.h"
#include "types.h"
#include "vga.h"

/*
 * Every character is also written to this I/O port. QEMU's debugcon device
 * (-debugcon file:...) records it, so CI can read what the kernel printed.
 * On real hardware nothing listens there and the write is ignored.
 */
#define DEBUGCON_PORT	0xE9

/* Enough for a 32-bit value in base 10. */
#define DIGITS_MAX	10
/* Keeps a silly width from overflowing the counter. */
#define WIDTH_MAX	1000

/* Flags and width of one conversion, as in "%-08". */
struct field {
	int	width;
	int	left;	/* '-': pad on the right */
	int	zero;	/* '0': pad numbers with zeros */
};

static void put_char(char c)
{
	outb(DEBUGCON_PORT, (uint8_t)c);
	vga_putchar(c);
}

static int put_repeat(char c, int n)
{
	int i;

	for (i = 0; i < n; i++)
		put_char(c);
	return n > 0 ? n : 0;
}

static int put_string(const char *s, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		put_char(s[i]);
	return (int)len;
}

/*
 * Print `prefix` then `body`, padded to the field width. Zero padding only
 * applies to numbers and goes between the prefix (sign or "0x") and the
 * digits, as in printf.
 */
static int print_field(const struct field *f, const char *prefix,
		       const char *body, size_t body_len, int numeric)
{
	size_t prefix_len = strlen(prefix);
	int pad = f->width - (int)(prefix_len + body_len);
	int zeros = !f->left && f->zero && numeric;
	int count = 0;

	if (!f->left && !zeros)
		count += put_repeat(' ', pad);
	count += put_string(prefix, prefix_len);
	if (zeros)
		count += put_repeat('0', pad);
	count += put_string(body, body_len);
	if (f->left)
		count += put_repeat(' ', pad);
	return count;
}

/*
 * Write `value` in `base` into `buf`, at least `min_digits` long (zero
 * padded). Returns the number of digits.
 */
static size_t format_unsigned(uint32_t value, uint32_t base, char *buf,
			      size_t min_digits)
{
	char reversed[DIGITS_MAX];
	size_t len = 0;
	size_t i;

	do {
		reversed[len++] = "0123456789abcdef"[value % base];
		value /= base;
	} while (value);
	while (len < min_digits && len < DIGITS_MAX)
		reversed[len++] = '0';
	for (i = 0; i < len; i++)
		buf[i] = reversed[len - 1 - i];
	return len;
}

static int print_conversion(char spec, const struct field *f, va_list *args)
{
	char buf[DIGITS_MAX];
	const char *s;
	char c;
	int32_t value;
	size_t len;

	switch (spec) {
	case 'c':
		c = (char)va_arg(*args, int);
		return print_field(f, "", &c, 1, 0);
	case 's':
		s = va_arg(*args, const char *);
		if (!s)
			s = "(null)";
		return print_field(f, "", s, strlen(s), 0);
	case 'd':
		value = va_arg(*args, int32_t);
		/* Negate as unsigned: -INT32_MIN does not fit in an int32_t. */
		len = format_unsigned(value < 0 ? -(uint32_t)value
						: (uint32_t)value, 10, buf, 0);
		return print_field(f, value < 0 ? "-" : "", buf, len, 1);
	case 'u':
		len = format_unsigned(va_arg(*args, uint32_t), 10, buf, 0);
		return print_field(f, "", buf, len, 1);
	case 'x':
		len = format_unsigned(va_arg(*args, uint32_t), 16, buf, 0);
		return print_field(f, "", buf, len, 1);
	case 'p':
		len = format_unsigned((uintptr_t)va_arg(*args, void *), 16,
				      buf, 8);
		return print_field(f, "0x", buf, len, 1);
	case '%':
		put_char('%');
		return 1;
	default:
		/* Unknown conversion: print it as written, minus the flags. */
		put_char('%');
		put_char(spec);
		return 2;
	}
}

/* Read the flags and width after a '%'. Returns the rest of the format. */
static const char *parse_field(const char *fmt, struct field *f)
{
	f->width = 0;
	f->left = 0;
	f->zero = 0;
	for (;; fmt++) {
		if (*fmt == '-')
			f->left = 1;
		else if (*fmt == '0')
			f->zero = 1;
		else
			break;
	}
	while (*fmt >= '0' && *fmt <= '9') {
		if (f->width < WIDTH_MAX)
			f->width = f->width * 10 + (*fmt - '0');
		fmt++;
	}
	if (f->width > WIDTH_MAX)
		f->width = WIDTH_MAX;
	return fmt;
}

int vprintk(const char *fmt, va_list args)
{
	struct field field;
	int count = 0;

	while (*fmt) {
		if (*fmt != '%') {
			put_char(*fmt++);
			count++;
			continue;
		}
		fmt = parse_field(fmt + 1, &field);
		if (!*fmt) {
			/* A '%' with nothing to convert at the end. */
			put_char('%');
			count++;
			break;
		}
		count += print_conversion(*fmt++, &field, &args);
	}
	return count;
}

int printk(const char *fmt, ...)
{
	va_list args;
	int count;

	va_start(args, fmt);
	count = vprintk(fmt, args);
	va_end(args);
	return count;
}
