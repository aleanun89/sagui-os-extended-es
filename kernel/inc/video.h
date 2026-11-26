// SPDX-License-Identifier: GPL-3.0-only
/* video.h - modo gráfico VESA con doble búfer (H17). */
#ifndef SAGUI_VIDEO_H
#define SAGUI_VIDEO_H
#include "tipos.h"

typedef uint32_t cor_t;   /* 0x00RRGGBB */
#define RGB(r, g, b) ((cor_t)(((r) << 16) | ((g) << 8) | (b)))

bool     video_iniciar(void);     /* false: sin modo gráfico, el sistema queda en texto */
bool     video_ativo(void);
uint32_t video_largura(void);
uint32_t video_altura(void);
/* Con varias pantallas (D290, telas.h) video_largura/altura son el escritorio entero; el modo de
 * la salida del arranque (la que cambia con SIS_MUDAR_MODO) es este: */
uint32_t video_prim_largura(void);
uint32_t video_prim_altura(void);
bool     video_refazer(void);     /* el escritorio cambió de forma: búfer nuevo, salidas en negro (telas.c) */
uint32_t *video_buffer(void);     /* búfer de atrás, 32 bits por píxel, ancho * alto */
uint8_t  *video_placa(void);      /* framebuffer de la tarjeta (para la medida del rapido.c) */
uint32_t  video_pitch(void);
bool      video_formato_direto(void);   /* 32 bits 0x00RRGGBB: la copia no convierte */
uint32_t  video_bpp(void);              /* profundidad de la tarjeta: 15, 16, 24 o 32 (D078) */
uint32_t  video_cor_placa(cor_t c);     /* D305: un color 0x00RRGGBB en el formato de la tarjeta (los rellenos de la GPU) */

void video_pixel(int x, int y, cor_t c);
void video_retangulo(int x, int y, int l, int a, cor_t c);
void video_linha(int x0, int y0, int x1, int y1, cor_t c);
void video_moldura(int x, int y, int l, int a, int espessura, cor_t borda);
void video_caixa(int x, int y, int l, int a, cor_t fundo, cor_t borda);   /* rectángulo con borde */
void video_degrade_vertical(int x, int y, int l, int a, cor_t topo, cor_t base);
void video_rolar(int y, int altura, int linhas, cor_t fundo);   /* sube una franja de la pantalla */
/* D195: mueve un rectángulo del búfer de atrás en (dx, dy) y, con el acelerador
 * 2D, también en la pantalla (true = la GPU la dejó hecha; false = marcado para la CPU). */
bool video_mover(int x, int y, int l, int a, int dx, int dy);

void video_marcar(int x, int y, int l, int a);   /* región cambiada, para copiar la próxima vez */
void video_mostrar(void);                         /* copia lo que cambió a la pantalla */
void video_mostrar_tudo(void);
void video_volcar_cpu(int x0, int y0, int x1, int y1);   /* D387: la copia de la CPU, para medirla */

/* Cambio de modo sin reiniciar (tarjeta de Bochs/QEMU, D059). El contenido
 * del búfer de atrás se pierde: quien llama redibuja todo. La profundidad
 * queda la que ya era en 15 y 16 bits y pasa a 32 en los otros casos (D078):
 * video_bpp_a_quente() dice cuál. */
bool     video_trocar_modo(uint32_t largura, uint32_t altura);
bool     video_troca_a_quente(void);
uint32_t video_bpp_a_quente(void);
uint32_t video_max_largura(void);   /* D472: máximo del cambio en caliente (BGA o virtio-gpu) */
uint32_t video_max_altura(void);
uint32_t video_memoria_mapeada(void);   /* bytes del framebuffer en la ventana del kernel */

/* Cursor del mouse: dibujado encima, al mostrar (H19). */
void video_cursor(int x, int y, bool visivel);
/* D230: el rectángulo donde el cursor no se dibuja (el contenido de una ventana
 * con JAN_SEM_CURSOR y foco); l o a <= 0 = ninguno. Lo pone el gestor de
 * ventanas; true = cambió (quien llama hace video_mostrar). */
bool video_zona_sem_cursor(int x, int y, int l, int a);
/* D432: la pantalla cedida a un programa que presenta por la GPU (gpu3d): video_mostrar no escribe. */
void video_exclusiva(bool ligar);
bool video_exclusiva_ativa(void);
bool video_cursor_exclusiva(int *x, int *y);   /* la flecha sobre el cuadro recién presentado */
bool video_ler_placa(int x, int y, int l, int a, uint32_t *dest, int passo_px);   /* de la pantalla a la RAM (lento) */
#define VIDEO_CUR_L 12
#define VIDEO_CUR_A 19

#endif
