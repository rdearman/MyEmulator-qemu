#ifndef __STDARG_H
#define __STDARG_H

/* REM uses a simple pointer va_list. The backend saves incoming argument
 * registers in the variadic function prologue and implements these builtins
 * directly. */
typedef char *va_list;

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap)         __builtin_va_end(ap)
#define va_arg(ap, ty)     __builtin_va_arg(ap, ty)
#define va_copy(dst, src)  __builtin_va_copy(dst, src)

typedef va_list __gnuc_va_list;

#endif
