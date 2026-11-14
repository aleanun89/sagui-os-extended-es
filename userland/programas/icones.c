// SPDX-License-Identifier: GPL-2.0-or-later
/* icones - la biblioteca de iconos del SaguiOS, a la vista (D135).
 *
 * Muestra todos los iconos de la biblioteca (userland/libsagui/icones.c) en una
 * rejilla, cada uno con su nombre. Al pulsar uno, su nombre sale en la barra
 * de abajo y en el registro del sistema (para las pruebas). */
#include "sagui.h"
#include "gui.h"
#include "icones.h"

#define MARGEM 8
#define ALT_CABECALHO 24
#define ALT_RODAPE 22
#define LARG_CELULA 98      /* el ancho de una celda de la rejilla */
#define ESCALA 2            /* los iconos se dibujan a doble (32x32) */

static jan_t *j;
static int escolhido = -1;
static int colunas, linhas, larg_celula, alt_celula, escala;

/* Reparte los iconos en la rejilla, según el tamaño de la ventana. */
static void calcula(void)
{
    int n = icones_total();
    int util = j->larg - 2 * MARGEM;
    colunas = util / LARG_CELULA;
    if (colunas < 1) colunas = 1;
    if (colunas > n) colunas = n;
    linhas = (n + colunas - 1) / colunas;
    larg_celula = util / colunas;
    alt_celula = (j->alt - ALT_CABECALHO - ALT_RODAPE) / (linhas > 0 ? linhas : 1);
    /* Si la celda no da para el icono a doble, se dibuja a tamaño natural. */
    escala = (alt_celula >= ESCALA * 16 + 18) ? ESCALA : 1;
}

static void desenhar(void)
{
    const icone_t *icones = icones_todos();
    int n = icones_total();
    char titulo[96];

    g_retangulo(j, 0, 0, j->larg, ALT_CABECALHO, COR_FUNDO);
    formatar_texto(titulo, sizeof titulo, "Iconos del SaguiOS: %d iconos de 16x16", n);
    rotulo(j, MARGEM, 4, titulo, COR_TEXTO);
    jan_mostrar_area(j, 0, 0, j->larg, ALT_CABECALHO);

    for (int i = 0; i < n; i++) {
        int c = i % colunas;
        int l = i / colunas;
        int x = MARGEM + c * larg_celula;
        int y = ALT_CABECALHO + l * alt_celula;
        int tam = 16 * escala;
        /* El icono, centrado en su celda. */
        icone_desenhar_indice(j, x + (larg_celula - tam) / 2, y + 2, i, escala);
        /* El nombre debajo, recortado a lo que cabe en la celda. */
        int max = larg_celula / 8;
        const char *nome = icones[i].chave;
        int len = 0;
        while (nome[len] && len < max) len++;
        if (len > 0 && y + tam + 4 + 16 <= j->alt - ALT_RODAPE) {
            char texto[32];
            if (tam_texto(nome) > (size_t)max) {
                copia_texto(texto, nome, (size_t)max);
                texto[max - 1] = '.';
            } else {
                copia_texto(texto, nome, sizeof texto);
            }
            rotulo(j, x + (larg_celula - len * 8) / 2, y + tam + 4, texto,
                   i == escolhido ? COR_SELECAO : COR_TEXTO_FRACO);
        }
    }
    jan_mostrar(j);

    /* La barra de abajo: el icono que se ha pulsado. */
    g_retangulo(j, 0, j->alt - ALT_RODAPE, j->larg, ALT_RODAPE, 0xE6E9EC);
    g_retangulo(j, 0, j->alt - ALT_RODAPE, j->larg, 1, COR_BORDA);
    if (escolhido >= 0)
        formatar_texto(titulo, sizeof titulo, "Icono: %s", icones[escolhido].chave);
    else
        copia_texto(titulo, "Pulsa un icono para ver su nombre", sizeof titulo);
    rotulo(j, MARGEM, j->alt - ALT_RODAPE + 4, titulo, COR_TEXTO);
    jan_mostrar_area(j, 0, j->alt - ALT_RODAPE, j->larg, ALT_RODAPE);
}

/* El icono que hay en el punto (x, y), o -1. */
static int em(int x, int y)
{
    if (y < ALT_CABECALHO || y >= j->alt - ALT_RODAPE) return -1;
    int c = (x - MARGEM) / larg_celula;
    int l = (y - ALT_CABECALHO) / alt_celula;
    if (c < 0 || c >= colunas || l < 0) return -1;
    int i = l * colunas + c;
    return i < icones_total() ? i : -1;
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    j = jan_abrir("Iconos del SaguiOS", 900, 700);
    if (!j) return 1;
    calcula();
    desenhar();
    registrar("icones: pronto\n");
    registrar("icones: %d icones\n", icones_total());

    evento_t e;
    for (;;) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) break;
        if (e.tipo == EV_CLIQUE) {
            int i = em(e.x, e.y);
            if (i >= 0) {
                escolhido = i;
                desenhar();
                registrar("icones: escolhido %s\n", icones_todos()[i].chave);
            }
        }
        if (jan_redimensionar(j, &e)) {
            calcula();
            desenhar();
        }
    }
    jan_fechar(j);
    return 0;
}
