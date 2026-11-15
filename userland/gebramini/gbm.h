// SPDX-License-Identifier: GPL-3.0-or-later
// GebraMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; esta versión nativa
// para SaguiOS está escrita desde cero a partir de sus ideas. Independiente de GeoGebra.
/* gbm.h - el motor de GebraMini (D309): expresiones, objetos geométricos, grafo de dependencias,
 * órdenes en español y ficheros .gbm. No dibuja nada ni toca el sistema: compila igual en SaguiOS
 * y en el anfitrión (las pruebas de unidad, tests/gebramini_unidad.c). */
#ifndef GBM_H
#define GBM_H
#include "sagui.h"

/* ------------------------------------------------------------ valores y expresiones */

typedef struct { double a, b; int vec; } gbm_val_t;      /* vec = 0: número (a); vec = 1: punto o vector (a, b) */

#define GBM_NODOS 14000
#define GBM_NOME  24
#define GBM_PAI_MAX 24
#define GBM_OBJ_MAX 400

/* Nodos del árbol de una expresión. */
enum { N_NUM, N_X, N_REF, N_NEG, N_ADD, N_SUB, N_MUL, N_DIV, N_POW, N_F1, N_F2, N_LLAMA, N_PAR, N_DNUM };
/* Funciones de un argumento (N_F1) y de dos (N_F2). */
enum { F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN, F_SINH, F_COSH, F_TANH, F_EXP, F_LN, F_LOG10, F_LOG2,
       F_SQRT, F_CBRT, F_ABS, F_FLOOR, F_CEIL, F_ROUND, F_SIGN, F_ABSCISA, F_ORDENADA, F_N1 };
enum { F2_ATAN2, F2_MIN, F2_MAX, F2_MOD, F2_HYPOT, F2_POW, F2_LOG, F_N2 };

typedef struct { uint8_t tipo, fn; int16_t a, b; double num; } gbm_nodo_t;

void   gbm_nodos_reiniciar(void);
int    gbm_nodos_usados(void);
/* Analiza una expresión (todo el texto). Devuelve la raíz o -1 con el error en gbm_erro. "con_x": permite la
 * variable x (la de las funciones). Los nombres que no son de funciones ni constantes se buscan entre los objetos. */
int    gbm_analizar(const char *texto, bool con_x);
extern char gbm_erro[96];
gbm_val_t gbm_avaliar(int raiz, double x);                 /* NaN si algo no está definido */
int    gbm_derivada(int raiz);                              /* nueva raíz con la derivada respecto a x */
int    gbm_copiar_arbol(int raiz);
int    gbm_refs(int raiz, int *saida, int max);            /* objetos a los que hace referencia (sin repetir) */
bool   gbm_usa_x(int raiz);
bool   gbm_constante(int raiz);                             /* sin referencias ni x: un valor suelto */
int    gbm_escrever(int raiz, char *s, int max, bool bonito);   /* la expresión como texto; bonito: ² ³ y espacios */
int    gbm_formatar(double v, int dec, char *s, int max);  /* 3.14, -2, 1e-07, sin ceros de más */
double gbm_texto_num(const char *s, const char **fim);     /* lee un número; fim = lo que queda */
/* Numérico */
typedef double (*gbm_fn_t)(void *ctx, double x);
int    gbm_zeros_f(gbm_fn_t fn, void *ctx, double a, double b, double *r, int max);   /* ceros de fn en [a, b], ordenados */
int    gbm_zeros(int raiz, double a, double b, double *r, int max);          /* raíces de f en [a, b] */
bool   gbm_ref_valor(int idx, gbm_val_t *v);               /* (motor) valor de un objeto dentro de una expresión */
double gbm_derivada_num(int raiz, double x);

/* ------------------------------------------------------------ objetos */

