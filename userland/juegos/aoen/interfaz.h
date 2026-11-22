/* SPDX-License-Identifier: GPL-3.0-or-later */
/* AoEN, copyright de su autor (el mantenedor de SaguiOS-Extended); GPL v3 o posterior. Port a SaguiOS: D382. */
/* Interfaz de AoEN (v8.5.4.7): estado compartido entre el bucle (main.c), los controles (ui_comun.c), los menús
 * (ui_menu.c), la partida (ui_hud.c) y los paneles (ui_paneles.c).
 *
 * Coordenadas: el ratón va en píxeles reales; todo lo que dibuja la interfaz va en coordenadas lógicas
 * (píxeles reales / app.us; app.us es 2 a partir de 1600x960). Las funciones de este fichero reciben coordenadas
 * lógicas salvo que digan otra cosa. */
#ifndef AOEN_INTERFAZ_H
#define AOEN_INTERFAZ_H
#include "ui.h"

/* ---------- aplicación ---------- */
typedef enum { SC_MENU, SC_SETUP, SC_OPTIONS, SC_GAME, SC_SAVES } Screen;

typedef struct {
	Screen screen, options_back, saves_back;
	int saves_mode;         /* 0 cargar, 1 guardar */
	int running, fullscreen, show_fps, scroll_speed, game_speed;
	int paused;             /* menú de pausa abierto */
	int us;                 /* escala de la interfaz de este cuadro (1 o 2) */
	int fps;
	float last_autosave;
	Uint32 t_pantalla;      /* SDL_GetTicks() al entrar en la pantalla actual (animaciones y fundido de entrada) */
	int cursor_propio;      /* 1: el juego dibuja su cursor según lo que hay debajo (y oculta el del sistema) */
	int nombres_raton;      /* 1: nombre y datos de lo que hay bajo el ratón en el mapa */
	int escala_dibujo;      /* % de la resolución del escritorio en pantalla completa (100, 75, 50): el sistema amplía */
	char data_dir[1100];
} App;
extern App app;

/* ---------- entrada del cuadro actual ---------- */
typedef struct {
	int mx, my;             /* ratón en píxeles reales */
	int mdown, mclick, rclick, wheel, mouse_seen, mpressed, mdx, mdy;
	int key[512];           /* teclas pulsadas en este cuadro (scancode) */
	const Uint8 *keys;      /* estado del teclado */
	int bloqueo;            /* 1: el ratón ya lo ha usado la interfaz en este cuadro (no llega al mapa) */
	int foco;               /* el ratón está en la ventana (desplazamiento por los bordes) */
} Entrada;
extern Entrada in;

int tecla(int scancode);           /* pulsada en este cuadro (sin repetición) */
int mayus(void);                   /* Mayús pulsada */
int ctrl(void);
int mod_down(void);                /* Mayús o Ctrl */

/* ---------- tema ---------- */
#define COL_PANEL  gfx_rgb(46, 29, 19)
#define COL_PANEL2 gfx_rgb(30, 19, 13)
#define COL_GOLD   gfx_rgb(205, 165, 70)
#define COL_CREAM  gfx_rgb(245, 230, 190)
#define COL_RED    gfx_rgb(150, 28, 24)
#define COL_REDH   gfx_rgb(190, 45, 38)
#define COL_DIM    gfx_rgb(170, 155, 125)
#define COL_OK     gfx_rgb(130, 235, 120)
#define COL_MAL    gfx_rgb(255, 120, 100)
#define COL_AVISO  gfx_rgb(255, 225, 120)

/* ---------- controles (ui_comun.c) ---------- */
typedef struct { int x, y, w, h; } Rect;
int hit(int x, int y, int w, int h);                    /* el ratón está dentro (y no lo tapa un panel modal) */
int hit_r(Rect r);
void bevel(int x, int y, int w, int h, u32 border);
void panel(int x, int y, int w, int h);                 /* panel de diálogo (textura de la civilización si hay datos) */
void panel_hueco(int x, int y, int w, int h);           /* hueco oscuro sobre un panel, para texto */
int button(int x, int y, int w, int h, const char *label, int enabled, int size);
int boton_t(int x, int y, int w, int h, const char *label, int enabled);   /* con la fuente de 8x16 */
int pestana(int x, int y, int w, int h, const char *label, int activa);  /* pestaña de una pantalla con páginas */
int cycler(int x, int y, int w, const char *label);    /* selector "< valor >": -1, 0 o +1 */
int checkbox(int x, int y, const char *label, int *v);
void texto_ajustado(int x, int y, int w, const char *s, u32 col, int max_lineas);   /* parte en líneas; 8x16 */
int texto_lineas(const char *s, int w);
void barra_progreso(int x, int y, int w, int h, int v, int max, u32 col);

