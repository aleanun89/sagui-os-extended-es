// SPDX-License-Identifier: GPL-2.0-or-later
/* diff (diferencia) - compara dos archivos línea a línea (D097). Formato normal o unificado (-u). */
#include "sagui.h"

static const char *AYUDA =
    "uso: diff [-u] [-q] [-i] [-b] archivo1 archivo2\n"
    "  -u  formato unificado (--- +++ @@, con 3 líneas de contexto)   -q  solo dice si difieren\n"
    "  -i  ignora mayúsculas    -b  ignora los espacios al comparar\n"
    "Código de salida: 0 si son iguales, 1 si difieren, 2 si hubo errores.\n"
    "Solo compara hasta 3000 líneas distintas en el medio de cada archivo.\n";

static bool ign_may, ign_esp;

static int minus(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static bool iguales(const char *a, const char *b)
{
    for (;;) {
        if (ign_esp) {
            while (*a == ' ' || *a == '\t') a++;
            while (*b == ' ' || *b == '\t') b++;
        }
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (ign_may) { x = minus(x); y = minus(y); }
        if (x != y) return false;
        if (!x) return true;
        a++; b++;
    }
}

static char **cargar(const char *nome, int *n, char **mem)
{
    int fd = abrir_entrada("diff", nome);
    if (fd < 0) return NULL;
    unsigned tam;
    char *b = ler_todo_fd(fd, &tam);
    fechar_entrada(fd);
    if (!b) return NULL;
    tam = (unsigned)utf8_para_latin1(b, tam, NULL);
    b[tam] = 0;
    int nl = 0;
    for (unsigned i = 0; i < tam; i++) if (b[i] == '\n') nl++;
    if (tam && b[tam - 1] != '\n') nl++;
    char **v = aloca((size_t)(nl + 1) * sizeof *v);
    if (!v) return NULL;
    int k = 0;
    char *ini = b;
    for (unsigned i = 0; i < tam && k < nl; i++)
        if (b[i] == '\n') { b[i] = 0; v[k++] = ini; ini = b + i + 1; }
    if (k < nl) v[k++] = ini;
    *n = k;
    *mem = b;
    return v;
}

/* operación por posición del guion: 0 = igual, 1 = borrada de A, 2 = añadida de B */
typedef struct { unsigned char op; int a, b; } paso_t;

int principal(int argc, char **argv)
{
    bool unificado = false, breve = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "uqib", &o)) >= 0) {
        switch (r) {
        case 'u': unificado = true; break;
        case 'q': breve = true; break;
        case 'i': ign_may = true; break;
        case 'b': ign_esp = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("diff", r, &o);
        }
    }
    if (argc - o.indice != 2) { imprimir_em(ERROS, "diff: hacen falta dos archivos (diff --help)\n"); return 2; }
    int na, nb;
    char *ma, *mb;
    char **A = cargar(argv[o.indice], &na, &ma);
    if (!A) return 2;
    char **B = cargar(argv[o.indice + 1], &nb, &mb);
    if (!B) return 2;
    /* quita el prefijo y el sufijo comunes */
    int pre = 0;
    while (pre < na && pre < nb && iguales(A[pre], B[pre])) pre++;
    int suf = 0;
    while (suf < na - pre && suf < nb - pre && iguales(A[na - 1 - suf], B[nb - 1 - suf])) suf++;
    int la = na - pre - suf, lb = nb - pre - suf;
    if (la == 0 && lb == 0) return 0;
    if (breve) { imprimir("Los archivos %s y %s son distintos\n", argv[o.indice], argv[o.indice + 1]); return 1; }
    if (la > 3000 || lb > 3000) { imprimir_em(ERROS, "diff: demasiadas diferencias para comparar\n"); return 2; }
    /* tabla de subsecuencia común más larga */
    int w = lb + 1;
    unsigned short *t = aloca((size_t)(la + 1) * (size_t)w * sizeof *t);
    if (!t) { imprimir_em(ERROS, "diff: sin memoria\n"); return 2; }
    for (int i = la - 1; i >= 0; i--)
        for (int j = lb - 1; j >= 0; j--) {
            if (iguales(A[pre + i], B[pre + j])) t[i * w + j] = (unsigned short)(t[(i + 1) * w + j + 1] + 1);
            else t[i * w + j] = t[(i + 1) * w + j] >= t[i * w + j + 1] ? t[(i + 1) * w + j] : t[i * w + j + 1];
        }
    paso_t *pasos = aloca((size_t)(la + lb + 1) * sizeof *pasos);
    if (!pasos) return 2;
    int np = 0, i = 0, j = 0;
    while (i < la || j < lb) {
        if (i < la && j < lb && iguales(A[pre + i], B[pre + j])) { pasos[np].op = 0; pasos[np].a = pre + i; pasos[np].b = pre + j; np++; i++; j++; }
        else if (j >= lb || (i < la && t[(i + 1) * w + j] >= t[i * w + j + 1])) { pasos[np].op = 1; pasos[np].a = pre + i; pasos[np].b = pre + j; np++; i++; }
        else { pasos[np].op = 2; pasos[np].a = pre + i; pasos[np].b = pre + j; np++; j++; }
    }
    libera(t);
    /* vuelve a añadir el prefijo y el sufijo comunes: sirven de contexto en -u */
    paso_t *todo = aloca((size_t)(pre + np + suf + 1) * sizeof *todo);
    if (!todo) return 2;
    int nt = 0;
    for (int x = 0; x < pre; x++) { todo[nt].op = 0; todo[nt].a = x; todo[nt].b = x; nt++; }
    for (int x = 0; x < np; x++) todo[nt++] = pasos[x];
    for (int x = 0; x < suf; x++) { todo[nt].op = 0; todo[nt].a = na - suf + x; todo[nt].b = nb - suf + x; nt++; }
    libera(pasos);
    pasos = todo;
    np = nt;
    if (!unificado) {
        for (int k = 0; k < np;) {
            if (pasos[k].op == 0) { k++; continue; }
            int e = k;
            while (e < np && pasos[e].op != 0) e++;
            int da = 0, db = 0, a0 = pasos[k].a, b0 = pasos[k].b;
            for (int x = k; x < e; x++) { if (pasos[x].op == 1) da++; else db++; }
            char cab[64], pa[24], pb[24];
            if (da == 0) formatar_texto(pa, sizeof pa, "%d", a0);
            else if (da == 1) formatar_texto(pa, sizeof pa, "%d", a0 + 1);
            else formatar_texto(pa, sizeof pa, "%d,%d", a0 + 1, a0 + da);
            if (db == 0) formatar_texto(pb, sizeof pb, "%d", b0);
            else if (db == 1) formatar_texto(pb, sizeof pb, "%d", b0 + 1);
            else formatar_texto(pb, sizeof pb, "%d,%d", b0 + 1, b0 + db);
            formatar_texto(cab, sizeof cab, "%s%c%s", pa, da == 0 ? 'a' : db == 0 ? 'd' : 'c', pb);
            saida_linha(cab, -1);
            char l[1100];
            for (int x = k; x < e; x++) if (pasos[x].op == 1) { formatar_texto(l, sizeof l, "< %s", A[pasos[x].a]); if (saida_linha(l, -1) < 0) return 1; }
            if (da && db) saida_linha("---", -1);
            for (int x = k; x < e; x++) if (pasos[x].op == 2) { formatar_texto(l, sizeof l, "> %s", B[pasos[x].b]); if (saida_linha(l, -1) < 0) return 1; }
            k = e;
        }
        return 1;
    }
    /* unificado: reúne los cambios en grupos con 3 líneas de contexto */
    char cab[200];
    formatar_texto(cab, sizeof cab, "--- %s", argv[o.indice]);
    saida_linha(cab, -1);
    formatar_texto(cab, sizeof cab, "+++ %s", argv[o.indice + 1]);
    saida_linha(cab, -1);
    const int C = 3;
    for (int k = 0; k < np;) {
        while (k < np && pasos[k].op == 0) k++;
        if (k >= np) break;
        int ini = k - C < 0 ? 0 : k - C;
        int fin = k;   /* último paso incluido (exclusive) */
        int e = k;
        for (;;) {
            while (e < np && pasos[e].op != 0) e++;
            int libres = 0, x = e;
            while (x < np && pasos[x].op == 0) { libres++; x++; }
            if (x >= np || libres > 2 * C) { fin = e + (libres < C ? libres : C); break; }
            e = x;
        }
        if (fin > np) fin = np;
        int ca = 0, cb = 0, a0 = -1, b0 = -1;
        for (int x = ini; x < fin; x++) {
            if (pasos[x].op != 2) { if (a0 < 0) a0 = pasos[x].a; ca++; }
            if (pasos[x].op != 1) { if (b0 < 0) b0 = pasos[x].b; cb++; }
        }
        if (a0 < 0) a0 = pasos[ini].a;
        if (b0 < 0) b0 = pasos[ini].b;
        char h[80];
        formatar_texto(h, sizeof h, "@@ -%d,%d +%d,%d @@", ca ? a0 + 1 : a0, ca, cb ? b0 + 1 : b0, cb);
        saida_linha(h, -1);
        char l[1100];
        for (int x = ini; x < fin; x++) {
            if (pasos[x].op == 0) formatar_texto(l, sizeof l, " %s", A[pasos[x].a]);
            else if (pasos[x].op == 1) formatar_texto(l, sizeof l, "-%s", A[pasos[x].a]);
            else formatar_texto(l, sizeof l, "+%s", B[pasos[x].b]);
            if (saida_linha(l, -1) < 0) return 1;
        }
        k = fin;
    }
    return 1;
}
