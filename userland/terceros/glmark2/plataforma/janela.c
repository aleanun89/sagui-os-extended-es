// SPDX-License-Identifier: GPL-3.0-only
/* janela.c - las ventanas de la libsagui para el estado nativo de glmark2 (D481).
 * Las cabeceras de la libsagui y del kernel son de C (no compilan como C++): aquí está todo lo que toca
 * jan_t y evento_t, y native-state-sagui.cpp solo ve punteros opacos. */
#include "sagui.h"
#include "gui.h"
#include "janela.h"

#define TECLA_ESC 27

void *gm_jan_abrir(const char *titulo, int largura, int altura)
{
    return jan_abrir(titulo, largura, altura);
}

void gm_jan_fechar(void *j) { jan_fechar(j); }
void gm_jan_mostrar(void *j) { jan_mostrar(j); }
int gm_jan_larg(void *j) { return ((jan_t *)j)->larg; }
int gm_jan_alt(void *j) { return ((jan_t *)j)->alt; }
unsigned gm_ms(void) { return tempo_ms(); }

/* Pide la pantalla completa y espera (hasta 1,5 s) a que el kernel mande el EV_REDIMENSIONAR de aceptarla. 1 = lograda. */
int gm_jan_completa(void *vj)
{
    jan_t *j = vj;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 0, 0);
    if (!jan_tela_cheia(j, true))
        return 0;
    unsigned t0 = tempo_ms();
    while (tempo_ms() - t0 < 1500) {
        evento_t e;
        if (esperar_evento(j->id, &e, 50) && e.tipo == EV_REDIMENSIONAR && jan_redimensionar(j, &e))
            return e.tecla == JAN_ESTADO_TELA_CHEIA;
    }
    return 0;
}

/* Vacía los eventos pendientes; 1 = hay que salir (cerrar la ventana o Esc). */
int gm_jan_eventos(void *vj)
{
    jan_t *j = vj;
    int sair = 0;
    evento_t e;
    while (proximo_evento(j->id, &e, false)) {
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC))
            sair = 1;
        else if (e.tipo == EV_REDIMENSIONAR)
            jan_redimensionar(j, &e);
    }
    return sair;
}
