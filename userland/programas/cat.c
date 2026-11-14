// SPDX-License-Identifier: GPL-3.0-only
/* cat (ver) - muestra archivos; sin argumentos (o con "-") copia la entrada a la salida (D097).
 * Opciones: -n numera todas las líneas, -b solo las que no están vacías, -s junta líneas
 * vacías seguidas, -E marca el final de línea con $, -T muestra los tabuladores como ^I. */
#include "sagui.h"

static bool numerar, num_no_vacias, apretar, marcar_fin, marcar_tab;
static int linea_n;
static bool pipe_roto;

static const char *AYUDA =
    "uso: cat [-nbsET] [archivo...]   (\"-\" o ningún archivo = la entrada)\n"
    "  -n  numera todas las líneas      -b  numera solo las no vacías\n"
    "  -s  junta varias líneas vacías   -E  pone $ al final de cada línea\n"
    "  -T  muestra los tabuladores como ^I\n";

/* Copia sin formato: convierte UTF-8 en Latin-1 (D027). Un comienzo de
 * secuencia que caiga al final de un bloque espera al bloque siguiente. */
static int copiar_simple(int fd)
{
    char buf[513];
    size_t pendente = 0;
    int n;
    while ((n = ler(fd, buf + pendente, (unsigned)(sizeof buf - 1 - pendente))) > 0) {
        size_t total = pendente + (size_t)n, sobra;
        size_t m = utf8_para_latin1(buf, total, &sobra);
        if (escrever_tudo(SAIDA, buf, (unsigned)(m - sobra)) < 0) { pipe_roto = true; return 1; }
        if (sobra) buf[0] = buf[m - 1];
        pendente = sobra;
    }
    if (pendente) escrever_tudo(SAIDA, buf, (unsigned)pendente);
    return n < 0;
}

/* Con opciones: línea a línea. */
static int copiar_lineas(int fd)
{
    leitor_linhas_t l;
    static char linha[1024], saida[1100];
    static bool ultima_vacia;
    leitor_iniciar(&l, fd, true);
    int n;
    bool salto;
    while ((n = leitor_linha(&l, linha, sizeof linha, &salto)) >= 0) {
        if (apretar && n == 0 && ultima_vacia) continue;
        ultima_vacia = n == 0;
        int k = 0;
        if (numerar || (num_no_vacias && n > 0)) k = formatar_texto(saida, sizeof saida, "%6d\t", ++linea_n);
        for (int i = 0; i < n && k < (int)sizeof saida - 4; i++) {
            if (marcar_tab && linha[i] == '\t') { saida[k++] = '^'; saida[k++] = 'I'; }
            else saida[k++] = linha[i];
        }
        if (marcar_fin) saida[k++] = '$';
        if (salto) saida[k++] = '\n';
        if (escrever_tudo(SAIDA, saida, (unsigned)k) < 0) { pipe_roto = true; return 1; }
    }
    return 0;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "nbsET", &o)) >= 0) {
        switch (r) {
        case 'n': numerar = true; break;
        case 'b': num_no_vacias = true; numerar = false; break;
        case 's': apretar = true; break;
        case 'E': marcar_fin = true; break;
        case 'T': marcar_tab = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("cat", r, &o);
        }
    }
    if (num_no_vacias) numerar = false;
    bool formato = numerar || num_no_vacias || apretar || marcar_fin || marcar_tab;
    int erros = 0;
    if (o.indice >= argc) return formato ? copiar_lineas(ENTRADA) : copiar_simple(ENTRADA);
    for (int i = o.indice; i < argc && !pipe_roto; i++) {
        int fd = abrir_entrada("cat", argv[i]);
        if (fd < 0) { erros++; continue; }
        erros += formato ? copiar_lineas(fd) : copiar_simple(fd);
        fechar_entrada(fd);
    }
    return erros ? 1 : 0;
}
