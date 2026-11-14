// SPDX-License-Identifier: GPL-2.0-or-later
/* bc - calculadora de línea de órdenes, versión sencilla (D232).
 *   echo "2+3*4" | bc      echo "scale=2; 10/3" | bc      bc -e "sqrt(2)" -s 4      bc (y se teclea; quit sale)
 * Aritmética de punto fijo con hasta 6 decimales; no es el bc completo (sin funciones propias ni bucles). */
#include "bc_logica.h"

static const char *AYUDA =
    "uso: bc [-q] [-s N] [-e EXPRESIÓN]... [archivo...]\n"
    "  Calcula las expresiones que lee (una por línea, o varias separadas por ;) de la entrada o de archivos.\n"
    "  -e EXPR  calcula esa expresión (se puede repetir)      -s N  decimales de partida (scale, 0 a 6)\n"
    "  -l       como -s 6 (la biblioteca matemática completa no está disponible)   -q  sin saludo\n"
    "Operadores: + - * / % ^ y paréntesis; variables de una letra (a=5); scale=N; sqrt(x); quit.\n"
    "Como el bc de Linux, la división da tantos decimales como scale (0 por defecto): 10/3 es 3, con scale=2 es 3.33.\n"
    "Límites: 6 decimales y números de hasta unos 9 billones.\n";

static bc_t b;

static int linea(const char *l)
{
    char sal[200];
    int r = bc_linea(&b, l, sal, sizeof sal);
    if (r == 1) imprimir("%s\n", sal);
    else if (r < 0) imprimir_em(ERROS, "bc: %s\n", sal);
    return r;
}

int principal(int argc, char **argv)
{
    bc_iniciar(&b);
    char *exprs[16];
    int ne = 0;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "qlws:e:", &o)) >= 0) {
        switch (r) {
        case 'q': case 'w': break;
        case 'l': b.scale = 6; break;
        case 's': {
            int n;
            if (!texto_para_numero(o.valor, &n) || n > 6) { imprimir_em(ERROS, "bc: -s: de 0 a 6\n"); return 2; }
            b.scale = n;
            break;
        }
        case 'e': if (ne < 16) exprs[ne++] = (char *)o.valor; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("bc", r, &o);
        }
    }
    int codigo = 0;
    for (int i = 0; i < ne; i++) if (linea(exprs[i]) < 0) codigo = 1;
    if (ne && o.indice >= argc) return codigo;
    for (int i = o.indice; i < argc || (i == o.indice && o.indice >= argc); i++) {
        const char *nom = i < argc ? argv[i] : "-";
        int fd = abrir_entrada("bc", nom);
        if (fd < 0) { codigo = 1; if (i >= argc) break; continue; }
        leitor_linhas_t l;
        leitor_iniciar(&l, fd, false);
        char buf[512];
        while (leitor_linha(&l, buf, sizeof buf, NULL) >= 0) {
            int q = linea(buf);
            if (q == 2) { fechar_entrada(fd); return codigo; }
            if (q < 0) codigo = 1;
        }
        fechar_entrada(fd);
        if (i >= argc) break;
    }
    return codigo;
}
