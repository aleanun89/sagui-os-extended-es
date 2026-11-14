// SPDX-License-Identifier: GPL-2.0-or-later
/* uniq - quita (o cuenta) líneas repetidas consecutivas (D097). Úsalo detrás de sort. */
#include "sagui.h"

static const char *AYUDA =
    "uso: uniq [-c] [-d] [-u] [-i] [entrada [salida]]\n"
    "  -c  antepone cuántas veces se repite     -d  solo las líneas repetidas\n"
    "  -u  solo las líneas que no se repiten    -i  ignora mayúsculas\n";

static bool cuenta, rep, solo_unicas, ignorar;

static int minus(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : (c >= 0xC0 && c <= 0xDE && c != 0xD7) ? c + 32 : c; }

static bool iguales(const char *a, const char *b)
{
    for (;; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (ignorar) { x = minus(x); y = minus(y); }
        if (x != y) return false;
        if (!x) return true;
    }
}

static int emitir(int fd, const char *l, int veces)
{
    if (rep && veces < 2) return 0;
    if (solo_unicas && veces > 1) return 0;
    char s[1100];
    int k = 0;
    if (cuenta) k = formatar_texto(s, sizeof s, "%7d ", veces);
    k += formatar_texto(s + k, sizeof s - (size_t)k, "%s\n", l);
    return escrever_tudo(fd, s, (unsigned)k);
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "cduiD", &o)) >= 0) {
        switch (r) {
        case 'c': cuenta = true; break;
        case 'd': rep = true; break;
        case 'u': solo_unicas = true; break;
        case 'i': ignorar = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("uniq", r, &o);
        }
    }
    int fd = ENTRADA, sal = SAIDA;
    if (o.indice < argc) { fd = abrir_entrada("uniq", argv[o.indice]); if (fd < 0) return 1; }
    if (o.indice + 1 < argc) {
        sal = abrir(argv[o.indice + 1], ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (sal < 0) { imprimir_em(ERROS, "uniq: %s: %s\n", argv[o.indice + 1], nome_erro(sal)); return 1; }
    }
    leitor_linhas_t l;
    static char actual[1024], nueva[1024];
    leitor_iniciar(&l, fd, true);
    int n = leitor_linha(&l, actual, sizeof actual, NULL), veces = 1;
    if (n < 0) return 0;
    int st = 0;
    while ((n = leitor_linha(&l, nueva, sizeof nueva, NULL)) >= 0) {
        if (iguales(actual, nueva)) { veces++; continue; }
        if (emitir(sal, actual, veces) < 0) { st = 1; break; }
        memcpy(actual, nueva, (size_t)n + 1);
        veces = 1;
    }
    if (!st) emitir(sal, actual, veces);
    fechar_entrada(fd);
    if (sal != SAIDA) fechar(sal);
    return st;
}
