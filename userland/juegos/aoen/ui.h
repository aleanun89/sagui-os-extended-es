/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
#ifndef AOEN_UI_H
#define AOEN_UI_H
#include "game.h"

typedef struct { float cx, cy, zoom; int sw, sh; } View;
extern View V;

typedef struct { Slp *stand, *move, *attack, *die, *decay; } AnimSet;

int assets_load(const char *dir);
void assets_free(void);
Sprite *icon_unit(int idx);
Sprite *icon_bld(int idx);

void view_proj(float wx, float wy, float *px, float *py);
void view_to_screen(float wx, float wy, int *sx, int *sy);
void screen_to_world(int sx, int sy, float *wx, float *wy);
void view_center_on(float wx, float wy);
void view_clamp(void);

void units_sheet(void);
void match_report(const int *stand, int n);
int sheet_list(int mode, int *ids, int max);
void sheet_draw(const int *ids, int n);
int tex_load(const char *dir);
int tex_available(void);
u32 tex_tile_color(int kind);
void render_terrain_hd(float time, int y0, int y1);   /* solo las filas [y0, y1) */
extern int hd_enabled;
void fondo_propio(void);   /* paisaje dibujado por código, cuando faltan los fondos originales (fondo.c) */
void draw_background(int which);   /* 0 menú principal, 1 pantalla de un jugador */
void render_world(int top_h, int bottom_h);
void render_placement(EntType t, int tx, int ty);
void render_minimap(int x, int y, int w, int h);
int minimap_pick(int x, int y, int w, int h, int mx, int my, float *wx, float *wy);

/* gráficos de la interfaz en INTERFAC.DRS (NULL si faltan) */
Slp *iface_slp(u32 id);
Sprite *iface_frame(u32 id, int frame, int min_w);
/* minimapa: filtros (recursos, solo militar) */
enum { MM_RECURSOS = 1, MM_MILITAR = 2 };
extern int mm_filtro;
/* marca breve en el mapa al dar una orden: 0 mover, 1 atacar, 2 patrullar o reunión */
void marca_orden(float wx, float wy, int tipo);

/* colores (índice: pcolor(jugador)) */
extern u32 col_player[MAX_PLAYERS];
#endif
