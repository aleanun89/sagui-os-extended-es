#ifndef SAGUI_STDLIB_H
#define SAGUI_STDLIB_H
#include <stddef.h>
void abort(void) __attribute__((noreturn));
void exit(int codigo) __attribute__((noreturn));
#define NULL ((void *)0)
#endif
