// SPDX-License-Identifier: GPL-2.0-or-later
/* truncate - cambia el tamaño de un archivo (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: truncate -s TAMAÑO archivo...\n"
    "  Reduce el archivo (corta lo que sobra) o lo agranda (rellena con ceros). Lo crea si no existe (salvo con -c).\n"
    "  TAMAÑO: bytes, o con sufijo K, M, G (1024); con + o - delante, relativo al tamaño actual (-s +1K).\n"
    "  -c  no crea archivos que no existan\n";

int principal(int argc, char **argv)
{
    const char *tam = NULL;
    bool crear = true;
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "s:c", &o)) >= 0) {
        switch (r) {
        case 's': tam = o.valor; break;
        case 'c': crear = false; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("truncate", r, &o);
        }
    }
    if (!tam || o.indice >= argc) { imprimir_em(ERROS, "uso: truncate -s TAMAÑO archivo...  (truncate --help)\n"); return 2; }
    const char *p = tam;
    int rel = 0;
    if (*p == '+') { rel = 1; p++; } else if (*p == '-') { rel = -1; p++; }
    uint32_t v = 0;
    if (!(*p >= '0' && *p <= '9')) { imprimir_em(ERROS, "truncate: %s: tamaño no válido\n", tam); return 2; }
    while (*p >= '0' && *p <= '9') v = v * 10 + (uint32_t)(*p++ - '0');
    if (*p == 'K' || *p == 'k') { v *= 1024; p++; } else if (*p == 'M' || *p == 'm') { v *= 1048576; p++; } else if (*p == 'G' || *p == 'g') { v *= 1073741824u; p++; }
    if (*p) { imprimir_em(ERROS, "truncate: %s: tamaño no válido\n", tam); return 2; }
    for (int i = o.indice; i < argc; i++) {
        info_arquivo_t inf;
        bool existe = info_arquivo(argv[i], &inf) == 0;
        if (!existe && !crear) continue;
        uint32_t actual = existe ? inf.tamanho : 0, nuevo = v;
        if (rel > 0) nuevo = actual + v;
        else if (rel < 0) nuevo = v > actual ? 0 : actual - v;
        int fd = abrir(argv[i], ABRIR_ESCREVER | ABRIR_CRIAR);
        if (fd < 0) { imprimir_em(ERROS, "truncate: %s: %s\n", argv[i], nome_erro(fd)); codigo = 1; continue; }
        int e = truncar(fd, nuevo);
        fechar(fd);
        if (e < 0) { imprimir_em(ERROS, "truncate: %s: %s\n", argv[i], nome_erro(e)); codigo = 1; }
    }
    return codigo;
}
