// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Inspirado en los CAD de órdenes clásicos,
// pero independiente: nada de código, iconos, marcas ni formatos propietarios de Autodesk.
/* pm.h - el motor de PlanoMini (D312): dibujo 2D con entidades, capas, referencia a objetos, órdenes
 * en el estilo de los CAD de línea de órdenes, modificaciones geométricas (recortar, alargar, equidistancia,
 * empalme...), fichero .pmini (v1 del prototipo y v2) y DXF ASCII. No dibuja ni toca el sistema:
 * compila igual en SaguiOS y en el anfitrión (las pruebas de unidad, tests/planomini_unidad.c). */
#ifndef PM_H
#define PM_H
#include "sagui.h"
#include "gbm_mat.h"

#define PM_PI   GM_PI
#define PM_ENT_MAX   3000
#define PM_CAPAS_MAX 32
#define PM_VERT_MAX  200
#define PM_EPS  1e-9

/* ------------------------------------------------------------ entidades */
enum { E_NADA = 0, E_PUNTO, E_LINEA, E_CIRCULO, E_ARCO, E_POLI, E_TEXTO, E_COTA, E_SOMBRA };
enum { C_LIN_H = 0, C_LIN_V, C_ALINEADA, C_RADIO, C_DIAM, C_ANGULAR };
enum { LT_CONT = 0, LT_TRAZOS, LT_PUNTOS, LT_TRAZOPUNTO, LT_N, LT_CAPA = 255 };
#define COL_CAPA (-1)

typedef struct { double x, y, b; } pm_vert_t;       /* b = bulge (tan de un cuarto del ángulo del arco) */

/* Geometría por tipo:
 *  PUNTO: p0,p1 = punto.   LINEA: (p0,p1)-(p2,p3).   CIRCULO: centro (p0,p1), r.   ARCO: centro, r, a0..a1 (antihorario, rad).
 *  POLI: v[0..nv-1], cerrada.   TEXTO: inserción (p0,p1), alt, giro (rad), txt.
 *  COTA: sub = C_*; lineales/alineada: def1 (p0,p1), def2 (p2,p3), ubicación de la línea (p4,p5);
 *        radio/diámetro: centro (p0,p1), r, a0 = ángulo de la línea de cota; angular: vértice (p0,p1), punto A (p2,p3),
 *        punto B (p4,p5), ubicación del arco (p6,p7). alt = altura del texto; txt = texto fijo ("" = medida).
 *  SOMBRA: contorno v[] (cerrado), sub = 0 líneas / 1 sólido, a0 = ángulo, r = espaciado. */
typedef struct {
    uint8_t tipo, capa, ltipo, cerrada, sub, sel;
    int32_t color;                  /* COL_CAPA o 0xRRGGBB */
    double p[8], r, a0, a1, alt, giro;
    char txt[80];
    pm_vert_t *v;
    int nv;
} pm_ent_t;

typedef struct { char nome[32]; uint32_t color; uint8_t ltipo, visible, bloq; } pm_capa_t;

extern pm_ent_t pm_ent[PM_ENT_MAX];
extern int pm_n;
extern pm_capa_t pm_capa[PM_CAPAS_MAX];
extern int pm_ncapa, pm_capa_act;
extern int32_t pm_color_act;        /* COL_CAPA o RGB */
extern int pm_ltipo_act;            /* LT_CAPA o LT_* */
extern double pm_cota_alt, pm_cota_flecha;
extern int pm_cota_dec;

/* Ayudas de dibujo */
#define REF_FIN 1
#define REF_MEDIO 2
#define REF_CENTRO 4
#define REF_INTER 8
#define REF_PERP 16
#define REF_TANG 32
#define REF_CUAD 64
#define REF_CERCA 128
extern int pm_rejilla, pm_orto, pm_forzc, pm_polar, pm_refent, pm_refmask;
extern double pm_paso_rej, pm_polar_ang;

/* Vista: centro en el plano, píxeles por unidad y tamaño de la zona de dibujo */
extern double pm_vcx, pm_vcy, pm_ve;
extern int pm_vw, pm_vh;
static inline double pm_w2sx(double x) { return pm_vw / 2.0 + (x - pm_vcx) * pm_ve; }
static inline double pm_w2sy(double y) { return pm_vh / 2.0 - (y - pm_vcy) * pm_ve; }
static inline double pm_s2wx(double x) { return pm_vcx + (x - pm_vw / 2.0) / pm_ve; }
static inline double pm_s2wy(double y) { return pm_vcy - (y - pm_vh / 2.0) / pm_ve; }
void pm_zoom_ext(void);
void pm_zoom_vent(double x0, double y0, double x1, double y1);
void pm_zoom_prev(void);
void pm_zoom_factor(double f, double sx, double sy);   /* (sx, sy) en píxeles: el punto que no se mueve */
void pm_vista_guardar(void);                           /* para ZOOM Previo */

