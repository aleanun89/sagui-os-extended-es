/* regex.h - expresiones regulares POSIX sobre las de la libsagui (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_REGEX_H
#define NS_REGEX_H

#include <stddef.h>
#include <sys/types.h>
typedef int regoff_t;
typedef struct {
    size_t re_nsub;
    void  *__er;        /* el er_t de la libsagui (regex.c, D097) */
} regex_t;
typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;
#define REG_EXTENDED 1
#define REG_ICASE    2
#define REG_NOSUB    4
#define REG_NEWLINE  8
#define REG_NOTBOL   1
#define REG_NOTEOL   2
#define REG_NOMATCH  1
#define REG_BADPAT   2
#define REG_ESPACE  12
/* Sobre las expresiones de la libsagui: sin subexpresiones (solo la
 * coincidencia entera en pmatch[0]). */
int    regcomp(regex_t *r, const char *padrao, int flags);
int    regexec(const regex_t *r, const char *texto, size_t n, regmatch_t *m, int flags);
size_t regerror(int erro, const regex_t *r, char *buf, size_t n);
void   regfree(regex_t *r);
#endif
