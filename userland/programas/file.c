// SPDX-License-Identifier: GPL-2.0-or-later
/* file - dice de qué tipo es cada archivo, mirando su contenido (D232). */
#include "file_logica.h"

static const char *AYUDA =
    "uso: file [-b] archivo...\n"
    "  Reconoce por la cabecera: ELF, PNG, JPEG, GIF, BMP, WebP, PDF, ZIP, gzip, bzip2, xz, tar, MP3, WAV, Ogg, FLAC,\n"
    "  MIDI, MP4, MPEG, SQLite, guiones (#!), XML, HTML y texto (ASCII, UTF-8 o ISO-8859). Lo demás es \"datos\".\n"
    "  -b  sin el nombre del archivo delante\n";

int principal(int argc, char **argv)
{
    bool corto = false;
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "b", &o)) >= 0) {
        if (r == 'b') corto = true;
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("file", r, &o);
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "uso: file archivo...  (file --help)\n"); return 2; }
    for (int i = o.indice; i < argc; i++) {
        char t[200];
        info_arquivo_t inf;
        int e = info_arquivo(argv[i], &inf);
        if (e < 0) { imprimir_em(ERROS, "file: %s: %s\n", argv[i], nome_erro(e)); codigo = 1; continue; }
        if (inf.tipo == TIPO_PASTA) copia_texto(t, "carpeta", sizeof t);
        else {
            int fd = abrir(argv[i], ABRIR_LER);
            if (fd < 0) { imprimir_em(ERROS, "file: %s: %s\n", argv[i], nome_erro(fd)); codigo = 1; continue; }
            static uint8_t cab[4096];
            int n = 0, k;
            while (n < (int)sizeof cab && (k = ler(fd, cab + n, (unsigned)(sizeof cab - (unsigned)n))) > 0) n += k;
            fechar(fd);
            file_tipo(cab, n, t, sizeof t);
        }
        if (corto) imprimir("%s\n", t); else imprimir("%s: %s\n", argv[i], t);
    }
    return codigo;
}
