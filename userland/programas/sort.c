// SPDX-License-Identifier: GPL-2.0-or-later
/* sort (ordenar) - ordena las líneas de archivos o de la entrada (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: sort [-rnufb] [-k CAMPO[nr]] [-t SEP] [-o archivo] [archivo...]\n"
    "  -r  orden inverso          -n  compara como números      -u  quita líneas repetidas\n"
    "  -f  ignora mayúsculas      -b  ignora espacios iniciales  -c  solo comprueba si está ordenado\n"
    "  -k N   ordena por el campo N (hasta el final de la línea); N puede llevar n, r, f: -k2nr\n"
    "  -t C   separador de campos (por defecto, espacios)    -o F  escribe el resultado en F\n";

static bool inverso, numerico, unicos, plegar, saltar_blancos, solo_comprobar;
static int campo, sep = -1;
static bool campo_num, campo_inv, campo_plegar;

static int minus(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : (c >= 0xC0 && c <= 0xDE && c != 0xD7) ? c + 32 : c; }

/* Salta hasta el comienzo del campo k (1 = toda la línea). */
static const char *clave(const char *s)
{
    if (campo <= 1) return s;
    for (int k = 1; k < campo; k++) {
        if (sep >= 0) {
            while (*s && *s != sep) s++;
            if (*s) s++;
        } else {
            while (*s == ' ' || *s == '\t') s++;
            while (*s && *s != ' ' && *s != '\t') s++;
        }
    }
    if (sep < 0) while (*s == ' ' || *s == '\t') s++;   /* los blancos iniciales del campo no cuentan */
    return s;
}

/* Compara dos textos como números decimales (sin desbordar: por dígitos). */
static int cmp_num(const char *a, const char *b)
{
    while (*a == ' ' || *a == '\t') a++;
    while (*b == ' ' || *b == '\t') b++;
    bool na = *a == '-', nb = *b == '-';
    if (na || *a == '+') a++;
    if (nb || *b == '+') b++;
    while (*a == '0') a++;
    while (*b == '0') b++;
    const char *ea = a, *eb = b;
    while (*ea >= '0' && *ea <= '9') ea++;
    while (*eb >= '0' && *eb <= '9') eb++;
    int la = (int)(ea - a), lb = (int)(eb - b);
    bool za = la == 0 && !(*ea == '.' && ea[1] >= '1' && ea[1] <= '9'), zb = lb == 0 && !(*eb == '.' && eb[1] >= '1' && eb[1] <= '9');
    if (za && zb) return 0;
    if (za) na = false;
    if (zb) nb = false;
    int r;
    if (na != nb) return na ? -1 : 1;
    if (la != lb) r = la < lb ? -1 : 1;
    else {
        r = 0;
        for (int i = 0; i < la && !r; i++) r = a[i] < b[i] ? -1 : a[i] > b[i] ? 1 : 0;
        if (!r && (*ea == '.' || *eb == '.')) {
            const char *fa = *ea == '.' ? ea + 1 : "", *fb = *eb == '.' ? eb + 1 : "";
            while (!r && ((*fa >= '0' && *fa <= '9') || (*fb >= '0' && *fb <= '9'))) {
                int ca = (*fa >= '0' && *fa <= '9') ? *fa++ : '0', cb = (*fb >= '0' && *fb <= '9') ? *fb++ : '0';
                r = ca < cb ? -1 : ca > cb ? 1 : 0;
            }
        }
    }
    return na ? -r : r;
}

static int cmp_texto(const char *a, const char *b, bool pleg)
{
    if (saltar_blancos) { while (*a == ' ' || *a == '\t') a++; while (*b == ' ' || *b == '\t') b++; }
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (pleg) { x = minus(x); y = minus(y); }
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}

static int comparar(const void *pa, const void *pb)
{
    const char *la = pa, *lb = pb;
    const char *a = clave(la), *b = clave(lb);
    bool num = numerico || campo_num, inv = inverso || campo_inv, pleg = plegar || campo_plegar;
    int r = num ? cmp_num(a, b) : cmp_texto(a, b, pleg);
    if (r == 0 && !unicos) r = cmp_texto(la, lb, false);   /* desempate: la línea entera, como sort */
    return inv ? -r : r;
}

