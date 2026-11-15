// SPDX-License-Identifier: GPL-3.0-only
/* local_rsync.c - copia local de rsync para SaguiOS (D314).
 *
 * rsync -a [--delete] [--dry-run] [-v] [--progress] ORIGEN... DESTINO
 *
 * Es código propio (no de openrsync): dentro de una misma máquina no hace falta el
 * protocolo ni el delta de bloques, así que se compara y se copian los ficheros que
 * cambiaron enteros. Un fichero se considera igual si tiene el mismo tamaño y, o
 * la misma fecha (cuando el sistema de archivos la guarda), o el mismo contenido:
 * los sistemas de archivos escribibles de SaguiOS no guardan fechas, así que se
 * compara el contenido (como rsync -c) y lo igual no se reescribe.
 * Las reglas de las barras son las de rsync: ORIGEN/ copia el contenido de la
 * carpeta; ORIGEN copia la carpeta misma dentro de DESTINO.
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "local_rsync.h"

#define RUTA 1024
#define BUF  (32 * 1024)

static const struct local_opts *O;
static unsigned long nf_total, nf_copiados, nf_borrados, nd_creados;
static unsigned long long bytes_total, bytes_copiados;
static int erros;

static void dizer(const char *fmt, const char *a, const char *b)
{
    if (O->callar || !O->verbose) return;
    printf(fmt, a, b ? b : "");
    putchar('\n');
}

static int comparar(const char *a, const char *b)   /* 1 = igual */
{
    int fa = open(a, O_RDONLY), fb = open(b, O_RDONLY), igual = 1;
    if (fa < 0 || fb < 0) { if (fa >= 0) close(fa); if (fb >= 0) close(fb); return 0; }
    static char ba[BUF], bb[BUF];
    for (;;) {
        ssize_t na = read(fa, ba, BUF), nb = 0;
        if (na < 0) { igual = 0; break; }
        ssize_t got = 0;
        while (got < na) {   /* read puede dar menos: se completa el mismo tramo */
            ssize_t r = read(fb, bb + got, (size_t)(na - got));
            if (r <= 0) break;
            got += r;
        }
        nb = got;
        if (na != nb || memcmp(ba, bb, (size_t)na)) { igual = 0; break; }
        if (na == 0) break;
    }
    close(fa); close(fb);
    return igual;
}

static void progreso(const char *nome, unsigned long long feito, unsigned long long tot)
{
    if (!O->progreso || O->callar) return;
    printf("\r%14llu %3d%%  %s", feito, tot ? (int)(feito * 100 / tot) : 100, nome);
    fflush(stdout);
}

static int copiar_ficheiro(const char *de, const char *para, const char *rel, off_t tam)
{
    char tmp[RUTA + 16];
    snprintf(tmp, sizeof tmp, "%s.rsync-tmp", para);
    int fa = open(de, O_RDONLY);
    if (fa < 0) { fprintf(stderr, "rsync: %s: %s\n", de, strerror(errno)); erros++; return -1; }
    int fb = open(tmp, O_WRONLY | O_CREAT | O_TRUNC);
    if (fb < 0) { fprintf(stderr, "rsync: %s: %s\n", tmp, strerror(errno)); close(fa); erros++; return -1; }
    static char b[BUF];
    unsigned long long feito = 0;
    int ok = 1;
    for (;;) {
        ssize_t n = read(fa, b, BUF);
        if (n < 0) { ok = 0; break; }
        if (n == 0) break;
        for (ssize_t p = 0; p < n;) {
            ssize_t w = write(fb, b + p, (size_t)(n - p));
            if (w <= 0) { ok = 0; break; }
            p += w;
        }
        if (!ok) break;
        feito += (unsigned long long)n;
        progreso(rel, feito, (unsigned long long)tam);
    }
    close(fa);
    close(fb);
    if (ok) {
        unlink(para);
        if (rename(tmp, para) < 0) ok = 0;
    }
    if (!ok) { unlink(tmp); fprintf(stderr, "rsync: %s: fallo al copiar\n", rel); erros++; return -1; }
    if (O->progreso && !O->callar) printf("\n");
    bytes_copiados += feito;
    return 0;
}

