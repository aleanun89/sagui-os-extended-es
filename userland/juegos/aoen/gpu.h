/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Dibujo de la partida por la GPU (D436, «fase 2»): el terreno, las entidades y lo que va encima del suelo se mandan como
 * triángulos con textura al renderizador de la SDL (en SaguiOS, gpu3d por la capa D288/D432); la interfaz se sigue pintando
 * con la CPU en una capa con alfa premultiplicado que solo se sube por franjas cuando cambia. Opcional: Opciones > Gráficos
 * («Dibujo por GPU») o AOEN_GPU=1; si la tarjeta no tiene 3D o el renderizador deja la GPU, vuelve solo a la CPU. */
#ifndef AOEN_GPU_H
#define AOEN_GPU_H

#include "aoen.h"

extern int gpu_pedido;    /* la opción: 1 si se quiere el dibujo por GPU */
extern int gpu_ativo;     /* este cuadro de la partida se dibuja por la GPU */
extern int gfx_gpu;       /* gfx.c: las primitivas del mundo van a la lista de la GPU */
extern int gfx_capa;      /* gfx.c: se pinta en la capa de la interfaz (alfa premultiplicado) */

/* Arranque: con un renderizador acelerado. Devuelve 1 si se podrá dibujar por la GPU. */
int gpu_iniciar(SDL_Renderer *rnd);
/* Opciones > Gráficos («Dibujo por GPU»): aplicar y el texto que va al lado */
void gpu_opcion(int si);
const char *gpu_opcion_texto(void);
/* Deja el dibujo por GPU (con el motivo en el registro y un aviso en la partida si hace falta) */
void gpu_apagar(const char *motivo);
/* Nombre del camino de dibujo y si la presentación va por la GPU (para el HUD de rendimiento) */
const char *gpu_estado(int *presenta_gpu);

/* Un cuadro: empezar (borra y decide si va por la GPU: 1 sí) y presentar (sube la capa y presenta). Si shot no es NULL, se
 * leen ahí los píxeles del cuadro (w*h, 0xAARRGGBB) antes de presentar. */
int gpu_cuadro_empezar(int w, int h);
void gpu_cuadro_presentar(u32 *shot, int pitch_px);

/* ---- el mundo (gfx.c y render.c) ---- */
void gpu_mundo_empezar(void);
void gpu_mundo_terminar(void);   /* resuelve los sprites en el atlas y manda todo, en orden */
/* primitivas (en píxeles reales; el recorte es el del momento) */
void gpu_op_relleno(int x, int y, int w, int h, u32 col, int alfa, const Recorte *r);
void gpu_op_linea(int x0, int y0, int x1, int y1, u32 col, const Recorte *r);
void gpu_op_oscuro(int x, int y, int w, int h, const Recorte *r);
void gpu_op_elipse(int cx, int cy, int rx, int ry, u32 col, int alfa, const Recorte *r);   /* alfa 256: sombra (mitad de luz) */
void gpu_op_sprite(const Sprite *s, int x, int y, int scale, int player, int shade, int flip, const Recorte *r);

/* ---- terreno ---- */
/* Una textura fija (las HD); NULL si no se pudo */
SDL_Texture *gpu_textura_fija(const u32 *px, int w, int h);
/* Triángulos con textura (t NULL: de color); modo SDL_BlendMode. Se juntan con los anteriores si se puede. */
void gpu_triangulos(SDL_Texture *t, int modo, const SDL_Vertex *v, int n);
/* Una loseta del terreno clásico: el rombo de la casilla (vértices px, py: izquierda, abajo, arriba, derecha) con su
 * sprite colocado en (ox, oy) a la escala scale/256, y la luz de esas cuatro esquinas (0..256) */
void gpu_loseta(const Sprite *s, int ox, int oy, int scale, const float px[4], const float py[4], const int l[4]);
/* Solo las filas [y0, y1) desde ahora (y1 <= y0: todo): el terreno no se pinta bajo las barras de la interfaz */
void gpu_recorte(int y0, int y1);
/* Resuelve antes en el atlas los sprites de las losetas (para que las páginas se suban una vez por cuadro) */
void gpu_loseta_preparar(const Sprite *s);

/* ---- la capa de la interfaz (gfx.c) ---- */
void gpu_capa_empezar(void);       /* fb.px pasa a la capa (transparente donde no se pinte) */
void gpu_capa_terminar(void);      /* fb.px vuelve al búfer de la CPU */
void gpu_capa_marca(int y0, int y1);   /* filas [y0, y1) pintadas en la capa */

/* ---- medidas (HUD de rendimiento, AOEN_PERFIL) ---- */
typedef struct {
	unsigned long tris, lotes, subidas_kb;   /* del último cuadro */
	int paginas, paginas_max, sprites;       /* atlas */
	int franjas_subidas, franjas;            /* capa */
} GpuMedidas;
void gpu_medidas(GpuMedidas *m);
unsigned long long gpu_espera_ciclos(void);   /* ciclos (perf_reloj) mandando triángulos y presentando */
int gpu_ativo_ultimo(void);                   /* el último cuadro de la partida fue por la GPU */
/* HUD de rendimiento (rendimiento.c): Ctrl+F u Opciones > Gráficos */
extern int hud_rend_visible;
void hud_rend_cuadro(void);
void hud_rend_alternar(void);
void hud_rend_dibujar(void);
void gpu_informe_empezar(void);   /* AOEN_BENCH: pone a cero las sumas */
void gpu_informe(void);           /* y las escribe en el registro (por cuadro) */

#endif
