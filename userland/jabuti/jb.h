// SPDX-License-Identifier: GPL-3.0-or-later
/* jb.h - «Jabuti», programación con bloques para niños en SaguiOS (D377, mejora 137).
 *
 * Aplicación original (no es un porte de Scratch: ni su código, ni su gato, ni su nombre).
 * División de los archivos:
 *   jb_modelo.c   la tabla de bloques, el árbol de bloques, el fichero de proyecto (texto + ZIP) y el deshacer
 *   jb_motor.c    el intérprete (hilos verdes, 30 cuadros por segundo) y el estado del escenario
 *   jb_escena.c   el dibujo del escenario y de los trajes (pixel art de 32x32 con paleta de 31 colores)
 *   jb_datos.c    la paleta, la biblioteca de personajes y fondos, la tabla de senos
 *   programas/jabuti.c  la ventana, la paleta de bloques, el arrastrar y soltar, los editores
 *
 * Números: punto fijo con tres decimales (1000 = 1,0), sin FPU, para que funcione en un Pentium II. */
#ifndef JB_H
#define JB_H
#include "sagui.h"

#define U 1000                    /* unidad del punto fijo */
#define ESC_L 480                 /* escenario lógico */
#define ESC_A 360
#define TRAJE_L 32                /* traje: 32x32 píxeles de paleta */
#define FONDO_L 48                /* fondo: 48x36 casillas (cada una de 10x10 píxeles lógicos) */
#define FONDO_A 36
#define TAM_BASE 48               /* un traje al 100 % mide 48 píxeles lógicos */
#define MAX_PERS 16
#define MAX_TRAJES 8
#define MAX_FONDOS 6
#define MAX_GUIONES 24
#define MAX_VARS 24
#define MAX_LISTAS 6
#define MAX_ELEM 64
#define MAX_HILOS 96
#define MAX_PROF 24
#define MAX_MSG 5

/* ---- paleta de 32 índices (0 = transparente) ---- */
extern const uint32_t jb_paleta[32];
extern const char jb_alfabeto[33];        /* ".0123456789abcdefghijklmnopqrstu" */
const char *jb_nombre_color(int i);       /* nombre en español del color de paleta, o NULL */
int jb_color_por_nombre(const char *n);   /* índice de paleta de un color con nombre, o -1 */
int jb_colores_n(void);
const char *jb_color_nombre_n(int k);

/* ---- trajes y fondos ---- */
typedef struct { char nombre[24]; uint8_t px[TRAJE_L * TRAJE_L]; } traje_t;
typedef struct { char nombre[24]; uint8_t px[FONDO_L * FONDO_A]; } fondo_t;

int  jb_biblioteca_n(void);               /* personajes de la biblioteca */
const char *jb_biblioteca_nombre(int i);  /* clave estable en ASCII ("sagui") */
const char *jb_biblioteca_titulo(int i);  /* texto del menú (se traduce con _()) */
void jb_biblioteca_traje(int i, int k, traje_t *t);   /* k-ésimo traje del personaje i de la biblioteca */
int  jb_biblioteca_trajes(int i);
int  jb_fondos_n(void);
const char *jb_fondo_nombre(int i);
const char *jb_fondo_titulo(int i);
void jb_fondo_biblioteca(int i, fondo_t *f);
int  jb_buscar_biblioteca(const char *clave);
int  jb_buscar_fondo(const char *clave);

int  jb_seno(int grados_x1000);           /* seno x 10000 (el ángulo en milésimas de grado) */
int  jb_coseno(int grados_x1000);
int  jb_raiz(int n);                      /* raíz entera */

/* ---- jabuti ---- */
enum { C_EVENTOS, C_MOV, C_APAR, C_SONIDO, C_LAPIZ, C_CONTROL, C_SENS, C_OPER, C_VARS, NCAT };
enum { K_HAT, K_STACK, K_CAP, K_C, K_REP, K_BOOL };
enum { M_NADA, M_TECLA, M_MENSAJE, M_OBJETIVO, M_TOCANDO, M_ROT, M_TRAJE, M_FONDO, M_COLOR, M_SONIDO, M_VAR, M_LISTA, M_DETENER, M_DISTANCIA, M_MAXIMO };

