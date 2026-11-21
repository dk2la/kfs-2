#include "printk.h"
#include "io.h"
#include "stdarg.h"
#include "string.h"
#include "types.h"
#include "vga.h"

/* QEMU's debug console. On real hardware nothing listens on this port. */
#define DEBUGCON_PORT	0xE9

/* A sign or "0x", then at most ten digits (a 32-bit value in base 10). */
#define FIELD_MAX	12

struct format {
	int	left;	/* '-': pad on the right */
	int	zero;	/* '0': pad numbers with zeros */
	int	width;	/* minimum field width, 0 for none */
};

static void emit(char c)
{
	vga_putchar(c);
	outb(DEBUGCON_PORT, (uint8_t)c);
}

static int emit_repeat(char c, int count)
{
	int i;

	for (i = 0; i < count; i++)
		emit(c);
	return count > 0 ? count : 0;
}

/*
 * Print `len` characters of `text` in a field of at least `fmt->width`.
 * The first `prefix` characters (a sign) stay in front of any zero padding.
 */
static int print_field(const char *text, int len, int prefix,
		       const struct format *fmt)
{
	int pad = fmt->width > len ? fmt->width - len : 0;
	int i = 0;

	if (!fmt->left && !fmt->zero)
		emit_repeat(' ', pad);
	for (; i < prefix; i++)
		emit(text[i]);
	if (!fmt->left && fmt->zero)
		emit_repeat('0', pad);
	for (; i < len; i++)
		emit(text[i]);
	if (fmt->left)
		emit_repeat(' ', pad);
	return len + pad;
}

/* Write `value` in `base` to `dst`, at least `min_digits` long. */
static int format_unsigned(char *dst, uint32_t value, uint32_t base,
			   int min_digits)
{
	char digits[FIELD_MAX];
	int len = 0;
	int out = 0;

	do {
		digits[len++] = "0123456789abcdef"[value % base];
		value /= base;
	} while (value);
	while (len < min_digits)
		digits[len++] = '0';
	while (len)
		dst[out++] = digits[--len];
	return out;
}

static int print_conversion(char spec, const struct format *fmt,
			    va_list *args)
{
	struct format text_fmt = *fmt;
	char buf[FIELD_MAX];
	const char *s;
	int32_t d;
	int prefix = 0;

	/* Zero padding only makes sense for numbers. */
	text_fmt.zero = 0;
	switch (spec) {
	case 'c':
		buf[0] = (char)va_arg(*args, int);
		return print_field(buf, 1, 0, &text_fmt);
	case 's':
		s = va_arg(*args, const char *);
		if (!s)
			s = "(null)";
		return print_field(s, (int)strlen(s), 0, &text_fmt);
	case 'd':
		d = va_arg(*args, int32_t);
		if (d < 0)
			buf[prefix++] = '-';
		/* Negate as unsigned: -INT32_MIN does not fit in an int32_t. */
		return print_field(buf, prefix + format_unsigned(buf + prefix,
				   d < 0 ? -(uint32_t)d : (uint32_t)d, 10, 0),
				   prefix, fmt);
	case 'u':
		return print_field(buf, format_unsigned(buf,
				   va_arg(*args, uint32_t), 10, 0), 0, fmt);
	case 'x':
		return print_field(buf, format_unsigned(buf,
				   va_arg(*args, uint32_t), 16, 0), 0, fmt);
	case 'p':
		buf[0] = '0';
		buf[1] = 'x';
		return print_field(buf, 2 + format_unsigned(buf + 2,
				   (uintptr_t)va_arg(*args, void *), 16, 8),
				   0, &text_fmt);
	case '%':
		emit('%');
		return 1;
	default:
		/* Unknown conversion: print it as written. */
		emit('%');
		emit(spec);
		return 2;
	}
}

int printk(const char *fmt, ...)
{
	struct format field;
	va_list args;
	int count = 0;

	va_start(args, fmt);
	while (*fmt) {
		if (*fmt != '%') {
			emit(*fmt++);
			count++;
			continue;
		}
		fmt++;
		field.left = 0;
		field.zero = 0;
		field.width = 0;
		for (; *fmt == '-' || *fmt == '0'; fmt++) {
			if (*fmt == '-')
				field.left = 1;
			else
				field.zero = 1;
		}
		while (*fmt >= '0' && *fmt <= '9')
			field.width = field.width * 10 + (*fmt++ - '0');
		if (!*fmt) {
			/* A lone '%' at the end of the format. */
			emit('%');
			count++;
			break;
		}
		count += print_conversion(*fmt++, &field, &args);
	}
	va_end(args);
	return count;
}
