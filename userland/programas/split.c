// SPDX-License-Identifier: GPL-2.0-or-later
/* split - parte un archivo en trozos (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: split [-l LÍNEAS | -b BYTES] [-d] [archivo [PREFIJO]]\n"
    "  Corta el archivo (o la entrada) en trozos llamados PREFIJOaa, PREFIJOab... (PREFIJO por defecto: x).\n"
    "  -l N  N líneas por trozo (1000 por defecto)   -b N  N bytes por trozo (admite K y M)\n"
    "  -d    sufijos numéricos (x00, x01...) en vez de letras\n";

static int trozo_fd = -1;

static bool abrir_trozo(const char *prefijo, int idx, bool num)
{
    char nom[200];
    if (num) formatar_texto(nom, sizeof nom, "%s%02d", prefijo, idx);
    else formatar_texto(nom, sizeof nom, "%s%c%c", prefijo, 'a' + idx / 26, 'a' + idx % 26);
    if (!num && idx >= 26 * 26) { imprimir_em(ERROS, "split: demasiados trozos (usa -d o trozos mayores)\n"); return false; }
    trozo_fd = abrir(nom, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (trozo_fd < 0) { imprimir_em(ERROS, "split: %s: %s\n", nom, nome_erro(trozo_fd)); return false; }
    return true;
}

int principal(int argc, char **argv)
{
    int lineas = 1000;
    unsigned bytes = 0;
    bool num = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "l:b:d", &o)) >= 0) {
        switch (r) {
        case 'l': if (!texto_para_numero(o.valor, &lineas) || lineas < 1) { imprimir_em(ERROS, "split: -l: número no válido\n"); return 2; } break;
        case 'b': {
            const char *p = o.valor;
            unsigned v = 0;
            while (*p >= '0' && *p <= '9') v = v * 10 + (unsigned)(*p++ - '0');
            if (*p == 'K' || *p == 'k') { v *= 1024; p++; } else if (*p == 'M' || *p == 'm') { v *= 1048576; p++; }
            if (*p || v == 0) { imprimir_em(ERROS, "split: -b: tamaño no válido\n"); return 2; }
            bytes = v;
            break;
        }
        case 'd': num = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("split", r, &o);
        }
    }
    int fd = abrir_entrada("split", o.indice < argc ? argv[o.indice] : "-");
    if (fd < 0) return 1;
    const char *prefijo = o.indice + 1 < argc ? argv[o.indice + 1] : "x";
    static char buf[4096];
    int idx = 0, k, en_trozo_lin = 0;
    unsigned en_trozo_bytes = 0;
    while ((k = ler(fd, buf, sizeof buf)) > 0) {
        int p = 0;
        while (p < k) {
            if (trozo_fd < 0) { if (!abrir_trozo(prefijo, idx++, num)) return 1; en_trozo_lin = 0; en_trozo_bytes = 0; }
            int q = p;
            if (bytes) {
                unsigned quedan = bytes - en_trozo_bytes;
                q = p + (int)((unsigned)(k - p) < quedan ? (unsigned)(k - p) : quedan);
                en_trozo_bytes += (unsigned)(q - p);
                escrever_tudo(trozo_fd, buf + p, (unsigned)(q - p));
                if (en_trozo_bytes >= bytes) { fechar(trozo_fd); trozo_fd = -1; }
            } else {
                bool cerrar = false;
                while (q < k) { if (buf[q++] == '\n' && ++en_trozo_lin >= lineas) { cerrar = true; break; } }
                escrever_tudo(trozo_fd, buf + p, (unsigned)(q - p));
                if (cerrar) { fechar(trozo_fd); trozo_fd = -1; }
            }
            p = q;
        }
    }
    if (trozo_fd >= 0) fechar(trozo_fd);
    fechar_entrada(fd);
    return 0;
}