enum {
    O_BANDERA, O_TECLA, O_CLIC, O_RECIBIR, O_ENVIAR,
    O_MOVER, O_GIRAR_D, O_GIRAR_I, O_IR_XY, O_IR_A, O_DESLIZAR, O_APUNTAR, O_APUNTAR_A, O_SUMAR_X, O_FIJAR_X,
    O_SUMAR_Y, O_FIJAR_Y, O_REBOTAR, O_GIRO, O_POS_X, O_POS_Y, O_DIRECCION,
    O_DECIR_T, O_DECIR, O_PENSAR_T, O_PENSAR, O_TRAJE, O_SIG_TRAJE, O_FONDO, O_SUMAR_TAM, O_FIJAR_TAM, O_MOSTRAR,
    O_OCULTAR, O_N_TRAJE, O_TAMANO,
    O_NOTA, O_SONIDO, O_CALLAR,
    O_LAPIZ_BAJAR, O_LAPIZ_SUBIR, O_LAPIZ_BORRAR, O_LAPIZ_COLOR, O_LAPIZ_GROSOR, O_LAPIZ_SELLAR, O_LAPIZ_SIG,
    O_ESPERAR, O_REPETIR, O_SIEMPRE, O_SI, O_SI_NO, O_ESPERAR_HASTA, O_REPETIR_HASTA, O_DETENER,
    O_TOCANDO, O_TOCANDO_COLOR, O_TECLA_PULSADA, O_RATON_PULSADO, O_RATON_X, O_RATON_Y, O_PREGUNTAR, O_RESPUESTA,
    O_CRONO, O_CRONO_REINICIAR, O_DISTANCIA,
    O_SUMA, O_RESTA, O_MULT, O_DIV, O_AZAR, O_MAYOR, O_MENOR, O_IGUAL, O_Y, O_O, O_NO, O_UNIR, O_LETRA, O_LONGITUD,
    O_RESTO, O_REDONDEAR, O_ABS, O_RAIZ,
    O_FIJAR_VAR, O_CAMBIAR_VAR, O_MOSTRAR_VAR, O_OCULTAR_VAR, O_VAR,
    O_LISTA_AGREGAR, O_LISTA_VACIAR, O_LISTA_ELEM, O_LISTA_LONG,
    O_FIN
};

typedef struct {
    const char *clave;       /* nombre estable en el fichero (ASCII) */
    uint8_t cat, tipo, menu;
    const char *rotulo;      /* «mover # pasos»: # número, $ texto, ? booleano, @ menú */
    const char *def[3];      /* valores por defecto de las ranuras */
} op_t;
extern const op_t jb_ops[O_FIN];
extern const uint32_t jb_color_cat[NCAT];
extern const char *const jb_nombre_cat[NCAT];

typedef struct blk blk_t;
typedef struct { blk_t *rep; char t[40]; } arg_t;
struct blk {
    uint8_t op;
    int16_t w, h;            /* tamaño total calculado al maquetar (sin los bloques siguientes) */
    int16_t wr, hr, hb1, hb2;   /* ancho y alto de la fila de rótulo y alto de los cuerpos de los bloques en C */
    blk_t *sig, *c1, *c2;
    arg_t a[3];
};

int    jb_nranuras(int op);                  /* cuántas ranuras (# $ ? @) tiene el rótulo */
char   jb_tipo_ranura(int op, int i);        /* '#', '$', '?' o '@' */
blk_t *jb_blk_nuevo(int op);                 /* con los valores por defecto */
blk_t *jb_blk_copiar(const blk_t *b, bool con_sig);
void   jb_blk_liberar(blk_t *b);             /* libera toda la cadena (sig, cuerpos y reporteros) */
blk_t *jb_ultimo(blk_t *b);
int    jb_op_buscar(const char *clave);
bool   jb_num_parse(const char *s, int32_t *v);        /* true si todo el texto es un número */
void   jb_num_texto(int32_t v, char *s, int max);      /* 12500 -> "12.5" */

/* ---- el proyecto ---- */
typedef struct { blk_t *top; int x, y; } guion_t;
typedef struct {
    char nombre[24];
    int x, y, dir, tam;      /* x, y, dir en milésimas; tam en % */
    bool visible;
    int giro;                /* 0 todo, 1 izquierda-derecha, 2 no girar */
    int ntrajes, traje;
    traje_t *trajes[MAX_TRAJES];
    int nguiones;
    guion_t guiones[MAX_GUIONES];
    char biblio[24];         /* clave de la biblioteca de la que viene (para el fichero), o "" */
    /* estado de ejecución */
    char burbuja[96]; int burb_pensar; unsigned burb_hasta;
    bool lapiz; int lapiz_color, lapiz_grosor;
    int sx0, sy0, sx1, sy1;  /* rectángulo del contenido opaco del traje actual (en píxeles del traje) */
} pers_t;
typedef struct { char nombre[24]; int32_t n; char t[40]; bool es_t; bool mostrar; } var_t;
typedef struct { char nombre[24]; int n; char v[MAX_ELEM][24]; } bqlista_t;

typedef struct {
    int npers;
    pers_t *pers[MAX_PERS];
    int nfondos, fondo;
    fondo_t *fondos[MAX_FONDOS];
    char fondo_biblio[MAX_FONDOS][24];
    int nvars; var_t vars[MAX_VARS];
    int nlistas; bqlista_t listas[MAX_LISTAS];
    char error[160];
} proy_t;
extern proy_t jb_p;

