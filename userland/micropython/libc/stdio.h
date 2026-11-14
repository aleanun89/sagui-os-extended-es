#ifndef SAGUI_STDIO_H
#define SAGUI_STDIO_H
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#include <stddef.h>
#include <stdarg.h>
#define EOF (-1)
int printf(const char *fmt, ...);
int snprintf(char *s, size_t n, const char *fmt, ...);
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int putchar(int c);
int puts(const char *s);
#endif