/* ------------------------------------------------------------ utilidades de geometría */
int    pm_fmt(double v, int dec, char *s, int max);   /* 3.14, -2, sin ceros de más */
double pm_atof(const char *s, const char **fim);      /* lee un número (con signo, punto decimal y exponente) */
double pm_ang(double dx, double dy);                  /* atan2 en [0, 2pi) */
double pm_angnorm(double a);                          /* a [0, 2pi) */
bool   pm_ang_en(double a, double a0, double a1);     /* a dentro del arco antihorario a0..a1 */
double pm_dist(double x0, double y0, double x1, double y1);
/* Piezas: un segmento o un arco (círculo entero = arco de 0 a 2pi) */
typedef struct { uint8_t arco, inv; double x0, y0, x1, y1, cx, cy, r, a0, a1; } pm_pz_t;
int  pm_piezas(const pm_ent_t *e, pm_pz_t *pz, int max);
int  pm_pz_inter(const pm_pz_t *a, const pm_pz_t *b, bool ext_a, bool ext_b, double (*out)[2], int max);
double pm_pz_dist(const pm_pz_t *p, double x, double y, double *qx, double *qy);   /* distancia al punto más cercano */
double pm_pz_param(const pm_pz_t *p, double x, double y);   /* segmento: t (0..1 en el segmento); arco: ángulo del punto */
void pm_bulge_arco(double x0, double y0, double x1, double y1, double b, pm_pz_t *p);   /* pieza de un tramo con bulge */
double pm_bulge_de(double a0, double a1, bool horario);

/* Dibujo: toda entidad se reduce a segmentos, triángulos, polígonos y textos */
typedef struct {
    void (*seg)(void *c, double x0, double y0, double x1, double y1);
    void (*tri)(void *c, double x0, double y0, double x1, double y1, double x2, double y2);
    void (*poli)(void *c, const double *xs, const double *ys, int n);   /* relleno sólido */
    void (*txt)(void *c, double x, double y, double alt, double giro, const char *t, int centrado);
    void *c;
    double tol;                      /* tolerancia al aplanar arcos (unidades del plano) */
} pm_sink_t;
void   pm_dibujar_ent(const pm_ent_t *e, const pm_sink_t *s);
double pm_txt_ancho(double alt, const char *t);
void   pm_cota_texto(const pm_ent_t *e, char *s, int max);
double pm_cota_valor(const pm_ent_t *e);
int    pm_aplanar(const pm_ent_t *e, double *xs, double *ys, int max);   /* contorno de una entidad cerrada */
bool   pm_ent_bbox(const pm_ent_t *e, double *x0, double *y0, double *x1, double *y1);
double pm_dist_ent(const pm_ent_t *e, double x, double y);
bool   pm_ent_en_ventana(const pm_ent_t *e, double x0, double y0, double x1, double y1, bool cruce);
bool   pm_ent_visible(const pm_ent_t *e);         /* capa visible */
bool   pm_ent_editable(const pm_ent_t *e);        /* visible y no bloqueada */
uint32_t pm_ent_color(const pm_ent_t *e);
int    pm_ent_ltipo(const pm_ent_t *e);
void   pm_ent_clonar(pm_ent_t *dst, const pm_ent_t *src);
void   pm_ent_liberar(pm_ent_t *e);
void   pm_ent_transformar(pm_ent_t *e, const double *m);   /* m = {a, b, tx, c, d, ty} */
void   pm_mat_traslacion(double *m, double dx, double dy);
void   pm_mat_giro(double *m, double cx, double cy, double ang);
void   pm_mat_escala(double *m, double cx, double cy, double f);
void   pm_mat_simetria(double *m, double x0, double y0, double x1, double y1);

/* Documento */
void pm_limpiar(void);
int  pm_anadir_actual(pm_ent_t *e);              /* con las propiedades actuales; libera los vértices de e */
extern int pm_explo_n;
int  pm_anadir(const pm_ent_t *e);                 /* completa capa, color y tipo de línea actuales; devuelve el índice o -1 */
void pm_borrar(int i);
int  pm_capa_buscar(const char *nome);
int  pm_capa_nueva(const char *nome);
void pm_ent_nueva(pm_ent_t *e, int tipo);
int  pm_poli_vert(pm_ent_t *e, double x, double y, double b);
void pm_rect_poli(pm_ent_t *e, double x0, double y0, double x1, double y1);

/* Operaciones de modificación (devuelven 0 = bien, o un código de error con texto en pm_erro) */
extern char pm_erro[120];
int pm_recortar(int idx, double px, double py, const int *bordes, int nb);   /* bordes: índices de entidades (nb = -1: todas las demás) */
int pm_extender(int idx, double px, double py, const int *bordes, int nb);
int pm_equidistancia(int idx, double d, double sx, double sy);               /* crea una entidad nueva */
int pm_empalme(int i1, double p1x, double p1y, int i2, double p2x, double p2y, double r);
int pm_chaflan(int i1, double p1x, double p1y, int i2, double p2x, double p2y, double d1, double d2);
int pm_empalme_poli(int idx, double r);            /* todas las esquinas de la polilínea */
int pm_explotar(int idx);                          /* sustituye la entidad por sus partes */
int pm_sombra_de(int idx, int sub, double ang, double esp);   /* crea el sombreado de un contorno cerrado */
int pm_contorno_en(double x, double y);            /* entidad cerrada que contiene el punto (o -1) */

