// SPDX-License-Identifier: GPL-2.0-or-later
/* touch (tocar) - crea archivos vacíos si no existen (D097). Si ya existen, rescribe su
 * primer byte para que el sistema de archivos actualice la fecha (el kernel no tiene utimes). */
#include "sagui.h"

static const char *AYUDA =
    "uso: touch [-c] archivo...\n"
    "  -c  no crea los archivos que no existan\n"
    "Si el archivo existe, intenta actualizar su fecha de modificación; si no existe lo crea vacío.\n";

int principal(int argc, char **argv)
{
    bool no_crear = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "camf", &o)) >= 0) {
        if (r == 'c') no_crear = true;
        else if (r == 'a' || r == 'm' || r == 'f') continue;
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("touch", r, &o);
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "touch: falta el nombre del archivo (touch --help)\n"); return 2; }
    int erros = 0;
    for (int i = o.indice; i < argc; i++) {
        info_arquivo_t inf;
        if (info_arquivo(argv[i], &inf) == 0) {
            if (inf.tipo == TIPO_PASTA) continue;
            if (inf.tamanho > 0) {
                int fd = abrir(argv[i], ABRIR_LER | ABRIR_ESCREVER);
                if (fd >= 0) {
                    char c;
                    if (ler(fd, &c, 1) == 1 && posicionar(fd, 0, 0) >= 0) escrever(fd, &c, 1);
                    fechar(fd);
                }
            }
            continue;
        }
        if (no_crear) continue;
        int fd = abrir(argv[i], ABRIR_ESCREVER | ABRIR_CRIAR);
        if (fd < 0) { imprimir_em(ERROS, "touch: %s: %s\n", argv[i], nome_erro(fd)); erros++; continue; }
        fechar(fd);
    }
    return erros ? 1 : 0;
}
