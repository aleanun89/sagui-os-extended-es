/* stdlib.h - memoria, números, ordenación y entorno (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_STDLIB_H
#define NS_STDLIB_H

#include <stddef.h>
#if defined(_DEFAULT_SOURCE) || defined(_BSD_SOURCE) || defined(_GNU_SOURCE)
#include <alloca.h>
#endif
#define RAND_MAX 2147483647
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MB_CUR_MAX 1
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
void  *malloc(size_t n);
void  *calloc(size_t n, size_t tam);
void  *realloc(void *p, size_t n);
void   free(void *p);
int    posix_memalign(void **p, size_t alin, size_t n);
void  *aligned_alloc(size_t alin, size_t n);
size_t malloc_usable_size(void *p);
void   exit(int codigo) __attribute__((noreturn));
void   _Exit(int codigo) __attribute__((noreturn));
void   abort(void) __attribute__((noreturn));
int    atexit(void (*f)(void));
int    atoi(const char *s);
long   atol(const char *s);
long long atoll(const char *s);
double atof(const char *s);
long   strtol(const char *s, char **fim, int base);
unsigned long strtoul(const char *s, char **fim, int base);
long long strtoll(const char *s, char **fim, int base);
unsigned long long strtoull(const char *s, char **fim, int base);
double strtod(const char *s, char **fim);
float  strtof(const char *s, char **fim);
int    abs(int v);
long   labs(long v);
long long llabs(long long v);
div_t  div(int a, int b);
ldiv_t ldiv(long a, long b);
int    rand(void);
void   srand(unsigned s);
long   random(void);
void   srandom(unsigned s);
void   qsort(void *base, size_t n, size_t tam, int (*cmp)(const void *, const void *));
void  *bsearch(const void *chave, const void *base, size_t n, size_t tam,
               int (*cmp)(const void *, const void *));
char  *getenv(const char *nome);
int    setenv(const char *nome, const char *valor, int substituir);
int    unsetenv(const char *nome);
/* putenv: NO es la de POSIX: COPIA la cadena (como setenv), así que el búfer del llamador puede liberarse (D383). */
int    putenv(char *cadena);
char  *realpath(const char *caminho, char *resolvido);
int    mkstemp(char *modelo);
char  *mkdtemp(char *modelo);
int    system(const char *cmd);
#endif
