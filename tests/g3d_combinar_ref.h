// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_combinar_ref.h - el resultado de referencia de un estado gpu3d v2 (D425, D426) para las pruebas de
 * unidad de los generadores de programas de píxeles (tests/r500_fp_unidad.c, tests/r600_ps_unidad.c).
 *
 * Es la tubería de docs/gpu3d.md («gpu3d v2»), con flotantes del PC: por unidad activa, la combinación de
 * color y de alfa (GL_COMBINE de OpenGL ES 1.1, §3.7.12), escala y saturación; después el especular y la
 * niebla. Las entradas son el color del vértice, el especular (su alfa es el factor de niebla), el texel de
 * cada unidad, la constante de cada unidad y el color de la niebla, todos con canales de 0 a 1. */
#ifndef G3D_COMBINAR_REF_H
#define G3D_COMBINAR_REF_H

typedef struct { float c[4]; } ref_v4;   /* r, g, b, a */

typedef struct {
    ref_v4 prim, espec, tex[4], cons[4], neblina;
} ref_entrada_t;

static float ref_sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static ref_v4 ref_fonte(const ref_entrada_t *in, int u, int ft, ref_v4 ant)
{
    switch (ft) {
    case G3D_FT_TEXTURA:   return in->tex[u];
    case G3D_FT_CONSTANTE: return in->cons[u];
    case G3D_FT_PRIMARIA:  return in->prim;
    default:               return ant;
    }
}

static float ref_func(int f, float a0, float a1, float a2)
{
    switch (f) {
    case G3D_CB_MODULAR:     return a0 * a1;
    case G3D_CB_SOMAR:       return a0 + a1;
    case G3D_CB_SOMAR_SINAL: return a0 + a1 - 0.5f;
    case G3D_CB_INTERPOLAR:  return a0 * a2 + a1 * (1 - a2);
    case G3D_CB_SUBTRAIR:    return a0 - a1;
    default:                 return a0;
    }
}

static ref_v4 ref_combinar(const gpu3d_estado2_t *e, const ref_entrada_t *in)
{
    ref_v4 ant = in->prim;
    int nu = e->unidades > 4 ? 4 : e->unidades;
    for (int u = 0; u < nu; u++) {
        const gpu3d_unidade_t *x = &e->u[u];
        if (!x->ativa) continue;
        float ac[3][3], aa[3];
        for (int k = 0; k < 3; k++) {
            ref_v4 s = ref_fonte(in, u, x->rgb_fonte[k], ant);
            for (int c = 0; c < 3; c++) {
                float v = (x->rgb_opd[k] == G3D_OPD_COR || x->rgb_opd[k] == G3D_OPD_UM_MENOS_COR) ? s.c[c] : s.c[3];
                if (x->rgb_opd[k] == G3D_OPD_UM_MENOS_COR || x->rgb_opd[k] == G3D_OPD_UM_MENOS_ALFA) v = 1 - v;
                ac[k][c] = v;
            }
            ref_v4 sa = ref_fonte(in, u, x->alfa_fonte[k], ant);
            aa[k] = x->alfa_opd[k] == G3D_OPD_UM_MENOS_ALFA ? 1 - sa.c[3] : sa.c[3];
        }
        ref_v4 r;
        if (x->rgb_func == G3D_CB_DOT3_RGB || x->rgb_func == G3D_CB_DOT3_RGBA) {
            float d = 0;
            for (int c = 0; c < 3; c++) d += (ac[0][c] - 0.5f) * (ac[1][c] - 0.5f);
            d = ref_sat(4 * d * x->rgb_escala);
            r.c[0] = r.c[1] = r.c[2] = d;
            if (x->rgb_func == G3D_CB_DOT3_RGBA) r.c[3] = d;
            else r.c[3] = ref_sat(ref_func(x->alfa_func, aa[0], aa[1], aa[2]) * x->alfa_escala);
        } else {
            for (int c = 0; c < 3; c++) r.c[c] = ref_sat(ref_func(x->rgb_func, ac[0][c], ac[1][c], ac[2][c]) * x->rgb_escala);
            r.c[3] = ref_sat(ref_func(x->alfa_func, aa[0], aa[1], aa[2]) * x->alfa_escala);
        }
        ant = r;
    }
    ref_v4 o;
    for (int c = 0; c < 3; c++) o.c[c] = ref_sat(ant.c[c] + in->espec.c[c]);
    o.c[3] = ant.c[3];
    if (e->flags & G3D2_NEBLINA) {
        float f = in->espec.c[3];
        for (int c = 0; c < 3; c++) o.c[c] = f * o.c[c] + (1 - f) * in->neblina.c[c];
    }
    return o;
}

/* Generador pseudoaleatorio (las pruebas tienen que dar siempre lo mismo). */
static unsigned ref_semente = 12345u;
static inline unsigned ref_rand(void) { ref_semente = ref_semente * 1103515245u + 12345u; return (ref_semente >> 16) & 0x7FFFu; }
static inline float ref_canal(void) { return (float)(ref_rand() % 256) / 255.0f; }
static inline ref_v4 ref_cor(void) { ref_v4 v; for (int c = 0; c < 4; c++) v.c[c] = ref_canal(); return v; }

/* Un estado v2 al azar: 1 a 4 unidades (alguna apagada), todas las funciones, orígenes y operandos. */
static inline void ref_estado_azar(gpu3d_estado2_t *e)
{
    unsigned char *p = (unsigned char *)e;
    for (unsigned i = 0; i < sizeof *e; i++) p[i] = 0;
    e->versao = 2;
    e->unidades = (unsigned char)(1 + ref_rand() % 4);
    if (ref_rand() % 4 == 0) e->flags |= G3D2_NEBLINA;
    for (int u = 0; u < e->unidades; u++) {
        gpu3d_unidade_t *x = &e->u[u];
        x->ativa = ref_rand() % 6 != 0;
        x->vaga = (unsigned char)u; x->coord = (unsigned char)u; x->aniso = 1;
        x->rgb_func = (unsigned char)(ref_rand() % 8);
        x->alfa_func = (unsigned char)(ref_rand() % 6);
        static const unsigned char esc[3] = { 1, 2, 4 };
        x->rgb_escala = esc[ref_rand() % 3]; x->alfa_escala = esc[ref_rand() % 3];
        for (int k = 0; k < 3; k++) {
            x->rgb_fonte[k] = (unsigned char)(ref_rand() % 4); x->rgb_opd[k] = (unsigned char)(ref_rand() % 4);
            x->alfa_fonte[k] = (unsigned char)(ref_rand() % 4); x->alfa_opd[k] = (unsigned char)(2 + ref_rand() % 2);
        }
    }
}

#endif
