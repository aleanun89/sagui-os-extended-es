// SPDX-License-Identifier: GPL-2.0-or-later
/* cut (cortar) - extrae campos o columnas de cada línea (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: cut -f LISTA [-d C] [-s] [archivo...]     cut -c LISTA [archivo...]\n"
    "  -f LISTA  campos separados por el delimitador (por defecto tabulador)\n"
    "  -d C      delimitador de campos (un carácter)    -s  salta las líneas sin delimitador\n"
    "  -c LISTA  columnas (caracteres)\n"
    "LISTA: N, N-M, N-, -M o varias separadas por comas (1,3-5,8-). Se empieza a contar en 1.\n";

#define MAX_RANGOS 16
static int rg_ini[MAX_RANGOS], rg_fin[MAX_RANGOS], nrg;

static bool analizar_lista(const char *s)
{
    while (*s) {
        if (nrg >= MAX_RANGOS) return false;
        int a = 1, b = 1 << 30;
        const char *p = s;
        bool tiene_a = false;
        if (*p >= '0' && *p <= '9') { a = 0; tiene_a = true; while (*p >= '0' && *p <= '9') a = a * 10 + (*p++ - '0'); }
        if (*p == '-') {
            p++;
            if (*p >= '0' && *p <= '9') { b = 0; while (*p >= '0' && *p <= '9') b = b * 10 + (*p++ - '0'); }
            else if (!tiene_a) return false;
        } else if (tiene_a) b = a;
        else return false;
        if (a < 1 || b < a) return false;
        rg_ini[nrg] = a; rg_fin[nrg] = b; nrg++;
        if (*p == ',') p++;
        else if (*p) return false;
        s = p;
    }
    return nrg > 0;
}

static bool elegido(int k)
{
    for (int i = 0; i < nrg; i++) if (k >= rg_ini[i] && k <= rg_fin[i]) return true;
    return false;
}

int principal(int argc, char **argv)
{
    const char *lista_f = NULL, *lista_c = NULL;
    int delim = '\t';
    bool solo_con = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "f:c:d:s", &o)) >= 0) {
        switch (r) {
        case 'f': lista_f = o.valor; break;
        case 'c': lista_c = o.valor; break;
        case 'd':
            if (tam_texto(o.valor) != 1) { imprimir_em(ERROS, "cut: el delimitador debe ser un solo carácter\n"); return 2; }
            delim = (unsigned char)o.valor[0];
            break;
        case 's': solo_con = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("cut", r, &o);
        }
    }
    if (!lista_f && !lista_c) { imprimir_em(ERROS, "cut: indica qué cortar con -f o -c (cut --help)\n"); return 2; }
    if (!analizar_lista(lista_f ? lista_f : lista_c)) { imprimir_em(ERROS, "cut: lista no válida: %s\n", lista_f ? lista_f : lista_c); return 2; }
    int nargs = argc - o.indice, erros = 0;
    static char linha[4096], sal[4096];
    for (int a = 0; a < (nargs ? nargs : 1); a++) {
        int fd = nargs ? abrir_entrada("cut", argv[o.indice + a]) : ENTRADA;
        if (fd < 0) { erros++; continue; }
        leitor_linhas_t l;
        leitor_iniciar(&l, fd, true);
        int n;
        while ((n = leitor_linha(&l, linha, sizeof linha, NULL)) >= 0) {
            int k = 0;
            if (lista_c) {
                for (int i = 0; i < n; i++) if (elegido(i + 1)) sal[k++] = linha[i];
            } else {
                bool hay = false;
                for (int i = 0; i < n; i++) if (linha[i] == delim) { hay = true; break; }
                if (!hay) {
                    if (solo_con) continue;
                    memcpy(sal, linha, (size_t)n);
                    k = n;
                } else {
                    int campo = 1, ini = 0;
                    bool primero = true;
                    for (int i = 0; i <= n; i++) {
                        if (i == n || linha[i] == delim) {
                            if (elegido(campo)) {
                                if (!primero) sal[k++] = (char)delim;
                                memcpy(sal + k, linha + ini, (size_t)(i - ini));
                                k += i - ini;
                                primero = false;
                            }
                            campo++;
                            ini = i + 1;
                        }
                    }
                }
            }
            if (saida_linha(sal, k) < 0) { fechar_entrada(fd); return erros ? 1 : 0; }
        }
        fechar_entrada(fd);
    }
    return erros ? 1 : 0;
}