static int cmp_nome(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* Los nombres de una carpeta, ordenados. Devuelve el número, o -1. */
static int ler_pasta(const char *p, char ***nomes)
{
    DIR *d = opendir(p);
    if (!d) return -1;
    char **v = NULL;
    int n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (n == cap) { cap = cap ? cap * 2 : 32; v = realloc(v, (size_t)cap * sizeof *v); if (!v) { closedir(d); return -1; } }
        v[n++] = strdup(e->d_name);
    }
    closedir(d);
    if (n > 1) qsort(v, (size_t)n, sizeof *v, cmp_nome);
    *nomes = v;
    return n;
}

static void liberar(char **v, int n) { for (int i = 0; i < n; i++) free(v[i]); free(v); }

/* Borra el árbol dest/rel entero (--delete). */
static void apagar_arvore(const char *p, const char *rel)
{
    struct stat st;
    if (lstat(p, &st) < 0) return;
    if (S_ISDIR(st.st_mode)) {
        char **v; int n = ler_pasta(p, &v);
        for (int i = 0; i < n; i++) {
            char c[RUTA], r[RUTA];
            snprintf(c, sizeof c, "%s/%s", p, v[i]);
            snprintf(r, sizeof r, "%s/%s", rel, v[i]);
            apagar_arvore(c, r);
        }
        if (n >= 0) liberar(v, n);
        dizer("deleting %s/", rel, NULL);
        if (!O->simular && rmdir(p) < 0) { fprintf(stderr, "rsync: %s: %s\n", p, strerror(errno)); erros++; }
    } else {
        dizer("deleting %s", rel, NULL);
        if (!O->simular && unlink(p) < 0) { fprintf(stderr, "rsync: %s: %s\n", p, strerror(errno)); erros++; }
    }
    nf_borrados++;
}

static int criar_pasta(const char *p, const char *rel)
{
    struct stat st;
    if (stat(p, &st) == 0) return S_ISDIR(st.st_mode) ? 0 : -1;
    dizer("%s/", rel, NULL);
    nd_creados++;
    if (O->simular) return 0;
    if (mkdir(p, 0755) < 0 && errno != EEXIST) { fprintf(stderr, "rsync: %s: %s\n", p, strerror(errno)); erros++; return -1; }
    return 0;
}

static void sincronizar(const char *org, const char *dst, const char *rel);

/* -p: el destino toma los modos del origen (D282; compat_sagui.c, no-op si el montaje no los guarda). */
static void con_modo(const char *org, const char *dst)
{
    if (O->permisos && !O->simular) rs_copiar_modo(org, dst);
}

static void um_item(const char *org, const char *dst, const char *rel)
{
    struct stat so, sd;
    if (stat(org, &so) < 0) { fprintf(stderr, "rsync: %s: %s\n", org, strerror(errno)); erros++; return; }
    int existe = stat(dst, &sd) == 0;
    if (S_ISDIR(so.st_mode)) {
        if (!O->recursivo) { dizer("skipping directory %s", rel, NULL); return; }
        if (existe && !S_ISDIR(sd.st_mode)) { fprintf(stderr, "rsync: %s: existe y no es una carpeta\n", dst); erros++; return; }
        if (!existe && criar_pasta(dst, rel) < 0) return;
        sincronizar(org, dst, rel);
        con_modo(org, dst);
        return;
    }
    if (!S_ISREG(so.st_mode)) { dizer("skipping non-regular file %s", rel, NULL); return; }
    nf_total++;
    bytes_total += (unsigned long long)so.st_size;
    if (existe && S_ISREG(sd.st_mode) && sd.st_size == so.st_size &&
        ((!O->sumas && so.st_mtime && so.st_mtime == sd.st_mtime) || comparar(org, dst))) {
        con_modo(org, dst);
        return;
    }
    dizer("%s", rel, NULL);
    nf_copiados++;
    if (O->simular) { bytes_copiados += (unsigned long long)so.st_size; return; }
    if (copiar_ficheiro(org, dst, rel, so.st_size) == 0) con_modo(org, dst);
}

