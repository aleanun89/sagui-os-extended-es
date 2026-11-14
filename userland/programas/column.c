// SPDX-License-Identifier: GPL-2.0-or-later
/* column - da formato de columnas a un texto (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: column [-t] [-s SEPARADORES] [-o SEPARADOR] [-c ANCHO] [archivo...]\n"
    "  -t  tabla: alinea en columnas los campos de cada línea (separados por espacios o por -s)\n"
    "  -s  caracteres que separan los campos con -t (por defecto, blanco)   -o  texto que se escribe entre columnas (dos espacios)\n"
    "  Sin -t reparte las líneas en varias columnas para llenar el ancho de la pantalla (-c, 80 por defecto), por filas.\n";

#define MAXL 2000
#define MAXC 24

int principal(int argc, char **argv)
{
    bool tabla = false;
    const char *seps = " \t", *salsep = "  ";
    int ancho = 80;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "ts:o:c:x", &o)) >= 0) {
        switch (r) {
        case 't': tabla = true; break;
        case 's': seps = o.valor; break;
        case 'o': salsep = o.valor; break;
        case 'x': break;
        case 'c': if (!texto_para_numero(o.valor, &ancho) || ancho < 10) { imprimir_em(ERROS, "column: -c: número no válido\n"); return 2; } break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("column", r, &o);
        }
    }
    int fd = abrir_entrada("column", o.indice < argc ? argv[o.indice] : "-");
    if (fd < 0) return 1;
    unsigned tam;
    char *t = ler_todo_fd(fd, &tam);
    fechar_entrada(fd);
    if (!t) { imprimir_em(ERROS, "column: sin memoria\n"); return 1; }
    static char *lin[MAXL];
    int nl = 0;
    for (char *p = t; *p && nl < MAXL;) {
        lin[nl++] = p;
        while (*p && *p != '\n') p++;
        if (*p) *p++ = 0;
    }
    if (!tabla) {
        int mx = 0;
        for (int i = 0; i < nl; i++) { int l = (int)tam_texto(lin[i]); if (l > mx) mx = l; }
        int w = mx + 2, cols = ancho / w;
        if (cols < 1) cols = 1;
        for (int i = 0; i < nl; i += cols) {
            char sal[600];
            int k = 0;
            for (int c = 0; c < cols && i + c < nl; c++) {
                int l = (int)tam_texto(lin[i + c]);
                memcpy(sal + k, lin[i + c], (size_t)l);
                k += l;
                if (c < cols - 1 && i + c + 1 < nl) while (l++ < w) sal[k++] = ' ';
            }
            if (saida_linha(sal, k) < 0) return 0;
        }
        return 0;
    }
    /* tabla: campos por línea */
    static char *campo[MAXL][MAXC];
    static int nc[MAXL];
    int ancho_c[MAXC] = { 0 };
    for (int i = 0; i < nl; i++) {
        char *p = lin[i];
        nc[i] = 0;
        for (;;) {
            while (*p && acha_char(seps, (unsigned char)*p) && (seps[0] == ' ' || seps[0] == '\t' ? true : false)) p++;   /* los blancos se juntan */
            if (!*p) break;
            char *f = p;
            if (nc[i] == MAXC - 1) { campo[i][nc[i]++] = p; break; }
            while (*p && !acha_char(seps, (unsigned char)*p)) p++;
            campo[i][nc[i]++] = f;
            if (!*p) break;
            *p++ = 0;
        }
        for (int c = 0; c < nc[i]; c++) { int l = (int)tam_texto(campo[i][c]); if (l > ancho_c[c]) ancho_c[c] = l; }
    }
    int ns = (int)tam_texto(salsep);
    for (int i = 0; i < nl; i++) {
        char sal[2000];
        int k = 0;
        for (int c = 0; c < nc[i]; c++) {
            int l = (int)tam_texto(campo[i][c]);
            if (k + l + ancho_c[c] + ns >= (int)sizeof sal) break;
            memcpy(sal + k, campo[i][c], (size_t)l);
            k += l;
            if (c < nc[i] - 1) { for (int j = l; j < ancho_c[c]; j++) sal[k++] = ' '; memcpy(sal + k, salsep, (size_t)ns); k += ns; }
        }
        if (saida_linha(sal, k) < 0) return 0;
    }
    return 0;
}
