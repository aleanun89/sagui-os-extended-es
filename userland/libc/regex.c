/* regex.c - regcomp/regexec de POSIX sobre las expresiones de la libsagui
 * (regex.c de la libsagui, D097) para NetSurf (D184).
 *
 * NetSurf solo las usa al guardar una página completa (save_complete.c,
 * para buscar los @import del CSS). Carencias: no hay subexpresiones (solo
 * pmatch[0], la coincidencia entera) y el tamaño del programa de la
 * expresión está limitado (ER_MAX_INST).
 */
#include "sagui.h"

#include "errno.h"
#include "regex.h"
#include "stdlib.h"
#include "string.h"

int regcomp(regex_t *r, const char *padrao, int flags)
{
    er_t *e = malloc(sizeof *e);
    if (!e) return REG_ESPACE;
    if (er_compilar(e, padrao, (flags & REG_EXTENDED) != 0, (flags & REG_ICASE) != 0) != 0) {
        free(e);
        return REG_BADPAT;
    }
    r->__er = e;
    r->re_nsub = 0;
    return 0;
}

int regexec(const regex_t *r, const char *texto, size_t n, regmatch_t *m, int flags)
{
    (void)flags;
    int ini, fim;
    if (!r->__er) return REG_BADPAT;
    if (!er_buscar(r->__er, texto, (int)strlen(texto), 0, &ini, &fim)) return REG_NOMATCH;
    for (size_t i = 0; i < n; i++) {
        m[i].rm_so = i == 0 ? ini : -1;
        m[i].rm_eo = i == 0 ? fim : -1;
    }
    return 0;
}

size_t regerror(int erro, const regex_t *r, char *buf, size_t n)
{
    const char *t = erro == REG_NOMATCH ? "sin coincidencia"
                  : erro == REG_ESPACE ? "sin memoria"
                  : (r && r->__er) ? ((er_t *)r->__er)->erro : "expresión no válida";
    if (n) strlcpy(buf, t, n);
    return strlen(t) + 1;
}

void regfree(regex_t *r)
{
    free(r->__er);
    r->__er = NULL;
}
