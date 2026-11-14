// SPDX-License-Identifier: GPL-2.0-or-later
/* base64 - codifica o decodifica Base64 (D232).
 *   base64 archivo      echo hola | base64      echo aG9sYQo= | base64 -d      base64 -w 0 foto.png */
#include "base64_logica.h"

static const char *AYUDA =
    "uso: base64 [-d] [-i] [-w COLUMNAS] [archivo]\n"
    "  Sin -d codifica el archivo (o la entrada) y escribe el texto en líneas de 76 columnas.\n"
    "  -d  decodifica     -i  al decodificar ignora los caracteres que no son Base64     -w N  columnas (0 = una sola línea)\n";

int principal(int argc, char **argv)
{
    bool dec = false, ign = false;
    int ancho = 76;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "diw:D", &o)) >= 0) {
        switch (r) {
        case 'd': case 'D': dec = true; break;
        case 'i': ign = true; break;
        case 'w': if (!texto_para_numero(o.valor, &ancho)) { imprimir_em(ERROS, "base64: -w: número no válido\n"); return 2; } break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("base64", r, &o);
        }
    }
    if (argc - o.indice > 1) { imprimir_em(ERROS, "base64: sobran argumentos (base64 --help)\n"); return 2; }
    int fd = abrir_entrada("base64", o.indice < argc ? argv[o.indice] : "-");
    if (fd < 0) return 1;
    unsigned tam;
    char *datos = ler_todo_fd(fd, &tam);
    fechar_entrada(fd);
    if (!datos) { imprimir_em(ERROS, "base64: sin memoria\n"); return 1; }
    if (dec) {
        uint8_t *sal = aloca(tam + 4);
        if (!sal) { imprimir_em(ERROS, "base64: sin memoria\n"); return 1; }
        int n = b64_decodificar(datos, (int)tam, sal, (int)tam + 4, ign);
        if (n < 0) { imprimir_em(ERROS, "base64: entrada no válida\n"); return 1; }
        escrever_tudo(SAIDA, sal, (unsigned)n);
        return 0;
    }
    int cap = (int)((tam + 2) / 3 * 4) + 4;
    char *sal = aloca((unsigned)cap);
    if (!sal) { imprimir_em(ERROS, "base64: sin memoria\n"); return 1; }
    int n = b64_codificar((const uint8_t *)datos, (int)tam, sal, cap);
    if (ancho <= 0) {
        escrever_tudo(SAIDA, sal, (unsigned)n);
        if (n) escrever_tudo(SAIDA, "\n", 1);
    } else {
        for (int i = 0; i < n; i += ancho) {
            int k = n - i < ancho ? n - i : ancho;
            if (escrever_tudo(SAIDA, sal + i, (unsigned)k) < 0 || escrever_tudo(SAIDA, "\n", 1) < 0) return 0;
        }
    }
    return 0;
}