/* teclado en los menús: los controles que lo admiten se registran en orden; Arriba/Abajo mueven el foco,
 * Izquierda/Derecha cambian el valor y Intro pulsa */
int foco_control(int x, int y, int w, int h);           /* registra un control; devuelve 1 si tiene el foco */
void foco_inicio(void);                                 /* al empezar cada pantalla con teclado */
extern int foco_actual, foco_n;

/* ---------- iconos (ui_comun.c) ----------
 * Con los datos originales se usan los dibujos de INTERFAC.DRS (recursos, órdenes, mejoras); sin ellos, dibujos propios. */
enum {
	IC_MADERA, IC_COMIDA, IC_ORO, IC_PIEDRA, IC_POB, IC_ATAQUE, IC_ARMADURA, IC_PERFORANTE, IC_ALCANCE, IC_VELOCIDAD, IC_VIDA, IC_TIEMPO,
	IC_AGRESIVA, IC_DEFENSIVA, IC_QUIETA, IC_PASIVA, IC_PROTEGER, IC_PATRULLA, IC_DETENER, IC_DESEMBARCAR, IC_EMBARCAR,
	IC_REUNION, IC_SIGUIENTE, IC_CANCELAR, IC_CONSTRUIR, IC_ELIMINAR, IC_DIPLOMACIA, IC_AYUDA, IC_PUNTOS, IC_ALERTA, IC_EDAD,
	IC_MEJORA, IC_VENDER, IC_COMPRAR, IC_TRIBUTO, IC_ALDEANO, IC_SOLDADO, IC_RECURSOS, IC_MILITAR, IC_COUNT
};
void icono(int id, int x, int y, int sz);              /* dentro de un cuadrado de sz x sz */
void icono_rec(int kind, int x, int y, int h);          /* icono de recurso de alto h (para costes en línea) */
int coste_linea(int x, int y, const int *coste, int faltan_marca);   /* "icono 50 icono 20": devuelve el ancho */
int coste_ancho(const int *coste);
Sprite *icono_mejora(int tech);
Sprite *icono_unidad(EntType t);
Sprite *icono_edificio(EntType t);

/* ---------- ayudas emergentes ---------- */
typedef struct {
	char titulo[80];
	int coste[RES_COUNT];
	int con_coste;
	float tiempo;
	char tecla[24];
	char desc[220];
	char motivo[120];        /* en rojo: por qué no se puede */
	char extra[160];         /* en gris: requisitos u otros datos */
	Rect ancla;
} Tip;
void tip_poner(const Tip *t);                           /* la ayuda de este cuadro (la última gana) */
void tip_dibujar(void);
void tip_cancelar(void);                                 /* al final del cuadro, encima de todo */
void tip_simple(Rect ancla, const char *titulo, const char *desc);
const Tip *tip_actual(void);

/* ---------- comprobación de solapes (pruebas) ----------
 * Cada bloque de la interfaz se registra; con AOEN_SOLAPES=1 se cuentan los pares que se pisan y se escriben en el registro. */
void solape_registrar(int x, int y, int w, int h, const char *nombre);
void solape_fin_cuadro(void);

/* ---------- aspecto con los gráficos originales (paneles de piedra, madera, mármol...) ---------- */
int estilo_jugador(void);                               /* 0 egipcio, 1 griego, 2 asiático, 3 babilonio */
void fondo_barra(int x, int y, int w, int h, int superior);   /* barra superior o inferior de la partida */
void fondo_dialogo(int x, int y, int w, int h);

/* ---------- lo elegido en la configuración de la partida (ui_menu.c) ---------- */
typedef struct {
	int players, map, size, tiempo, victoria, recursos, popcap, seed, seed_focus, revelado, espectador;
	int compartida;                                               /* victoria compartida por equipos y aliados */
	int recursos_mapa;                                            /* 0 escasos, 1 normales, 2 abundantes (orden del menú) */
	int comercio;                                                 /* comercio mejorado (barcos y carros con modo intercambio) */
	int fuego_amigo;                                              /* fuego amigo del daño en área (D447) */
	int diff[MAX_PLAYERS], civ[MAX_PLAYERS], team[MAX_PLAYERS];   /* team 0: sin equipo */
	int color[MAX_PLAYERS];                                       /* 1..8 */
	int res[RES_COUNT];                                           /* recursos personalizados: índice en la tabla */
} ConfPartida;
extern ConfPartida su;
void new_seed(void);
void start_game(void);
void setup_desde_menu(Setup *s);