/* Sincroniza el contenido de la carpeta org en dst (que ya existe, salvo en simulación). */
static void sincronizar(const char *org, const char *dst, const char *rel)
{
    char **vo = NULL, **vd = NULL;
    int no = ler_pasta(org, &vo);
    if (no < 0) { fprintf(stderr, "rsync: %s: no se puede leer la carpeta\n", org); erros++; return; }
    for (int i = 0; i < no; i++) {
        char co[RUTA], cd[RUTA], cr[RUTA];
        snprintf(co, sizeof co, "%s/%s", org, vo[i]);
        snprintf(cd, sizeof cd, "%s/%s", dst, vo[i]);
        if (rel[0]) snprintf(cr, sizeof cr, "%s/%s", rel, vo[i]); else snprintf(cr, sizeof cr, "%s", vo[i]);
        um_item(co, cd, cr);
    }
    if (O->borrar) {
        int nd = ler_pasta(dst, &vd);
        for (int i = 0; i < nd; i++) {
            int esta = 0;
            for (int j = 0; j < no && !esta; j++) esta = !strcmp(vo[j], vd[i]);
            if (esta) continue;
            char cd[RUTA], cr[RUTA];
            snprintf(cd, sizeof cd, "%s/%s", dst, vd[i]);
            if (rel[0]) snprintf(cr, sizeof cr, "%s/%s", rel, vd[i]); else snprintf(cr, sizeof cr, "%s", vd[i]);
            apagar_arvore(cd, cr);
        }
        if (nd >= 0) liberar(vd, nd);
    }
    liberar(vo, no);
}

int rsync_local(const struct local_opts *o, int norigens, char **origens, const char *destino)
{
    O = o;
    char dst[RUTA];
    snprintf(dst, sizeof dst, "%s", destino);
    size_t ld = strlen(dst);
    int destino_barra = ld > 1 && dst[ld - 1] == '/';
    while (ld > 1 && dst[ld - 1] == '/') dst[--ld] = 0;

    struct stat sd;
    int dst_existe = stat(dst, &sd) == 0;
    int dst_pasta = dst_existe && S_ISDIR(sd.st_mode);
    if (norigens > 1 || destino_barra || dst_pasta || o->recursivo) {
        /* el destino es una carpeta: se crea si hace falta (un nivel, como rsync) */
        if (!dst_existe) { if (criar_pasta(dst, ".") < 0) return 1; }
        else if (!dst_pasta) { fprintf(stderr, "rsync: %s: no es una carpeta\n", dst); return 1; }
    }
    for (int i = 0; i < norigens; i++) {
        char org[RUTA];
        snprintf(org, sizeof org, "%s", origens[i]);
        size_t lo = strlen(org);
        int barra = lo > 1 && org[lo - 1] == '/';
        while (lo > 1 && org[lo - 1] == '/') org[--lo] = 0;
        struct stat so;
        if (stat(org, &so) < 0) { fprintf(stderr, "rsync: %s: %s\n", org, strerror(errno)); erros++; continue; }
        if (S_ISDIR(so.st_mode) && barra) {
            if (!o->recursivo) { fprintf(stderr, "rsync: %s: es una carpeta (use -r o -a)\n", org); erros++; continue; }
            sincronizar(org, dst, "");
        } else {
            const char *base = strrchr(org, '/');
            base = base ? base + 1 : org;
            char cd[RUTA];
            if (dst_pasta || norigens > 1 || destino_barra || o->recursivo || (!dst_existe && S_ISDIR(so.st_mode)))
                snprintf(cd, sizeof cd, "%s/%s", dst, base);
            else snprintf(cd, sizeof cd, "%s", dst);
            um_item(org, cd, base);
        }
    }
    if (!o->callar && (o->verbose || o->estadisticas || o->simular)) {
        printf("\nsent %llu bytes  total size is %llu%s\n", bytes_copiados, bytes_total,
               o->simular ? " (DRY RUN)" : "");
        if (o->estadisticas)
            printf("Number of files: %lu\nNumber of files transferred: %lu\nNumber of files deleted: %lu\n"
                   "Number of directories created: %lu\nTotal file size: %llu bytes\n"
                   "Total transferred file size: %llu bytes\n",
                   nf_total, nf_copiados, nf_borrados, nd_creados, bytes_total, bytes_copiados);
    }
    return erros ? 23 : 0;
}
