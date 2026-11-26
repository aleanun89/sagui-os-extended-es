/* string.h - cadenas y memoria (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_STRING_H
#define NS_STRING_H

#include <stddef.h>
void  *memcpy(void *d, const void *s, size_t n);
void  *memmove(void *d, const void *s, size_t n);
void  *memset(void *d, int v, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memchr(const void *s, int c, size_t n);
void  *memrchr(const void *s, int c, size_t n);
void  *memmem(const void *h, size_t hn, const void *n, size_t nn);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t max);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
int    strcoll(const char *a, const char *b);
char  *strcpy(char *d, const char *s);
char  *strncpy(char *d, const char *s, size_t n);
char  *stpcpy(char *d, const char *s);
size_t strlcpy(char *d, const char *s, size_t n);
size_t strlcat(char *d, const char *s, size_t n);
char  *strcat(char *d, const char *s);
char  *strncat(char *d, const char *s, size_t n);
char  *strchr(const char *s, int c);
char  *strchrnul(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *h, const char *n);
char  *strcasestr(const char *h, const char *n);
char  *strpbrk(const char *s, const char *aceitos);
size_t strspn(const char *s, const char *aceitos);
size_t strcspn(const char *s, const char *recusados);
char  *strtok(char *s, const char *sep);
char  *strtok_r(char *s, const char *sep, char **resto);
char  *strsep(char **s, const char *sep);
char  *strdup(const char *s);
char  *strndup(const char *s, size_t n);
char  *strerror(int erro);
int    strerror_r(int erro, char *buf, size_t n);   /* D477: versión XSI (servidores.c) */
#include <strings.h>
#endif
