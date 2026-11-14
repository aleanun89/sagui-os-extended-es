// SPDX-License-Identifier: GPL-2.0-or-later
/* bloqueo_anfitriao.c - el analizador de listas, la tabla y la búsqueda del
 * bloqueo por DNS (D218), compilados en el PC con los sanitizadores para
 * tests/h218_bloqueo_dns.py. Usa los mismos archivos que el kernel y el
 * programa: kernel/inc/bloqueo_tabla.h y userland/programas/bloqueo_hosts.h.
 *
 *   compilar HOSTS PERMITIDOS SAIDA.bin   analiza y escribe la tabla como «bloqueo»
 *   validar TABLA.bin                     lo que haría el kernel: 0, -1 o -2
 *   consultar TABLA.bin SUB NOMES.txt     una línea «nombre resultado» por nombre
 *   medir TABLA.bin SUB NOMES.txt VUELTAS nanosegundos por consulta */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bloqueo_tabla.h"
#include "../../userland/programas/bloqueo_hosts.h"

typedef struct { uint64_t *v; uint32_t n, cap; } vetor_t;

static void por(void *ctx, const char *d, int n)
{
    vetor_t *v = ctx;
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 1024;
        v->v = realloc(v->v, v->cap * sizeof(uint64_t));
        if (!v->v) abort();
    }
    v->v[v->n++] = bloqueo_hash(d, (uint32_t)n);
}

static void analisar(const char *caminho, vetor_t *v, uint32_t *boas, uint32_t *mas)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) return;
    static char l[1 << 16];
    while (fgets(l, sizeof l, f)) {
        int n = (int)strlen(l);
        while (n && l[n - 1] == '\n') n--;
        int k = bh_linha(l, n, por, v);
        if (k > 0) *boas += (uint32_t)k; else if (k < 0) (*mas)++;
    }
    fclose(f);
}

static double agora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static unsigned char *ler_tudo(const char *caminho, uint32_t *tam)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) { perror(caminho); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *d = malloc(n ? (size_t)n : 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) exit(1);
    fclose(f);
    *tam = (uint32_t)n;
    return d;
}

int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "compilar")) {
        vetor_t vb = { 0 }, vp = { 0 };
        uint32_t boas = 0, mas = 0, pb = 0, pm = 0;
        double t0 = agora();
        analisar(argv[2], &vb, &boas, &mas);
        analisar(argv[3], &vp, &pb, &pm);
        uint32_t nb = bh_ordenar_unicos(vb.v, vb.n), np = bh_ordenar_unicos(vp.v, vp.n);
        double t1 = agora();
        bloqueo_cabecera_t c;
        memset(&c, 0, sizeof c);
        memcpy(c.magia, BLOQ_MAGIA, 8);
        c.versao = 1;
        c.n_bloq = nb;
        c.n_perm = np;
        FILE *f = fopen(argv[4], "wb");
        fwrite(&c, sizeof c, 1, f);
        if (nb) fwrite(vb.v, 8, nb, f);
        if (np) fwrite(vp.v, 8, np, f);
        fclose(f);
        printf("boas %u mas %u bloq %u perm %u bytes %u ms %.1f\n", boas, mas, nb, np,
               (unsigned)(sizeof c + 8u * (nb + np)), (t1 - t0) * 1000);
        free(vb.v);
        free(vp.v);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "validar")) {
        uint32_t tam;
        unsigned char *d = ler_tudo(argv[2], &tam);
        printf("%d\n", bloqueo_validar(d, tam));
        free(d);
        return 0;
    }
    if ((argc == 5 && !strcmp(argv[1], "consultar")) || (argc == 6 && !strcmp(argv[1], "medir"))) {
        uint32_t tam;
        unsigned char *d = ler_tudo(argv[2], &tam);
        if (bloqueo_validar(d, tam) != 0) { fprintf(stderr, "tabla no válida\n"); return 1; }
        const bloqueo_cabecera_t *c = (const bloqueo_cabecera_t *)d;
        const uint64_t *bloq = (const uint64_t *)(d + sizeof *c), *perm = bloq + c->n_bloq;
        bool sub = atoi(argv[3]) != 0;
        uint32_t tn;
        char *txt = (char *)ler_tudo(argv[4], &tn);
        txt = realloc(txt, tn + 1);
        txt[tn] = 0;
        /* los nombres, separados por '\n' */
        uint32_t nn = 0;
        for (uint32_t i = 0; i < tn; i++) if (txt[i] == '\n') { txt[i] = 0; nn++; }
        char **nomes = malloc((nn + 1) * sizeof *nomes);
        uint32_t *tams = malloc((nn + 1) * sizeof *tams);
        uint32_t k = 0;
        for (char *p = txt; p < txt + tn && k < nn; p += strlen(p) + 1) { nomes[k] = p; tams[k] = (uint32_t)strlen(p); k++; }
        if (argv[1][0] == 'c') {
            for (uint32_t i = 0; i < k; i++)
                printf("%s %d\n", nomes[i], bloqueo_decidir(nomes[i], tams[i], bloq, c->n_bloq, perm, c->n_perm, sub));
        } else {
            int voltas = atoi(argv[5]);
            volatile unsigned soma = 0;
            double t0 = agora();
            for (int v = 0; v < voltas; v++)
                for (uint32_t i = 0; i < k; i++)
                    soma += (unsigned)bloqueo_decidir(nomes[i], tams[i], bloq, c->n_bloq, perm, c->n_perm, sub);
            double t = agora() - t0;
            printf("ns %.1f bloqueadas %u\n", t * 1e9 / ((double)voltas * k), soma);
        }
        free(nomes);
        free(tams);
        free(txt);
        free(d);
        return 0;
    }
    fprintf(stderr, "uso: compilar | validar | consultar | medir\n");
    return 2;
}
