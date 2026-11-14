// SPDX-License-Identifier: GPL-2.0-or-later
/* tail (cauda) - últimas líneas (o bytes) de archivos o de la entrada (D097).
 * tail -f sigue el archivo mientras crece (Ctrl+C para salir). */
#include "sagui.h"

static const char *AYUDA =
    "uso: tail [-n N | -N] [-n +N] [-c N] [-f] [-q] [-v] [archivo...]\n"
    "  -n N   las últimas N líneas (por defecto 10); -n +N desde la línea N\n"
    "  -c N   los últimos N bytes\n"
    "  -f     sigue mostrando lo que se añada al archivo (Ctrl+C sale)\n"
    "  -q / -v  sin / con cabecera «==> nombre <==» (con varios archivos, por defecto sí)\n";

static int n_lineas = 10, n_bytes;
static bool modo_bytes, desde_inicio, seguir;

/* Escribe el final (o desde la línea N) de un bloque en memoria. */
static int mostrar_cola(char *b, unsigned n)
{
    n = (unsigned)utf8_para_latin1(b, n, NULL);
    unsigned ini = 0;
    if (modo_bytes) {
        if (desde_inicio) ini = n_bytes > 1 ? (unsigned)(n_bytes - 1) : 0;
        else ini = n > (unsigned)n_bytes ? n - (unsigned)n_bytes : 0;
        if (ini > n) ini = n;
    } else if (desde_inicio) {
        int l = 1;
        while (ini < n && l < n_lineas) { if (b[ini] == '\n') l++; ini++; }
        if (n_lineas <= 1) ini = 0;
    } else {
        /* retrocede N saltos de línea; un salto final no cuenta */
        int cuenta = 0;
        unsigned i = n;
        if (i > 0 && b[i - 1] == '\n') i--;
        while (i > 0 && cuenta < n_lineas) {
            i--;
            if (b[i] == '\n') { cuenta++; if (cuenta == n_lineas) { i++; break; } }
        }
        ini = n_lineas == 0 ? n : i;
    }
    return escrever_tudo(SAIDA, b + ini, n - ini) < 0 ? 1 : 0;
}

static int seguir_arquivo(int fd)
{
    char buf[512];
    for (;;) {
        int n = ler(fd, buf, sizeof buf);
        if (n > 0) { if (escrever_tudo(SAIDA, buf, (unsigned)n) < 0) return 1; }
        else if (n < 0) return 1;
        else dormir(300);
    }
}

int principal(int argc, char **argv)
{
    bool callar = false, siempre = false;
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = { { "lines", 'n' }, { "bytes", 'c' }, { "follow", 'f' }, { "quiet", 'q' }, { "silent", 'q' }, { "verbose", 'v' }, { NULL, 0 } };
    o.numerica = true;
    o.longas = largas;
    int r, v;
    while ((r = obter_opcao(argc, argv, "n:c:fqv", &o)) >= 0) {
        switch (r) {
        case 'n': case 'c': case '#': {
            const char *s = o.valor;
            bool mas = *s == '+';
            if (*s == '+' || *s == '-') s++;
            if (!texto_para_numero(s, &v)) { imprimir_em(ERROS, "tail: %s: número no válido\n", o.valor); return 2; }
            if (r == 'c') { n_bytes = v; modo_bytes = true; } else { n_lineas = v; modo_bytes = false; }
            desde_inicio = mas;
            break;
        }
        case 'f': seguir = true; break;
        case 'q': callar = true; break;
        case 'v': siempre = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("tail", r, &o);
        }
    }
    int nargs = argc - o.indice, erros = 0;
    if (nargs == 0) {
        unsigned n;
        char *b = ler_todo_fd(ENTRADA, &n);
        if (!b) { imprimir_em(ERROS, "tail: sin memoria\n"); return 1; }
        int e = mostrar_cola(b, n);
        libera(b);
        return e;
    }
    for (int i = 0; i < nargs; i++) {
        const char *nome = argv[o.indice + i];
        int fd = abrir_entrada("tail", nome);
        if (fd < 0) { erros++; continue; }
        if ((nargs > 1 || siempre) && !callar) {
            char t[300];
            formatar_texto(t, sizeof t, "%s==> %s <==\n", i ? "\n" : "", nome);
            escrever_texto(SAIDA, t);
        }
        unsigned n;
        char *b = ler_todo_fd(fd, &n);
        if (!b) { imprimir_em(ERROS, "tail: %s: sin memoria\n", nome); erros++; fechar_entrada(fd); continue; }
        if (mostrar_cola(b, n)) { libera(b); fechar_entrada(fd); return 1; }
        libera(b);
        if (seguir && nargs == 1) return seguir_arquivo(fd);   /* el descriptor sigue en el final */
        fechar_entrada(fd);
    }
    return erros ? 1 : 0;
}