/* ---------- pantallas ---------- */
void draw_menu(void);
void draw_setup(void);
void draw_options(void);
void opciones_pagina(int p);                             /* 0 gráficos, 1 interfaz, 2 sonido, 3 teclas */
void draw_saves(void);
void setup_tecla_texto(const char *t);                  /* texto escrito (semilla) */
int setup_tecla(int sym);                               /* teclas especiales en la configuración; 1 si la usó */

/* partida (ui_hud.c) */
extern int top_h, bottom_h;
void hud_disponer(void);                                /* calcula las medidas de la interfaz para este cuadro */
void draw_hud(void);
void game_input(float dt);
void hud_mundo(void);                                   /* marcas sobre el mundo: órdenes, rutas, banderas (antes de la interfaz) */
void hud_reiniciar(void);                               /* partida nueva o cargada */
extern EntType placing;
extern int selecting, sel_x0, sel_y0;
int first_selected(void);
void select_none(void);
void avisos_actualizar(float dt);
void aviso(int icono, const char *texto, u32 col, float x, float y);   /* x < 0: sin posición */
int saltar_aviso(void);
int hud_orden_pos(int k, int *x, int *y);               /* centro del botón k de la rejilla, en píxeles reales (pruebas) */
void hud_mm_pos(float wx, float wy, int *x, int *y);    /* un punto del mundo en el minimapa, en píxeles reales */
extern Rect prueba_tributo, prueba_relacion;            /* botones de la diplomacia (pruebas) */
extern Rect prueba_fuego;                                  /* casilla «Fuego amigo» de la configuración (pruebas) */
extern Rect prueba_comercio;                            /* casilla «Comercio mejorado» de la configuración (pruebas) */
void prueba_paso(int frame);                            /* AOEN_PRUEBA: guiones de las pruebas de la interfaz */
extern int prueba_activa;                               /* hay guion AOEN_PRUEBA (el ratón simulado está dentro) */
extern int final_pest;

/* cursor propio y lo que hay bajo el ratón (ui_cursor.c, ui_hud.c; v8.5.5.3) */
enum { CUR_FLECHA, CUR_ATACAR, CUR_MADERA, CUR_COMIDA, CUR_ORO, CUR_PIEDRA, CUR_CONSTRUIR, CUR_OBJETIVO, CUR_PROHIBIDO, CUR_DESPLAZAR, CUR_COUNT };
void cursor_dibujar(int tipo, int x, int y, int s, int dx, int dy);   /* en píxeles reales; s: escala */
void aro_elipse(int cx, int cy, int rx, int ry, u32 col, int grueso);
int hud_cursor(int *dx, int *dy);                      /* el cursor que toca en la partida (dx, dy: desplazamiento) */
void hud_rotulo_raton(void);                            /* rótulo de lo que hay bajo el ratón (coordenadas lógicas) */
void dibujar_teclas(int x, int y, int w, int h);        /* la tabla de teclas (ayuda F1 y Opciones > Teclas) */

/* paneles (ui_paneles.c) */
enum { PANEL_NINGUNO, PANEL_DIPLOMACIA, PANEL_AYUDA, PANEL_PUNTOS };
extern int panel_abierto;
extern int marcador_modo;                               /* 0 compacto, 1 ampliado, 2 oculto (Tab) */
void draw_marcador(int x, int y, int ancho_max);
void draw_panel_abierto(void);
void paneles_cerrar(void);
void draw_endscreen(void);
void draw_pause(void);
int panel_modal(void);                                  /* hay un panel que tapa el mapa */
void grafica_puntos(int x, int y, int w, int h);

/* main.c */
void start_game_setup(const Setup *s);
void enter_loaded_game(void);
int do_save(int i);
int do_load(int i);
int any_save(void);
void refresh_slots(void);
void apply_fullscreen(void);
float default_zoom(void);
void slot_path(int i, char *out, size_t n);
#define NUM_SLOTS 10
typedef struct { int exists, incompatible; SaveInfo si; } Slot;
extern Slot slots[NUM_SLOTS];
extern char saves_msg[1400];
#endif
