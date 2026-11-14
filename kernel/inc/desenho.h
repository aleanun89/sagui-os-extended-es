// SPDX-License-Identifier: GPL-3.0-only
/* desenho.h - dibujo con recorte sobre una superficie de píxeles (fase 5). */
#ifndef SAGUI_DESENHO_H
#define SAGUI_DESENHO_H
#include "tipos.h"
#include "arte.h"

typedef struct {
    uint32_t *px;
    int larg, alt;
    int cx0, cy0, cx1, cy1;   /* recorte: solo se pinta [cx0,cx1) x [cy0,cy1) */
} superficie_t;

void sup_iniciar(superficie_t *s, uint32_t *px, int larg, int alt);
void sup_recorte(superficie_t *s, int x, int y, int l, int a);   /* l<=0: sin recorte */
void sup_retangulo(superficie_t *s, int x, int y, int l, int a, uint32_t cor);
void sup_moldura(superficie_t *s, int x, int y, int l, int a, uint32_t cor);
void sup_texto(superficie_t *s, int x, int y, const char *t, uint32_t cor);
void sup_desenho(superficie_t *s, int x, int y, const desenho_t *d, int escala);
/* Dibujo en 16x16: los de 16 en tamaño natural, los de 32 reducidos a la mitad. */
void sup_desenho_metade(superficie_t *s, int x, int y, const desenho_t *d);
void sup_copiar(superficie_t *s, int x, int y, const uint32_t *origem, int larg, int alt, int passo);
int  texto_largura(const char *t);
/* Copia origen (ol x oa) ampliada o reducida al rectángulo (x, y, l, a). */
void sup_escalar(superficie_t *s, int x, int y, int l, int a, const uint32_t *origem, int ol, int oa);

#endif
