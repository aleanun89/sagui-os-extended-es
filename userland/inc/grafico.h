// SPDX-License-Identifier: GPL-3.0-only
/* grafico.h - componente de gráficos de la libsagui (v8.5.6, D469).
 *
 * Líneas, áreas, barras (verticales y horizontales, agrupadas y apiladas), tarta, anillo, dispersión,
 * histograma y velas (OHLC), con ejes de marcas «bonitas» (1-2-5), leyenda, tooltip, animación por tiempo,
 * datos en vivo (ventana deslizante) y exportación a PNG/BMP.
 *
 * Esta cabecera usa SOLO tipos básicos de C (int, unsigned, double, char): la incluyen tanto la libsagui
 * como el módulo de MicroPython (que no puede incluir sagui.h, ver sagui_mp.h) y la prueba del anfitrión.
 * Los colores son 0x00RRGGBB; los píxeles de destino también (el formato de jan_t::px y de sagui.Imagen).
 *
 * Dos capas:
 *   - funciones puras (grafico_calc.c): escalas, marcas, formato de números, apilado, curvas de suavizado,
 *     geometría de barras, colisiones. No dependen de nada (se prueban en el PC: tests/grafico_unidad.c);
 *   - el componente (gui_grafico.c): estado, dibujo sobre un búfer de píxeles, ratón, animación, exportar.
 *
 * Uso típico (en un bucle de eventos):
 *     grafico_t *g = grafico_nuevo();
 *     grafico_tipo(g, GR_BARRAS);
 *     int s = grafico_agregar_serie(g, "Ventas", 0);
 *     double v[] = { 3, 5, 2 };
 *     grafico_datos(g, s, v, 3);
 *     // en cada tic:  if (grafico_avanzar(g, tempo_ms()) || grafico_sucio(g)) grafico_dibujar(g, px, larg, alt);
 *     // con el ratón: grafico_mover(g, x, y);  grafico_pulsar(g, x, y, &ev);
 */
#ifndef SAGUI_GRAFICO_H
#define SAGUI_GRAFICO_H

/* ------------------------------------------------------------ tipos y opciones */

enum { GR_LINEAS, GR_AREA, GR_BARRAS, GR_BARRAS_H, GR_TARTA, GR_ANILLO, GR_DISPERSION, GR_HISTOGRAMA, GR_VELAS, GR_NTIPOS };
enum { GR_LEY_NINGUNA, GR_LEY_ABAJO, GR_LEY_ARRIBA, GR_LEY_DERECHA, GR_LEY_IZQUIERDA };
/* Curvas de suavizado de la animación. */
enum { GR_EASE_LINEAL, GR_EASE_SALIDA, GR_EASE_ENTRADA, GR_EASE_AMBAS };

/* grafico_opcion(g, GR_O_*, valor) */
enum {
    GR_O_LEYENDA,      /* GR_LEY_* (por omisión abajo) */
    GR_O_REJILLA,      /* 0/1 */
    GR_O_ANIMADO,      /* 0/1: interpolar al aparecer y al cambiar los datos */
    GR_O_APILADO,      /* 0/1: barras, áreas */
    GR_O_SUAVE,        /* 0/1: líneas y áreas con curva (Catmull-Rom) */
    GR_O_PUNTOS,       /* 0/1: marcadores en las líneas */
    GR_O_DURACION,     /* ms de la animación (400 por omisión) */
    GR_O_VALORES,      /* 0/1: el valor escrito sobre cada barra / porción */
    GR_O_HUECO,        /* anillo: radio interior en % del exterior (55 por omisión) */
    GR_O_BINS,         /* histograma: número de intervalos (0 = automático) */
    GR_O_VENTANA,      /* datos en vivo: máximo de puntos por serie (0 = sin límite, tope GR_MAX_PUNTOS) */
    GR_O_CERO,         /* 0/1: el eje de valores incluye el cero (1 por omisión; en barras siempre) */
    GR_O_EASE,         /* GR_EASE_* (salida por omisión) */
    GR_O_FONDO         /* 0/1: pinta el fondo de la superficie (1 por omisión; 0 = transparente sobre lo que haya) */
};

