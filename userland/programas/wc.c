// SPDX-License-Identifier: GPL-3.0-only
/* wc (conta) - cuenta líneas, palabras y letras (bytes) de archivos o de la entrada (D097).
 * Sin opciones, el formato de siempre («N líneas, N palabras, N letras») cuando se
 * cuenta un solo origen; con opciones o con varios archivos, columnas como en Unix. */
#include "sagui.h"

typedef struct { unsigned linhas, palavras, bytes, maxlinha; } cont_t;

static const char *AYUDA =
    "uso: wc [-lwcmL] [archivo...]   (sin archivos, cuenta la entrada)\n"
    "  -l líneas   -w palabras   -c bytes   -m caracteres   -L la línea más larga\n"
    "Sin opciones: líneas, palabras y bytes. Con varios archivos añade una línea «total».\n";

static int contar(int fd, cont_t *c)
{
    char buf[512];
    bool dentro = false;
    unsigned col = 0;
    int n;
    while ((n = ler(fd, buf, sizeof buf)) > 0) {
        for (int i = 0; i < n; i++) {
            c->bytes++;
            if (buf[i] == '\n') { c->linhas++; if (col > c->maxlinha) c->maxlinha = col; col = 0; }
            else col++;
            if (eh_espaco(buf[i])) dentro = false;
            else if (!dentro) { dentro = true; c->palavras++; }
        }
    }
    if (col > c->maxlinha) c->maxlinha = col;
    return n < 0;
}

int principal(int argc, char **argv)
{
    bool l = false, w = false, c = false, m = false, maior = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "lwcmL", &o)) >= 0) {
        switch (r) {
        case 'l': l = true; break;
        case 'w': w = true; break;
        case 'c': c = true; break;
        case 'm': m = true; break;
        case 'L': maior = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("wc", r, &o);
        }
    }
    bool sem_opcoes = !(l || w || c || m || maior);
    int nargs = argc - o.indice;
    if (sem_opcoes && nargs <= 1) {   /* el formato de siempre */
        cont_t t = { 0, 0, 0, 0 };
        int fd = nargs ? abrir_entrada("wc", argv[o.indice]) : ENTRADA;
        if (fd < 0) return 1;
        int e = contar(fd, &t);
        fechar_entrada(fd);
        imprimir("%u líneas, %u palabras, %u letras\n", t.linhas, t.palavras, t.bytes);
        return e;
    }
    if (sem_opcoes) { l = w = c = true; }
    int ncol = (l ? 1 : 0) + (w ? 1 : 0) + ((c || m) ? 1 : 0) + (maior ? 1 : 0);
    int ancho = (ncol == 1 && nargs <= 1) ? 0 : 7;
    cont_t total = { 0, 0, 0, 0 };
    int erros = 0, ok = 0;
    for (int i = 0; i < (nargs ? nargs : 1); i++) {
        const char *nome = nargs ? argv[o.indice + i] : NULL;
        int fd = nome ? abrir_entrada("wc", nome) : ENTRADA;
        if (fd < 0) { erros++; continue; }
        cont_t t = { 0, 0, 0, 0 };
        erros += contar(fd, &t);
        fechar_entrada(fd);
        ok++;
        total.linhas += t.linhas; total.palavras += t.palavras; total.bytes += t.bytes;
        if (t.maxlinha > total.maxlinha) total.maxlinha = t.maxlinha;
        char s[160];
        int k = 0;
        if (l) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, t.linhas);
        if (w) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, t.palavras);
        if (c || m) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, t.bytes);
        if (maior) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, t.maxlinha);
        if (nome) formatar_texto(s + k, sizeof s - (size_t)k, " %s", nome);
        saida_linha(s, -1);
    }
    if (nargs > 1 && ok) {
        char s[160];
        int k = 0;
        if (l) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, total.linhas);
        if (w) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, total.palavras);
        if (c || m) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, total.bytes);
        if (maior) k += formatar_texto(s + k, sizeof s - (size_t)k, "%*u", ancho, total.maxlinha);
        formatar_texto(s + k, sizeof s - (size_t)k, " total");
        saida_linha(s, -1);
    }
    return erros ? 1 : 0;
}
