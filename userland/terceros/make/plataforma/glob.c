/* SPDX-License-Identifier: GPL-3.0-only */
/* glob.c - glob() mínimo para GNU make en SaguiOS: comodines *, ? y [..] en cualquier componente de
 * la ruta, con las funciones de directorio que make aporta (GLOB_ALTDIRFUNC). Sin llaves ni ~. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fnmatch.h>
#include "glob.h"

static int hay_magia(const char *s)
{
    for (; *s; s++)
        if (*s == '*' || *s == '?' || *s == '[') return 1;
    return 0;
}

static int cmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int anadir(glob_t *g, size_t *cap, const char *ruta)
{
    if (g->gl_pathc + 2 > *cap) {
        size_t nc = *cap ? *cap * 2 : 16;
        char **nv = realloc(g->gl_pathv, nc * sizeof(char *));
        if (!nv) return -1;
        g->gl_pathv = nv;
        *cap = nc;
    }
    char *c = strdup(ruta);
    if (!c) return -1;
    g->gl_pathv[g->gl_pathc++] = c;
    g->gl_pathv[g->gl_pathc] = NULL;
    return 0;
}

/* Expande el resto del patrón 'resto' tras el prefijo 'pre' (ya resuelto, con barra final si es dir). */
static int expandir(glob_t *g, size_t *cap, const char *pre, const char *resto)
{
    while (*resto == '/') resto++;
    if (!*resto) return 0;
    const char *fin = strchr(resto, '/');
    size_t n = fin ? (size_t)(fin - resto) : strlen(resto);
    char comp[512];
    if (n >= sizeof comp) return 0;
    memcpy(comp, resto, n);
    comp[n] = 0;
    const char *sig = fin ? fin : resto + n;
    size_t lp = strlen(pre);
    char *ruta = malloc(lp + n + 2);
    if (!ruta) return -1;
    if (!hay_magia(comp)) {
        struct stat st;
        memcpy(ruta, pre, lp);
        memcpy(ruta + lp, comp, n + 1);
        int ok = g->gl_lstat(ruta, &st) == 0;
        int r = 0;
        if (ok) {
            if (*sig) {
                strcat(ruta, "/");
                r = expandir(g, cap, ruta, sig);
            } else r = anadir(g, cap, ruta);
        }
        free(ruta);
        return r;
    }
    char *dir = malloc(lp + 2);
    if (!dir) { free(ruta); return -1; }
    memcpy(dir, pre, lp);
    dir[lp] = 0;
    void *d = g->gl_opendir(lp ? dir : ".");
    int r = 0;
    if (d) {
        struct dirent *e;
        while ((e = g->gl_readdir(d)) != NULL) {
            if (e->d_name[0] == '.' && comp[0] != '.') continue;
            if (fnmatch(comp, e->d_name, 0) != 0) continue;
            size_t ln = strlen(e->d_name);
            char *nuevo = malloc(lp + ln + 2);
            if (!nuevo) { r = -1; break; }
            memcpy(nuevo, pre, lp);
            memcpy(nuevo + lp, e->d_name, ln + 1);
            if (*sig) {
                struct stat st;
                if (g->gl_stat(nuevo, &st) == 0 && S_ISDIR(st.st_mode)) {
                    strcat(nuevo, "/");
                    r = expandir(g, cap, nuevo, sig);
                }
            } else r = anadir(g, cap, nuevo);
            free(nuevo);
            if (r) break;
        }
        g->gl_closedir(d);
    }
    free(dir);
    free(ruta);
    return r;
}

int glob(const char *patron, int flags, int (*errfn)(const char *, int), glob_t *g)
{
    size_t cap = 0;
    (void)errfn;
    g->gl_pathc = 0;
    g->gl_pathv = NULL;
    if (!(flags & GLOB_ALTDIRFUNC)) return GLOB_ABORTED;
    const char *p = patron;
    char pre[2] = "";
    if (*p == '/') { pre[0] = '/'; pre[1] = 0; }
    if (expandir(g, &cap, pre, p) < 0) { globfree(g); return GLOB_NOSPACE; }
    if (g->gl_pathc == 0) return GLOB_NOMATCH;
    if (!(flags & GLOB_NOSORT)) qsort(g->gl_pathv, g->gl_pathc, sizeof(char *), cmp);
    return 0;
}

void globfree(glob_t *g)
{
    for (size_t i = 0; i < g->gl_pathc; i++) free(g->gl_pathv[i]);
    free(g->gl_pathv);
    g->gl_pathv = NULL;
    g->gl_pathc = 0;
}