#define GR_MAX_SERIES   12
#define GR_MAX_PUNTOS   512
#define GR_MAX_ETQ      24     /* bytes por nombre de categoría o de serie */
#define GR_MAX_LEY      24     /* elementos de la leyenda */

typedef struct grafico grafico_t;

/* Lo que devuelve grafico_pulsar. */
typedef struct {
    int serie;         /* serie (en tarta y anillo, siempre 0) */
    int indice;        /* categoría o punto */
    double valor;      /* valor (en velas, el cierre; en dispersión, y) */
    int leyenda;       /* 1 = el clic fue en la leyenda y alternó la serie `serie` (o la porción `indice`) */
} grafico_ev_t;

/* ------------------------------------------------------------ funciones puras (grafico_calc.c) */

typedef struct { double min, max, paso; int n; } gr_escala_t;   /* marcas: min, min+paso, ... (n marcas) */
typedef struct { int decimales; int miles; int pct; char dec, mil; char prefijo[8], sufijo[8]; } gr_fmt_t;

double gr_seno(double a);
double gr_coseno(double a);
double gr_abs(double x);
double gr_floor(double x);
double gr_ceil(double x);
double gr_raiz(double x);
double gr_p10(int e);                                       /* 10^e */
double gr_ease(int curva, double t);                        /* t en [0,1] -> [0,1] */
double gr_lerp(double a, double b, double t);
/* Escala automática: marcas 1-2-5 que cubren [lo, hi], como mucho `max_marcas`. `cero` = incluir el cero. */
void   gr_escala(double lo, double hi, int max_marcas, int cero, gr_escala_t *e);
/* Formato: «0», «0.0», «#,##0», «0.00», «0%», «0.0%», «$0.00», «0 kg», «auto». */
void   gr_fmt_parse(const char *spec, gr_fmt_t *f);
int    gr_fmt_num(const gr_fmt_t *f, double v, char *buf, int max);   /* bytes escritos (sin el 0) */
/* Apilado: base[s][i] y cima[s][i] de cada serie visible (las ocultas quedan con base = cima); los negativos se
 * apilan por debajo del cero, aparte de los positivos. `ys[s]` puede ser NULL (serie vacía). */
void   gr_apilar(const double *const *ys, const int *ns_pts, int ns, unsigned oculto, int n, double *base, double *cima);   /* base/cima: ns*n */
/* Geometría de una barra: la categoría i de n ocupa [ini + i*slot, +slot); `hueco` (0..1) es la parte vacía del
 * slot; apilado = una sola barra por categoría; si no, `nvis` barras juntas y la barra `k` (0..nvis-1). */
void   gr_barra_geom(double ini, double total, int n, int i, int nvis, int k, int apilado, double hueco, double *x, double *w);
/* Histograma: reparte los datos en nb intervalos [lo + j*paso, lo + (j+1)*paso). nb = 0 -> regla de Sturges. */
int    gr_bins(const double *v, int n, int nb, double *lo, double *paso, int *cuentas, int max_bins);
/* Punto de una curva Catmull-Rom entre p1 y p2 (p0 y p3 los vecinos), t en [0,1]. */
double gr_catmull(double p0, double p1, double p2, double p3, double t);
/* Colisiones. */
int    gr_en_rect(double x, double y, double rx, double ry, double rw, double rh);
int    gr_en_circulo(double x, double y, double cx, double cy, double r);
/* Sector entre los ángulos a0 y a1 (radianes, 0 = a las 12 en punto, crece en sentido horario) y radios r0..r1. */
int    gr_en_sector(double x, double y, double cx, double cy, double r0, double r1, double a0, double a1);
/* Dirección (x, y) de un ángulo medido desde las 12 en punto en sentido horario. */
void   gr_dir(double a, double *dx, double *dy);
/* Color de la serie i en paleta clara (oscuro = 0) u oscura. */
unsigned gr_color_serie(int i, int oscuro);

