#ifndef STDARG_H
#define STDARG_H

/*
 * Variadic arguments. These map straight onto compiler builtins, so no host
 * header or library is involved.
 */

typedef __builtin_va_list	va_list;

#define va_start(ap, last)	__builtin_va_start(ap, last)
#define va_arg(ap, type)	__builtin_va_arg(ap, type)
#define va_end(ap)		__builtin_va_end(ap)

#endif
