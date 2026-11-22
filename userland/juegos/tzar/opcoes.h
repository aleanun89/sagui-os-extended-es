/* SPDX-License-Identifier: GPL-3.0-or-later */
/* opcoes.h - opciones del juego Tzar que se guardan (tzar.cfg) y modos de pantalla: ventana o pantalla completa,
 * resolución del dibujo, escalado entero o ajustado con bandas negras, filtro nítido o suave e interfaz x2 (D417).
 *
 * Código propio (no sale de ReTzar; la regla de la interfaz x2 sigue a frontend/view.rs hud_layout: el mayor factor
 * entero que deja al menos 800x600, porque la interfaz original no cabe en menos). Sin SDL: el programa del juego
 * (jogo.c) pide el tamaño de la pantalla y aplica lo que dice tz_modo_calcular; las pruebas lo llaman sin pantalla. */
#ifndef TZAR_OPCOES_H
#define TZAR_OPCOES_H

#include <stdint.h>

enum { TZ_ESC_INTEIRA = 0, TZ_ESC_AJUSTAR = 1 };

/* resoluciones que ofrece el menú de opciones: 0 = la de la pantalla; las demás, las del original */
#define TZ_NRESOLUCOES 4
extern const int tz_resolucoes[TZ_NRESOLUCOES][2];

typedef struct {
	int tela_cheia;          /* 1 = pantalla completa */
	int resolucao;           /* índice en tz_resolucoes (0 = la de la pantalla) */
	int escala;              /* TZ_ESC_INTEIRA o TZ_ESC_AJUSTAR */
	int suave;               /* 0 = nítido (vecino más próximo, por defecto), 1 = suave (bilineal) */
	int interface;           /* con la resolución de la pantalla: 0 automática, 1-3 la interfaz x n */
	int painel_direita;      /* el panel de la partida a la derecha (LAYOUT800R) */
	int cursor_jogo;         /* 1 = los cursores del juego (por defecto) */
	int volume_som, volume_musica;   /* 0-100 */
	char aleat[240];         /* últimas opciones de la pantalla del mapa aleatorio (línea «ALEAT2 ...» de ui_menu.c), vacía si no hay */
} TzOpcoes;

void tz_opcoes_padrao(TzOpcoes *o);
/* lee tzar.cfg (clave=valor); lo que falta o no vale queda por defecto. 0 si se leyó. */
int tz_opcoes_ler(TzOpcoes *o, const char *ruta);
int tz_opcoes_gravar(const TzOpcoes *o, const char *ruta);
/* texto de la resolución i para el menú («Pantalla», «800x600»...) */
const char *tz_resolucao_nome(int i, char *buf, int n);

/* Lo que sale de las opciones para una salida de ow x oh (la ventana o la pantalla):
 * el lienzo del juego (cw x ch, al menos 800x600) y dónde se pinta escalado en la salida (dst). */
typedef struct {
	int cw, ch;              /* lienzo del juego */
	int ow, oh;              /* salida */
	int dx, dy, dw, dh;      /* rectángulo de destino en la salida (bandas negras alrededor) */
	int fator;               /* factor entero (0 si el escalado no es entero) */
} TzModo;

/* tamaño de la ventana para la resolución elegida fuera de la pantalla completa (cabe en maxw x maxh) */
void tz_modo_janela(const TzOpcoes *o, int maxw, int maxh, int *w, int *h);
/* el lienzo y el destino para una salida de ow x oh */
void tz_modo_calcular(const TzOpcoes *o, int ow, int oh, TzModo *m);
/* el destino de un lienzo dado (cw x ch) en la salida, según el escalado */
void tz_modo_destino(const TzOpcoes *o, int cw, int ch, int ow, int oh, TzModo *m);
/* coordenadas de la salida -> del lienzo (fuera del destino se recorta al borde) */
void tz_modo_ponto(const TzModo *m, int x, int y, int *cx, int *cy);

/* Escala el lienzo (cw x ch, 0xAARRGGBB, sin pitch) a la salida (pitch en píxeles) según el modo y pinta las
 * bandas de negro. suave = bilineal; si no, vecino más próximo (con caminos rápidos para x1 y x2). */
void tz_apresentar_escala(const uint32_t *src, const TzModo *m, uint32_t *dst, int pitch, int suave);

#endif
