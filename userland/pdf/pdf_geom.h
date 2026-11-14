// SPDX-License-Identifier: GPL-2.0-or-later
/* pdf_geom.h - geometría del motor de PDF (mejora 059, D138).
 *
 * Los trazos son la moneda común entre el intérprete de contenido y las
 * tipografías: un path del PDF y el contorno de un glifo se describen igual
 * (una lista de segmentos), así que el rasterizador solo conoce este tipo.
 *
 * Todo en coma flotante (doble). Este módulo se compila aparte con la x87
 * (-mfpmath=387), porque la libsagui común va sin coma flotante (D076). */
#ifndef SAGUI_PDF_GEOM_H
#define SAGUI_PDF_GEOM_H
#include "sagui.h"

typedef struct {
    double x, y;
} pdf_ponto_t;

typedef enum {
    PDF_TRA_LINHA,      /* de (x0,y0) a (x1,y1) */
    PDF_TRA_CUBICA      /* de (x0,y0) a (x1,y1) con dos puntos de control */
} pdf_tipo_t;

typedef struct {
    pdf_tipo_t tipo;
    double x0, y0;      /* inicio */
    double x1, y1;      /* fin */
    double cx1, cy1;    /* control 1 (cúbica) */
    double cx2, cy2;    /* control 2 (cúbica) */
} pdf_segmento_t;

/* Un camino: los segmentos van encadenados (el fin de uno es el inicio del
 * siguiente) y puede tener varios tramos separados (un "m" nuevo los parte). */
typedef struct {
    pdf_segmento_t *segs;
    int    n, cap;
    bool   fechado;     /* el último tramo vuelve al punto de partida */
} pdf_caminho_t;

void pdf_caminho_limpar(pdf_caminho_t *c);
void pdf_caminho_liberar(pdf_caminho_t *c);
bool pdf_caminho_add(pdf_caminho_t *c, const pdf_segmento_t *s);

/* Añade una línea desde (x0,y0) a (x1,y1). */
bool pdf_caminho_linha(pdf_caminho_t *c, double x0, double y0, double x1, double y1);
/* Añade una curva cúbica. */
bool pdf_caminho_cubica(pdf_caminho_t *c, double x0, double y0,
                        double cx1, double cy1, double cx2, double cy2,
                        double x1, double y1);

/* Aplanar un camino: convierte las curvas en segmentos de línea. Sirve para
 * pintar y para medir. La tolerancia va en unidades de página. */
bool pdf_caminho_achatado(const pdf_caminho_t *c, pdf_caminho_t *saida, double tolerancia);

/* Matriz 2x3 (como la de PDF): a b c d e f. */
typedef struct {
    double a, b, c, d, e, f;
} pdf_mat_t;

pdf_mat_t pdf_mat_identidade(void);
pdf_mat_t pdf_mat_multiplica(const pdf_mat_t *m, const pdf_mat_t *n);
pdf_mat_t pdf_mat_translada(double tx, double ty);
pdf_mat_t pdf_mat_escalada(double sx, double sy);
pdf_mat_t pdf_mat_rotacion(double graus);
void      pdf_mat_aplica(const pdf_mat_t *m, double x, double y, double *rx, double *ry);
double   pdf_mat_escala_media(const pdf_mat_t *m);   /* factor de escala aprox. */

#endif