void   jb_proy_vaciar(void);
void   jb_proy_nuevo(void);                  /* proyecto vacío con el sagui */
pers_t *jb_pers_nuevo(const char *nombre);
pers_t *jb_pers_biblioteca(int i);
void   jb_pers_liberar(pers_t *p);
void   jb_pers_borrar(int i);
traje_t *jb_traje_nuevo(const char *nombre);
void   jb_pers_recalcular(pers_t *p);        /* el rectángulo opaco del traje */
void   jb_guion_borrar(pers_t *p, int i);
int    jb_var_buscar(const char *nombre);
int    jb_var_crear(const char *nombre);
int    jb_lista_buscar(const char *nombre);
int    jb_lista_crear(const char *nombre);

/* ---- fichero de proyecto ---- */
char  *jb_escribir_texto(unsigned *n);       /* el proyecto como texto; libera() */
bool   jb_leer_texto(const char *t, unsigned n);   /* false = error en jb_p.error */
bool   jb_guardar(const char *ruta);         /* ZIP con proyecto.txt y leeme.txt */
bool   jb_abrir(const char *ruta);           /* ZIP o texto plano */
void   jb_guion_texto(const blk_t *b, char *s, int max);   /* una línea, para el registro */

/* deshacer: instantáneas del proyecto */
void   jb_hist_reiniciar(void);
void   jb_hist_tomar(void);                  /* antes de un cambio */
void   jb_hist_descartar(void);              /* quita la última instantánea (no hubo cambio) */
bool   jb_hist_deshacer(void);
bool   jb_hist_rehacer(void);
bool   jb_hist_hay_deshacer(void);
bool   jb_hist_hay_rehacer(void);

/* ---- motor ---- */

void jb_motor_iniciar(void);                 /* tras abrir un proyecto */
void jb_motor_bandera(void);
void jb_motor_parar(void);
void jb_motor_tecla(int tecla);              /* tecla pulsada: dispara los «al pulsar la tecla» */
void jb_motor_clic(int x, int y);            /* clic sobre el escenario (coordenadas lógicas centradas) */
void jb_motor_cuadro(unsigned ahora_ms);     /* un cuadro (1/30 s) */
void jb_motor_ejecutar(pers_t *p, blk_t *desde);   /* un clic sobre un bloque: ejecuta esa pila */
bool jb_motor_activo(void);                  /* ¿hay algún hilo? */
int  jb_motor_hilos(void);
extern int jb_raton_x, jb_raton_y;
extern bool jb_raton_pulsado;
extern unsigned char jb_teclas_agarradas[32];   /* mapa de teclas físicas (CV_*) */
extern bool jb_hay_pregunta;
extern char jb_pregunta[96], jb_respuesta[64];
void jb_responder(void);                     /* Intro en la caja de respuesta */
extern bool jb_sucio;                        /* algo cambió en el escenario: redibujar */
extern char jb_aviso[160];                   /* mensaje amable para la persona (vacío = ninguno) */
extern bool jb_log;
extern const char *jb_auto_resp;                /* respuesta automática a «preguntar» (pruebas) */
void jb_tecla_virtual(const char *nombre, unsigned ms);   /* mantiene una tecla «pulsada» (pruebas) */                          /* escribir el registro de las pruebas */
void jb_posicion(const pers_t *p, int *x, int *y);   /* en unidades enteras */
bool jb_tocando_borde(const pers_t *p);
void jb_valor_texto(const char *tecla, char *s, int max);

/* ---- escena ---- */
typedef struct { uint32_t *px; int l, a; } vista_t;
void jb_escena_dibujar(uint32_t *dst, int pitch, int vl, int va);   /* todo el escenario a ese tamaño */
void jb_escena_traje(uint32_t *dst, int pitch, int cl, int ca, const traje_t *t, int px, int py, int lado);   /* miniatura de un traje */
void jb_lapiz_borrar(void);
void jb_lapiz_linea(int x0, int y0, int x1, int y1, int color, int grosor);
void jb_lapiz_punto(int x, int y, int color, int grosor);
int  jb_color_en(int x, int y);              /* índice de paleta en (x, y) lógicos del fondo/lápiz */
int  jb_pers_en(int x, int y);               /* personaje visible bajo ese punto (el de más arriba), o -1 */
bool jb_pers_toca_color(const pers_t *p, int color);
bool jb_pers_toca_pers(const pers_t *a, const pers_t *b);
void jb_pers_caja(const pers_t *p, int *x0, int *y0, int *x1, int *y1);   /* caja lógica centrada */
extern int jb_zoom_escena;

#endif