int principal(int argc, char **argv)
{
    const char *salida = NULL;
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = { { "reverse", 'r' }, { "numeric-sort", 'n' }, { "unique", 'u' }, { "key", 'k' },
                                            { "field-separator", 't' }, { "output", 'o' }, { "ignore-case", 'f' }, { NULL, 0 } };
    o.longas = largas;
    int r;
    while ((r = obter_opcao(argc, argv, "rnufbck:t:o:", &o)) >= 0) {
        switch (r) {
        case 'r': inverso = true; break;
        case 'n': numerico = true; break;
        case 'u': unicos = true; break;
        case 'f': plegar = true; break;
        case 'b': saltar_blancos = true; break;
        case 'c': solo_comprobar = true; break;
        case 'k': {
            const char *s = o.valor;
            int v = 0;
            if (*s < '0' || *s > '9') { imprimir_em(ERROS, "sort: -k %s: campo no válido\n", o.valor); return 2; }
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            campo = v;
            for (; *s; s++) {
                if (*s == 'n') campo_num = true;
                else if (*s == 'r') campo_inv = true;
                else if (*s == 'f') campo_plegar = true;
                else if (*s == ',') break;   /* -k2,2: solo se usa el comienzo */
            }
            break;
        }
        case 't': sep = (unsigned char)o.valor[0]; break;
        case 'o': salida = o.valor; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("sort", r, &o);
        }
    }
    /* lee todo a un solo bloque: líneas separadas por \0 */
    unsigned cap = 8192, n = 0;
    char *b = aloca(cap);
    if (!b) { imprimir_em(ERROS, "sort: sin memoria\n"); return 1; }
    int erros = 0, nargs = argc - o.indice;
    for (int i = 0; i < (nargs ? nargs : 1); i++) {
        int fd = nargs ? abrir_entrada("sort", argv[o.indice + i]) : ENTRADA;
        if (fd < 0) { erros++; continue; }
        unsigned m;
        char *t = ler_todo_fd(fd, &m);
        fechar_entrada(fd);
        if (!t) { imprimir_em(ERROS, "sort: sin memoria\n"); return 1; }
        m = (unsigned)utf8_para_latin1(t, m, NULL);
        if (n + m + 2 > cap) {
            while (n + m + 2 > cap) cap *= 2;
            char *nb = realoca(b, cap);
            if (!nb) { imprimir_em(ERROS, "sort: sin memoria\n"); return 1; }
            b = nb;
        }
        memcpy(b + n, t, m);
        n += m;
        if (m && t[m - 1] != '\n') b[n++] = '\n';   /* la última línea sin salto también cuenta */
        libera(t);
    }
    int nl = 0;
    for (unsigned i = 0; i < n; i++) if (b[i] == '\n') nl++;
    char **v = aloca((size_t)(nl ? nl : 1) * sizeof *v);
    if (!v) { imprimir_em(ERROS, "sort: sin memoria\n"); return 1; }
    int k = 0;
    char *ini = b;
    for (unsigned i = 0; i < n; i++)
        if (b[i] == '\n') { b[i] = 0; v[k++] = ini; ini = b + i + 1; }
    if (solo_comprobar) {
        for (int i = 1; i < nl; i++)
            if (comparar(v[i - 1], v[i]) > 0 || (unicos && comparar(v[i - 1], v[i]) == 0)) {
                imprimir_em(ERROS, "sort: la línea %d no está en orden\n", i + 1);
                return 1;
            }
        return 0;
    }
    ordenar_ponteiros((void **)v, nl, comparar);
    int fd = SAIDA;
    if (salida) {
        fd = abrir(salida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) { imprimir_em(ERROS, "sort: %s: %s\n", salida, nome_erro(fd)); return 2; }
    }
    for (int i = 0; i < nl; i++) {
        if (unicos && i > 0 && comparar(v[i - 1], v[i]) == 0) continue;
        size_t len = tam_texto(v[i]);
        if (escrever_tudo(fd, v[i], (unsigned)len) < 0 || escrever_tudo(fd, "\n", 1) < 0) break;
    }
    if (fd != SAIDA) fechar(fd);
    return erros ? 2 : 0;
}
