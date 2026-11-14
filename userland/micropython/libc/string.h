/* string.h mínimo: implementado em libc_sagui.c (e memcpy/memset/memmove/memcmp na libsagui). */
#ifndef SAGUI_STRING_H
#define SAGUI_STRING_H
#include <stddef.h>
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int v, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memchr(const void *s, int c, size_t n);
size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strcpy(char *d, const char *s);
char  *strcat(char *d, const char *s);
char  *strstr(const char *h, const char *n);
#endif
