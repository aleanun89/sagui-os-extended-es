// SPDX-License-Identifier: GPL-2.0-or-later
/* tee - copia la entrada a la salida y a archivos (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: tee [-a] [archivo...]\n"
    "  -a  añade al final de los archivos en vez de reemplazarlos\n"
    "Lee la entrada y la escribe en la salida y en cada archivo (ej.: ls | tee lista.txt | wc -l).\n";

int principal(int argc, char **argv)
{
    bool anadir = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "ai", &o)) >= 0) {
        if (r == 'a') anadir = true;
        else if (r == 'i') continue;
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("tee", r, &o);
    }
    int fds[16], nf = 0, erros = 0;
    for (int i = o.indice; i < argc && nf < 16; i++) {
        int fd = abrir(argv[i], ABRIR_ESCREVER | ABRIR_CRIAR | (anadir ? ABRIR_ACRESCENTAR : ABRIR_TRUNCAR));
        if (fd < 0) { imprimir_em(ERROS, "tee: %s: %s\n", argv[i], nome_erro(fd)); erros++; continue; }
        fds[nf++] = fd;
    }
    char buf[1024];
    int n;
    bool salida_viva = true;
    while ((n = ler(ENTRADA, buf, sizeof buf)) > 0) {
        if (salida_viva && escrever_tudo(SAIDA, buf, (unsigned)n) < 0) salida_viva = false;
        for (int i = 0; i < nf; i++) escrever_tudo(fds[i], buf, (unsigned)n);
        if (!salida_viva && nf == 0) break;
    }
    for (int i = 0; i < nf; i++) fechar(fds[i]);
    return erros ? 1 : 0;
}
