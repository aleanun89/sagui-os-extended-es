// SPDX-License-Identifier: GPL-2.0-or-later
/* head (cabeca) - primeras líneas (o bytes) de archivos o de la entrada (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: head [-n N | -N] [-c N] [-q] [-v] [archivo...]\n"
    "  -n N  las primeras N líneas (por defecto 10)    -c N  los primeros N bytes\n"
    "  -q    nunca escribe cabeceras                   -v  siempre escribe la cabecera\n"
    "Con varios archivos pone «==> nombre <==» antes de cada uno. Sin archivo lee la entrada.\n";

static int bytes_modo, lineas = 10;
static bool modo_bytes;

/* Copia las primeras líneas (o bytes) de fd; devuelve 1 si el pipe de salida se cerró. */
static int cabeza(int fd)
{
    char buf[1025];
    int n, restantes = modo_bytes ? bytes_modo : lineas;
    size_t pendente = 0;
    if (restantes <= 0) return 0;
    while ((n = ler(fd, buf + pendente, (unsigned)(sizeof buf - 1 - pendente))) > 0) {
        size_t total = pendente + (size_t)n, sobra = 0;
        size_t m = modo_bytes ? total : utf8_para_latin1(buf, total, &sobra);
        size_t fin = m - sobra;
        if (modo_bytes) {
            if ((int)fin > restantes) fin = (size_t)restantes;
            restantes -= (int)fin;
        } else {
            size_t i = 0;
            for (; i < fin && restantes > 0; i++)
                if (buf[i] == '\n') restantes--;
            fin = i;
        }
        if (escrever_tudo(SAIDA, buf, (unsigned)fin) < 0) return 1;
        if (restantes <= 0) return 0;
        if (sobra) buf[0] = buf[m - 1];
        pendente = sobra;
    }
    return 0;
}

int principal(int argc, char **argv)
{
    bool callar = false, siempre = false;
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = { { "lines", 'n' }, { "bytes", 'c' }, { "quiet", 'q' }, { "silent", 'q' }, { "verbose", 'v' }, { NULL, 0 } };
    o.numerica = true;
    o.longas = largas;
    int r;
    while ((r = obter_opcao(argc, argv, "n:c:qv", &o)) >= 0) {
        int v;
        switch (r) {
        case 'n': case '#':
            if (!texto_para_numero(o.valor, &v)) { imprimir_em(ERROS, "head: %s: número de líneas no válido\n", o.valor); return 2; }
            lineas = v; modo_bytes = false;
            break;
        case 'c':
            if (!texto_para_numero(o.valor, &v)) { imprimir_em(ERROS, "head: %s: número de bytes no válido\n", o.valor); return 2; }
            bytes_modo = v; modo_bytes = true;
            break;
        case 'q': callar = true; break;
        case 'v': siempre = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("head", r, &o);
        }
    }
    int nargs = argc - o.indice, erros = 0;
    if (nargs == 0) return cabeza(ENTRADA) ? 1 : 0;
    for (int i = 0; i < nargs; i++) {
        const char *nome = argv[o.indice + i];
        int fd = abrir_entrada("head", nome);
        if (fd < 0) { erros++; continue; }
        if ((nargs > 1 || siempre) && !callar) {
            char t[300];
            formatar_texto(t, sizeof t, "%s==> %s <==\n", i ? "\n" : "", compara_texto(nome, "-") == 0 ? "entrada estándar" : nome);
            escrever_texto(SAIDA, t);
        }
        int cerrado = cabeza(fd);
        fechar_entrada(fd);
        if (cerrado) break;
    }
    return erros ? 1 : 0;
}