/* Selección y pinzamientos */
int  pm_sel_contar(void);
void pm_sel_todo(bool v);
int  pm_entidad_en(double x, double y, double ap, int saltar);    /* la más cercana dentro de la apertura (o -1) */
int  pm_sel_ventana(double x0, double y0, double x1, double y1, bool cruce, bool quitar);
int  pm_grips(const pm_ent_t *e, double (*out)[2], int max);
void pm_grip_mover(int idx, int g, double x, double y);
int  pm_props_campos(const pm_ent_t *e, char (*etiq)[20], double *val);   /* devuelve cuántos */
void pm_props_aplicar(pm_ent_t *e, const double *val);

/* Referencia a objetos */
typedef struct { int tipo; double x, y; } pm_ref_t;   /* tipo: REF_* o 0 */
bool pm_ref_buscar(double x, double y, double ap, int mask, bool tiene_ult, double ux, double uy, pm_ref_t *r);

/* ------------------------------------------------------------ órdenes (pm_cmd.c) */
enum { ESP_NADA = 0, ESP_PUNTO, ESP_NUM, ESP_SEL, ESP_TEXTO, ESP_OPC, ESP_OBJ };
extern int  pm_esp;                 /* qué espera la orden activa */
extern char pm_prompt[200];         /* el aviso de la línea de órdenes */
extern char pm_msg[160];            /* último mensaje (resultado o error) */
extern bool pm_msg_err;
extern int  pm_accion;              /* petición al programa: PMA_* (se lee y se pone a 0) */
extern char pm_accion_arg[200];
enum { PMA_NADA = 0, PMA_ABRIR, PMA_GUARDAR, PMA_CAPAS, PMA_PNG, PMA_PDF, PMA_DXF, PMA_AYUDA, PMA_SALIR, PMA_REDIBUJAR, PMA_PROPS, PMA_NUEVO, PMA_IMPDXF, PMA_ESTILOCOTA };
extern bool pm_tiene_ult;           /* hay último punto (para @ y la dirección) */
extern double pm_ult_x, pm_ult_y;
extern double pm_cur_x, pm_cur_y;   /* cursor (para la dirección en ORTO/POLAR) */
extern bool pm_quitar;              /* Mayús en la selección */
extern bool pm_ref_siempre;         /* pm_ref_cursor ajusta aunque no haya una orden esperando un punto (pinzamientos) */
extern int  pm_hist_n;              /* líneas del historial de la línea de órdenes */
const char *pm_hist_linea(int i);
int pm_cargar_ex(const char *texto, bool con_vista);

bool pm_orden(const char *linea);   /* una línea de la barra de órdenes (orden, argumentos u opción); false = no se entendió */
void pm_punto(double x, double y);  /* un clic o un punto ya calculado */
void pm_ventana(double x0, double y0, double x1, double y1);   /* ventana/captura en la selección (x1 < x0 = captura) */
void pm_cancelar(void);             /* Esc */
void pm_enter(void);
bool pm_activa(void);               /* ¿hay una orden en marcha? */
const char *pm_orden_nombre(void);
void pm_iniciar_orden(const char *nombre);
int  pm_fantasma(double cx, double cy, pm_ent_t *out, int max);   /* vista previa elástica de la orden */
bool pm_base_ghost(double *bx, double *by);                        /* punto base de la línea elástica */
void pm_ref_cursor(double wx, double wy, double ap, double *rx, double *ry, pm_ref_t *ref);   /* cursor con rejilla, orto, polar y refent */
int  pm_sugerir(const char *pref, char (*sal)[20], int max);       /* nombres de orden que empiezan por pref */
int  pm_ordenes_lista(int i, const char **nombre, const char **alias);
bool pm_parse_coord(const char *s, double *x, double *y);          /* x,y  @dx,dy  @d<a  d (solo con dirección) */

/* Deshacer / rehacer: instantáneas en texto */
void pm_historial_reiniciar(void);
void pm_historial_guardar(void);
bool pm_deshacer(void);
bool pm_rehacer(void);
extern bool pm_modificado;

/* ------------------------------------------------------------ ficheros */
char *pm_serializar_ex(int *n, bool con_vista);
char *pm_serializar(int *n);                 /* .pmini v2; se libera con libera() */
int   pm_cargar(const char *texto);          /* .pmini v1 o v2; 0 = bien; error en pm_erro */
char *pm_dxf_escribir(int *n);               /* DXF ASCII (R12); se libera con libera() */
int   pm_dxf_leer(const char *texto, int *omitidas);   /* 0 = bien, N entidades omitidas */
uint32_t pm_aci_color(int aci);
int   pm_color_aci(uint32_t rgb);
extern const char *const pm_ltipo_nombre[LT_N];

#endif
