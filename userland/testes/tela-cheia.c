// SPDX-License-Identifier: GPL-2.0-or-later
/* tela-cheia - teste da D044: abre uma janela redimensionável e pede a
 * tela cheia pela chamada de sistema (não pelo F11). A cada tamanho novo,
 * pinta tudo de azul com uma moldura branca e registra o tamanho na serial.
 * "q" ou fechar termina; "t" alterna a tela cheia. */
#include "sagui.h"
#include "gui.h"

static void pintar(jan_t *j)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, 0x2A6FD8);
    g_moldura(j, 0, 0, j->larg, j->alt, 0xFFFFFF);
    g_texto(j, 10, 10, "tela-cheia", 0xFFFFFF);
    jan_mostrar(j);
}

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    jan_t *j = jan_abrir("Tela cheia", 320, 200);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, 100, 60);
    pintar(j);
    bool cheia = true, anima = false;
    int pos = 0;   /* D245: con "a" una barra se mueve y se pide el cuadro sin parar (mide los fps) */
    jan_tela_cheia(j, true);
    registrar("tela-cheia: pedi a tela cheia\n");
    evento_t e;
    for (;;) {
        if (anima) {
            pos = (pos + 8) % (j->larg > 40 ? j->larg - 40 : 1);
            g_retangulo(j, 0, 0, j->larg, j->alt, 0x2A6FD8);
            g_retangulo(j, pos, 30, 40, 40, 0xFFFF00);
            jan_mostrar_area(j, 0, 0, j->larg, j->alt);
        }
        if (proximo_evento(j->id, &e, !anima) != 1) continue;
        if (e.tipo == EV_TECLA && e.tecla == 'a') { anima = !anima; continue; }
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 'q')) break;
        if (e.tipo == EV_TECLA && e.tecla == 't') { cheia = !cheia; jan_tela_cheia(j, cheia); }
        if (e.tipo == EV_TELA) registrar("tela-cheia: tela nova %dx%d\n", e.x, e.y);   /* D059 */
        if (jan_redimensionar(j, &e)) {
            pintar(j);
            registrar("tela-cheia: tamanho %dx%d estado %u\n", j->larg, j->alt, e.tecla);
        }
    }
    jan_fechar(j);
    return 0;
}