enum {
    G_NADA = 0,
    G_PTO_LIVRE, G_PTO_EXPR, G_PTO_MEDIO, G_PTO_INTER, G_PTO_EM, G_PTO_RAIZ, G_PTO_EXTR,
    G_RETA, G_SEMIRRETA, G_SEGMENTO, G_VETOR, G_PERP, G_PARAL, G_MEDIATRIZ, G_BISSETRIZ,
    G_CIRC_PP, G_CIRC_R, G_CIRC_3,
    G_POLIGONO, G_POLREG,
    G_ANGULO, G_DIST, G_AREA, G_NUM_EXPR, G_SLIDER,
    G_TEXTO, G_FUNCAO,
    G_TIPOS
};

typedef struct {
    uint8_t  tipo;
    uint8_t  ok;                     /* el valor está definido */
    uint8_t  visivel, rotulo, estilo, grosor;
    uint8_t  estado;                 /* recálculo: 0 pendiente, 1 en curso, 2 hecho */
    uint32_t cor;
    char     nome[GBM_NOME];
    int      npai;
    int16_t  pai[GBM_PAI_MAX];
    int      raiz;                   /* expresión (punto, número, función o radio), o -1 */
    int      k;                      /* índice de la intersección o de la raíz; lados del polígono regular */
    double   param;                  /* posición de un punto sobre un objeto */
    double   v[6];                   /* valor calculado (ver gbm_motor.c) */
    double   min, max, passo;        /* deslizador */
    double   px, py;                 /* posición del texto o del deslizador en la vista (píxeles) */
    char     texto[96];
} gbm_obj_t;

extern gbm_obj_t gbm_obj[GBM_OBJ_MAX];
extern int       gbm_n;
extern int       gbm_decimais;       /* decimales de los valores mostrados (2 por defecto) */

void   gbm_limpar(void);
int    gbm_buscar(const char *nome);                          /* índice o -1 */
const char *gbm_nome(int i);
bool   gbm_renomear(int i, const char *novo);                  /* false: nombre inválido o repetido */
void   gbm_recalcular(void);
/* Ejecuta una orden de la barra de entrada. 0 = bien (se creó o cambió algo; el último objeto tocado en
 * *ultimo si no es NULL); si falla, -1 con el motivo en gbm_erro. */
int    gbm_orden(const char *texto, int *ultimo);
bool   gbm_apagar(int i);                                      /* borra el objeto y todo lo que depende de él */
bool   gbm_depende(int i, int de);                             /* i depende (directa o indirectamente) de "de" */
int    gbm_dependentes(int i, int *saida, int max);
int    gbm_descrever(int i, char *s, int max);                 /* "A = (1, 2)", "c: (x - 1)² + (y - 2)² = 9"... */
int    gbm_serializar(char *s, int max, const double *vista);  /* fichero .gbm; vista = {cx, cy, escala} o NULL */
int    gbm_carregar(const char *texto, double *vista);         /* 0 = bien; si no, -1 y gbm_erro (con la línea) */
bool   gbm_livre(int i);                                       /* ¿se puede arrastrar? */
void   gbm_arrastar(int i, double wx, double wy);              /* lleva el objeto al punto del plano (con restricciones) */
/* Cálculo de puntos de corte: los puntos de dos objetos (hasta max). */
int    gbm_interseccoes(int a, int b, double *xs, double *ys, int max);
bool   gbm_eh_ponto(int t);
bool   gbm_eh_reta(int t);        /* recta, semirrecta, segmento, vector y derivadas: v = P0, P1 */
bool   gbm_eh_circ(int t);
bool   gbm_eh_num(int t);
bool   gbm_eh_poligono(int t);
/* Vértices de un polígono (vale para el regular). */
int    gbm_vertices(int i, double *xs, double *ys, int max);
double gbm_valor_funcao(int i, double x);                      /* y = f(x) de un objeto función; NaN si no existe */
bool   gbm_ponto_xy(int i, double *x, double *y);
int    gbm_comandos(const char **lista, int max);              /* nombres de las órdenes (para completar) */
int    gbm_cor_padrao(int tipo, int ordem);
#endif