/* ------------------------------------------------------------ el componente (gui_grafico.c) */

grafico_t *grafico_nuevo(void);
void       grafico_liberar(grafico_t *g);
void       grafico_tipo(grafico_t *g, int tipo);
int        grafico_tipo_actual(const grafico_t *g);
void       grafico_titulo(grafico_t *g, const char *t);
void       grafico_etiquetas_ejes(grafico_t *g, const char *x, const char *y);
void       grafico_opcion(grafico_t *g, int opcion, double valor);
double     grafico_opcion_leer(const grafico_t *g, int opcion);
void       grafico_formato(grafico_t *g, const char *spec);
void       grafico_separadores(grafico_t *g, char decimal, char miles);
void       grafico_rango(grafico_t *g, int automatico, double min, double max);   /* eje de valores */
/* Categorías (etiquetas del eje de categorías y de las porciones). */
void       grafico_categorias_limpiar(grafico_t *g);
int        grafico_categoria_agregar(grafico_t *g, const char *nombre);
int        grafico_n_categorias(const grafico_t *g);
/* Series. color 0 = el de la paleta. Devuelven el índice o -1. */
int        grafico_agregar_serie(grafico_t *g, const char *nombre, unsigned color);
void       grafico_quitar_series(grafico_t *g);
int        grafico_n_series(const grafico_t *g);
int        grafico_serie_color(const grafico_t *g, int s, unsigned color);   /* 0 = ok */
int        grafico_serie_nombre(grafico_t *g, int s, const char *nombre);
int        grafico_datos(grafico_t *g, int s, const double *y, int n);              /* sustituye (anima) */
int        grafico_datos_xy(grafico_t *g, int s, const double *x, const double *y, const double *tam, int n);   /* dispersión */
int        grafico_agregar_punto(grafico_t *g, int s, double y);                    /* al final; con ventana, desplaza */
int        grafico_agregar_punto_xy(grafico_t *g, int s, double x, double y, double tam);
int        grafico_n_puntos(const grafico_t *g, int s);
double     grafico_valor(const grafico_t *g, int s, int i);                         /* el valor de destino */
void       grafico_limpiar(grafico_t *g);                                           /* sin series ni categorías */
void       grafico_ocultar(grafico_t *g, int item, int oculto);                     /* serie (o porción en tarta) */
int        grafico_oculto(const grafico_t *g, int item);
/* Tiempo y dibujo. */
int        grafico_avanzar(grafico_t *g, unsigned ahora_ms);    /* 1 = sigue animándose (hay que volver a dibujar) */
int        grafico_sucio(const grafico_t *g);                   /* cambió algo desde el último dibujo */
void       grafico_reiniciar(grafico_t *g);                     /* vuelve a hacer la animación de entrada */
void       grafico_dibujar(grafico_t *g, unsigned *px, int larg, int alt);
/* Ratón (coordenadas relativas al gráfico). grafico_mover devuelve 1 si cambió lo resaltado (hay que redibujar). */
int        grafico_mover(grafico_t *g, int x, int y);
void       grafico_salir(grafico_t *g);                          /* el ratón se fue */
int        grafico_pulsar(grafico_t *g, int x, int y, grafico_ev_t *ev);   /* 1 = clic en un elemento, 2 = alternó la leyenda, 0 = nada */
int        grafico_resaltado(const grafico_t *g, int *serie, int *indice);  /* 1 = hay uno */
const char *grafico_tooltip(const grafico_t *g);                  /* texto del tooltip vigente, o "" */
/* Exporta a .png o .bmp (por la extensión) con el estado final de la animación; 0 = bien. */
int        grafico_exportar(grafico_t *g, const char *ruta, int larg, int alt);

#endif
