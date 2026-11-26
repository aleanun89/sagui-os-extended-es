// SPDX-License-Identifier: GPL-3.0-only
/* sgl2_desenho.c - la tubería de SaguiGL 2.0 por la CPU (D475): atributos, sombreador de vértices (de 4 en
 * 4), recorte en el espacio de recorte contra los 6 planos, viewport, rasterizado en coma fija 28.4 con la
 * regla de arriba-izquierda recorrido en cuadrados de 2x2, varyings con corrección de perspectiva,
 * sombreador de fragmentos (las 4 vías del cuadrado), prueba de tijera, esténcil y Z (temprana si el
 * sombreador no descarta), mezcla y máscaras; y el muestreo de texturas de ES 2.0.
 *
 * Aquí es donde se enchufarán los backends de GPU (D475): con la misma RI enlazada, en vez de llamar al
 * intérprete se mandará el dibujo a gpu3d. */
#include "sgl2.h"

/* ------------------------------------------------------------ utilidades */
static float f_sat(float v) { return v > 0 ? (v < 1 ? v : 1) : 0; }
static int i_piso(float x) { if (x > 1e8f) x = 1e8f; if (x < -1e8f) x = -1e8f; int i = (int)x; return (float)i > x ? i - 1 : i; }
/* log2 aproximado (para el nivel de detalle): exponente + polinomio de la mantisa */
static float log2_rapido(float x)
{
    if (!(x > 0)) return -100.0f;
    union { float f; uint32_t u; } v = { x };
    int e = (int)((v.u >> 23) & 255) - 127;
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;
    float m = v.f - 1.0f;
    return (float)e + m * (1.4425449f + m * (-0.7181452f + m * 0.2737977f));
}
static float TAB255[256];
static int tab_ok;
static void tab_iniciar(void) { if (tab_ok) return; for (int i = 0; i < 256; i++) TAB255[i] = (float)i / 255.0f; tab_ok = 1; }

/* ------------------------------------------------------------ estado de un dibujo */
typedef struct { float c, dx, dy; } plano_t;
typedef struct ds ds_t;
typedef struct { ds_t *d; int st; } actx_t;
struct ds {
    sgl2_ctx_t *c; s2_prog_t *p; s2_alvo_t a;
    int nv, nvf;                    /* floats por vértice; varyings (registros) */
    int r[4];                       /* rectángulo de fragmentos: x0, y0, x1, y1 (excluido) */
    float vx, vy, vl2, va2, zm, zs; /* viewport y profundidad: zw = zm + zs * z */
    at_fonte_t at[S2_MAX_ATRIB];
    actx_t ax[2];
    int tarde;                      /* pruebas de esténcil y Z después del sombreador (hay discard) */
    float pt_x, pt_y, pt_tam;       /* el punto que se dibuja (gl_PointCoord) */
    int pt;
};

/* ------------------------------------------------------------ texturas */
static int envolver(int i, int n, GLenum m)
{
    if (m == GL_CLAMP_TO_EDGE) return i < 0 ? 0 : i >= n ? n - 1 : i;
    if (m == GL_MIRRORED_REPEAT) { int k = i % (2 * n); if (k < 0) k += 2 * n; return k < n ? k : 2 * n - 1 - k; }
    int k = i % n; return k < 0 ? k + n : k;
}
static void desempacotar(uint32_t p, float *o) { o[0] = TAB255[(p >> 16) & 255]; o[1] = TAB255[(p >> 8) & 255]; o[2] = TAB255[p & 255]; o[3] = TAB255[p >> 24]; }
/* D486: un texel; en una textura de profundidad es (d, d, d, 1) */
static void texel_ler(const s2_nivel_t *n, int i, int j, float *o)
{
    size_t k = (size_t)j * n->larg + i;
    if (n->z) { o[0] = o[1] = o[2] = n->z[k]; o[3] = 1.0f; }
    else desempacotar(n->px[k], o);
}
static void filtrar(const s2_nivel_t *n, float s, float t, int linear, GLenum ws, GLenum wt, float *o)
{
    float u = s * (float)n->larg, v = t * (float)n->alt;
    if (!linear) {
        int i = envolver(i_piso(u), n->larg, ws), j = envolver(i_piso(v), n->alt, wt);
        texel_ler(n, i, j, o);
        return;
    }
    u -= 0.5f; v -= 0.5f;
    int i0 = i_piso(u), j0 = i_piso(v);
    float a = u - (float)i0, b = v - (float)j0;
    int i1 = envolver(i0 + 1, n->larg, ws), j1 = envolver(j0 + 1, n->alt, wt);
    i0 = envolver(i0, n->larg, ws); j0 = envolver(j0, n->alt, wt);
    float p00[4], p10[4], p01[4], p11[4];
    texel_ler(n, i0, j0, p00); texel_ler(n, i1, j0, p10);
    texel_ler(n, i0, j1, p01); texel_ler(n, i1, j1, p11);
    for (int k = 0; k < 4; k++) {
        float x0 = p00[k] + (p10[k] - p00[k]) * a, x1 = p01[k] + (p11[k] - p01[k]) * a;
        o[k] = x0 + (x1 - x0) * b;
    }
}
static void amostra_nivel(const s2_tex_t *t, int face, float s, float tt, float lambda, float *o)
{
    GLenum minf = t->min_f, magf = t->mag_f;
    float c = (magf == GL_LINEAR && (minf == GL_NEAREST_MIPMAP_NEAREST || minf == GL_LINEAR_MIPMAP_NEAREST)) ? 0.5f : 0.0f;
    if (lambda <= c) { filtrar(&t->niv[face][0], s, tt, magf == GL_LINEAR, t->wrap_s, t->wrap_t, o); return; }
    if (minf == GL_NEAREST || minf == GL_LINEAR) { filtrar(&t->niv[face][0], s, tt, minf == GL_LINEAR, t->wrap_s, t->wrap_t, o); return; }
    int q = t->nniv - 1;
    int lin = minf == GL_LINEAR_MIPMAP_NEAREST || minf == GL_LINEAR_MIPMAP_LINEAR;
    if (minf == GL_NEAREST_MIPMAP_NEAREST || minf == GL_LINEAR_MIPMAP_NEAREST) {
        int d = lambda <= 0.5f ? 0 : (int)(lambda + 0.5f + 0.9999999f) - 1;
        if (d > q) d = q;
        filtrar(&t->niv[face][d], s, tt, lin, t->wrap_s, t->wrap_t, o);
        return;
    }
    if (lambda >= (float)q) { filtrar(&t->niv[face][q], s, tt, lin, t->wrap_s, t->wrap_t, o); return; }
    int d1 = i_piso(lambda); float f = lambda - (float)d1;
    float a[4], b[4];
    filtrar(&t->niv[face][d1], s, tt, lin, t->wrap_s, t->wrap_t, a);
    filtrar(&t->niv[face][d1 + 1], s, tt, lin, t->wrap_s, t->wrap_t, b);
    for (int k = 0; k < 4; k++) o[k] = a[k] + (b[k] - a[k]) * f;
}
/* dirección -> cara y (s, t) del cubo (tabla 3.21 de la especificación) */
static int cubo_cara(float x, float y, float z, float *s, float *t)
{
    float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, az = z < 0 ? -z : z;
    float sc, tc, ma; int f;
    if (ax >= ay && ax >= az) { ma = ax; if (x >= 0) { f = 0; sc = -z; tc = -y; } else { f = 1; sc = z; tc = -y; } }
    else if (ay >= az) { ma = ay; if (y >= 0) { f = 2; sc = x; tc = z; } else { f = 3; sc = x; tc = -z; } }
    else { ma = az; if (z >= 0) { f = 4; sc = x; tc = -y; } else { f = 5; sc = -x; tc = -y; } }
    if (ma == 0) ma = 1;
    *s = (sc / ma + 1) * 0.5f; *t = (tc / ma + 1) * 0.5f;
    return f;
}
void sgl2_amostrar(void *ctx, int amost, int alvo, int op, float coord[4][4], int vivo, int quad, float out[4][4])
{
    actx_t *ax = ctx;
    ds_t *d = ax->d;
    sgl2_ctx_t *c = d->c;
    int k = alvo == RI_TEX_CUBO;
    int un = amost >= 0 && amost < RI_MAX_AMOST ? d->p->amost[ax->st][amost] : 0;
    GLuint id = c->un_tex[k][un];
    s2_tex_t *t = id ? sgl2_tex(id) : &c->tex0[k];
    if (!t || !sgl2_tex_completa(t)) {
        for (int v = 0; v < 4; v++) { out[0][v] = out[1][v] = out[2][v] = 0; out[3][v] = 1; }
        return;
    }
    float s[4], tt[4]; int face[4] = { 0, 0, 0, 0 };
    if (k) for (int v = 0; v < 4; v++) face[v] = cubo_cara(coord[0][v], coord[1][v], coord[2][v], &s[v], &tt[v]);
    else for (int v = 0; v < 4; v++) { s[v] = coord[0][v]; tt[v] = coord[1][v]; }
    float lambda[4];
    float W = (float)t->niv[0][0].larg, H = (float)t->niv[0][0].alt;
    if (op == RI_TXL) for (int v = 0; v < 4; v++) lambda[v] = coord[3][v];
    else if (!quad) for (int v = 0; v < 4; v++) lambda[v] = 0;
    else {
        float dsx = (s[1] - s[0]) * W, dtx = (tt[1] - tt[0]) * H, dsy = (s[2] - s[0]) * W, dty = (tt[2] - tt[0]) * H;
        if (k && (face[1] != face[0] || face[2] != face[0])) dsx = dtx = dsy = dty = 0;
        float rx = dsx * dsx + dtx * dtx, ry = dsy * dsy + dty * dty;
        float l = 0.5f * log2_rapido(rx > ry ? rx : ry);
        for (int v = 0; v < 4; v++) lambda[v] = l + (op == RI_TXB ? coord[3][v] : 0);
    }
    for (int v = 0; v < 4; v++) {
        float o[4] = { 0, 0, 0, 1 };
        if (vivo & (1 << v)) amostra_nivel(t, face[v], s[v], tt[v], lambda[v], o);
        out[0][v] = o[0]; out[1][v] = o[1]; out[2][v] = o[2]; out[3][v] = o[3];
    }
}

/* ------------------------------------------------------------ fragmentos */
static int compara(GLenum f, float a, float b)
{
    switch (f) {
    case GL_NEVER: return 0; case GL_LESS: return a < b; case GL_EQUAL: return a == b; case GL_LEQUAL: return a <= b;
    case GL_GREATER: return a > b; case GL_NOTEQUAL: return a != b; case GL_GEQUAL: return a >= b;
    }
    return 1;
}
static uint8_t est_op(GLenum op, uint8_t s, int ref)
{
    switch (op) {
    case GL_ZERO: return 0;
    case GL_REPLACE: return (uint8_t)ref;
    case GL_INCR: return s == 255 ? 255 : (uint8_t)(s + 1);
    case GL_DECR: return s == 0 ? 0 : (uint8_t)(s - 1);
    case GL_INVERT: return (uint8_t)~s;
    case GL_INCR_WRAP: return (uint8_t)(s + 1);
    case GL_DECR_WRAP: return (uint8_t)(s - 1);
    }
    return s;
}
static void est_gravar(sgl2_ctx_t *c, uint8_t *e, GLenum op, int f)
{
    uint8_t n = est_op(op, *e, c->est[f].ref);
    uint8_t m = (uint8_t)c->est[f].masc_e;
    *e = (uint8_t)((*e & ~m) | (n & m));
}
/* esténcil y Z de las vías vivas; devuelve las que pasan */
static int testes(ds_t *d, int x, int y, int vivo, const float *z, int frente)
{
    sgl2_ctx_t *c = d->c;
    s2_alvo_t *a = &d->a;
    int f = frente ? 0 : 1;
    int com_est = c->est_on && a->est, com_z = c->z_on && a->z;
    if (!com_est && !com_z) return vivo;
    for (int v = 0; v < 4; v++) {
        if (!(vivo & (1 << v))) continue;
        int px = x + (v & 1), py = y + (v >> 1);
        uint8_t *e = com_est ? &a->est[(size_t)py * a->passo_e + px] : NULL;
        if (e) {
            int ref = c->est[f].ref < 0 ? 0 : c->est[f].ref > 255 ? 255 : c->est[f].ref;
            unsigned m = c->est[f].masc_v & 255;
            if (!compara(c->est[f].func, (float)(ref & m), (float)(*e & m))) { est_gravar(c, e, c->est[f].falha, f); vivo &= ~(1 << v); continue; }
        }
        if (com_z) {
            float *zb = &a->z[(size_t)py * a->passo_z + px];
            if (!compara(c->z_func, z[v], *zb)) { if (e) est_gravar(c, e, c->est[f].zfalha, f); vivo &= ~(1 << v); continue; }
            if (c->masc_z) *zb = z[v];
        }
        if (e) est_gravar(c, e, c->est[f].zpassa, f);
    }
    return vivo;
}
static float fator(GLenum f, int canal, const float *s, const float *dd, const float *k)
{
    switch (f) {
    case GL_ZERO: return 0;
    case GL_ONE: return 1;
    case GL_SRC_COLOR: return s[canal];
    case GL_ONE_MINUS_SRC_COLOR: return 1 - s[canal];
    case GL_DST_COLOR: return dd[canal];
    case GL_ONE_MINUS_DST_COLOR: return 1 - dd[canal];
    case GL_SRC_ALPHA: return s[3];
    case GL_ONE_MINUS_SRC_ALPHA: return 1 - s[3];
    case GL_DST_ALPHA: return dd[3];
    case GL_ONE_MINUS_DST_ALPHA: return 1 - dd[3];
    case GL_CONSTANT_COLOR: return k[canal];
    case GL_ONE_MINUS_CONSTANT_COLOR: return 1 - k[canal];
    case GL_CONSTANT_ALPHA: return k[3];
    case GL_ONE_MINUS_CONSTANT_ALPHA: return 1 - k[3];
    case GL_SRC_ALPHA_SATURATE: if (canal == 3) return 1; { float a = s[3], b = 1 - dd[3]; return a < b ? a : b; }
    }
    return 0;
}
static uint32_t empacotar(float v) { return (uint32_t)(f_sat(v) * 255.0f + 0.5f); }
static void escrever_cor(ds_t *d, int x, int y, int vivo, float cor[4][4])
{
    sgl2_ctx_t *c = d->c;
    s2_alvo_t *a = &d->a;
    if (!a->cor) return;
    int *mc = c->masc_cor;
    if (!mc[0] && !mc[1] && !mc[2] && (!mc[3] || !a->alfa)) return;
    for (int v = 0; v < 4; v++) {
        if (!(vivo & (1 << v))) continue;
        int px = x + (v & 1), py = y + (v >> 1);
        uint32_t *p = &a->cor[(long)py * a->passo + px];
        float s[4] = { f_sat(cor[0][v]), f_sat(cor[1][v]), f_sat(cor[2][v]), f_sat(cor[3][v]) };
        float r[4];
        uint32_t ant = *p;
        if (c->blend_on) {
            float dd[4]; desempacotar(ant, dd);
            if (!a->alfa) dd[3] = 1;
            for (int k = 0; k < 4; k++) {
                int e = k == 3;
                float fs = fator(c->b_src[e], k, s, dd, c->b_cor), fd = fator(c->b_dst[e], k, s, dd, c->b_cor);
                GLenum eq = c->b_eq[e];
                r[k] = eq == GL_FUNC_ADD ? s[k] * fs + dd[k] * fd : eq == GL_FUNC_SUBTRACT ? s[k] * fs - dd[k] * fd : dd[k] * fd - s[k] * fs;
            }
        } else for (int k = 0; k < 4; k++) r[k] = s[k];
        uint32_t R = empacotar(r[0]), G = empacotar(r[1]), B = empacotar(r[2]);
        uint32_t A = a->alfa ? empacotar(r[3]) : (ant & 0xFF000000u) >> 24;
        uint32_t novo = (A << 24) | (R << 16) | (G << 8) | B;
        uint32_t m = (mc[0] ? 0x00FF0000u : 0) | (mc[1] ? 0x0000FF00u : 0) | (mc[2] ? 0x000000FFu : 0) | (mc[3] && a->alfa ? 0xFF000000u : 0);
        *p = (ant & ~m) | (novo & m);
    }
}
/* un cuadrado de 2x2 con la esquina en (x, y): planos de z, 1/w y los varyings (ya por 1/w) */
static void quad(ds_t *d, int x, int y, int cob, const plano_t *pl, int frente)
{
    s2_prog_t *p = d->p;
    float z[4], iw[4], fx[4], fy[4];
    for (int v = 0; v < 4; v++) {
        fx[v] = (float)(x + (v & 1)) + 0.5f; fy[v] = (float)(y + (v >> 1)) + 0.5f;
        z[v] = f_sat(pl[0].c + pl[0].dx * fx[v] + pl[0].dy * fy[v]);
        iw[v] = pl[1].c + pl[1].dx * fx[v] + pl[1].dy * fy[v];
    }
    int vivo = cob;
    if (!d->tarde) { vivo = testes(d, x, y, vivo, z, frente); if (!vivo) return; }
    float (*ent)[4][4] = ri_exec_ent(p->xf);
    float w[4];
    for (int v = 0; v < 4; v++) w[v] = iw[v] != 0 ? 1.0f / iw[v] : 0;
    for (int k = 0; k < p->nvary; k++) {
        int r = p->vary_ent[k];
        for (int cc = 0; cc < 4; cc++) {
            const plano_t *q = &pl[2 + k * 4 + cc];
            for (int v = 0; v < 4; v++) ent[r][cc][v] = (q->c + q->dx * fx[v] + q->dy * fy[v]) * w[v];
        }
    }
    if (p->ent_coord >= 0) for (int v = 0; v < 4; v++) { ent[p->ent_coord][0][v] = fx[v]; ent[p->ent_coord][1][v] = fy[v]; ent[p->ent_coord][2][v] = z[v]; ent[p->ent_coord][3][v] = iw[v]; }
    if (p->ent_face >= 0) for (int cc = 0; cc < 4; cc++) for (int v = 0; v < 4; v++) ent[p->ent_face][cc][v] = frente ? 1.0f : 0.0f;
    if (p->ent_pcoord >= 0) for (int v = 0; v < 4; v++) {
        float s = 0, t = 0;
        if (d->pt) { s = (fx[v] - (d->pt_x - d->pt_tam * 0.5f)) / d->pt_tam; t = ((d->pt_y + d->pt_tam * 0.5f) - fy[v]) / d->pt_tam; }
        ent[p->ent_pcoord][0][v] = s; ent[p->ent_pcoord][1][v] = t; ent[p->ent_pcoord][2][v] = 0; ent[p->ent_pcoord][3][v] = 1;
    }
    vivo &= ri_exec_rodar(p->xf, 15, sgl2_amostrar, &d->ax[1]);
    if (!vivo) return;
    if (d->tarde) { vivo = testes(d, x, y, vivo, z, frente); if (!vivo) return; }
    if (p->sai_cor >= 0) escrever_cor(d, x, y, vivo, ri_exec_sai(p->xf)[p->sai_cor]);
}

/* ------------------------------------------------------------ rasterizado */
typedef struct { float x, y, z, iw; float at[4 * S2_MAX_VARY]; } jv_t;   /* vértice en la ventana (at ya por 1/w) */

static int incluir_borda(int64_t dx, int64_t dy) { return dy < 0 || (dy == 0 && dx > 0); }
static void plano_de(plano_t *q, float f0, float f1, float f2, float x0, float y0, float x1, float y1, float x2, float y2, float inv)
{
    q->dx = ((f1 - f0) * (y2 - y0) - (f2 - f0) * (y1 - y0)) * inv;
    q->dy = ((f2 - f0) * (x1 - x0) - (f1 - f0) * (x2 - x0)) * inv;
    q->c = f0 - q->dx * x0 - q->dy * y0;
}
static void triangulo_janela(ds_t *d, const jv_t *A, const jv_t *B, const jv_t *Cv, int frente, int poligono)
{
    int64_t X[3], Y[3];
    const jv_t *v[3] = { A, B, Cv };
    for (int i = 0; i < 3; i++) {
        float fx = v[i]->x * 16.0f, fy = v[i]->y * 16.0f;
        if (!(fx > -1e9f && fx < 1e9f && fy > -1e9f && fy < 1e9f)) return;
        X[i] = (int64_t)(fx < 0 ? fx - 0.5f : fx + 0.5f); Y[i] = (int64_t)(fy < 0 ? fy - 0.5f : fy + 0.5f);
    }
    int64_t area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]);
    if (area == 0) return;
    if (area < 0) {
        const jv_t *t = v[1]; v[1] = v[2]; v[2] = t;
        int64_t tx = X[1]; X[1] = X[2]; X[2] = tx; int64_t ty = Y[1]; Y[1] = Y[2]; Y[2] = ty;
        area = -area;
    }
    /* caja */
    int64_t mnx = X[0], mxx = X[0], mny = Y[0], mxy = Y[0];
    for (int i = 1; i < 3; i++) { if (X[i] < mnx) mnx = X[i]; if (X[i] > mxx) mxx = X[i]; if (Y[i] < mny) mny = Y[i]; if (Y[i] > mxy) mxy = Y[i]; }
    int x0 = (int)(mnx >> 4) - 1, x1 = (int)(mxx >> 4) + 2, y0 = (int)(mny >> 4) - 1, y1 = (int)(mxy >> 4) + 2;
    if (x0 < d->r[0]) x0 = d->r[0];
    if (y0 < d->r[1]) y0 = d->r[1];
    if (x1 > d->r[2]) x1 = d->r[2];
    if (y1 > d->r[3]) y1 = d->r[3];
    if (x0 >= x1 || y0 >= y1) return;
    x0 &= ~1; y0 &= ~1;
    /* bordes: E(P) = (Xb - Xa)(Py - Ya) - (Yb - Ya)(Px - Xa), dentro si E >= 0 (o > 0 según la regla) */
    int64_t ea[3], eb[3], ec[3], bias[3];
    for (int i = 0; i < 3; i++) {
        int a = (i + 1) % 3, b = (i + 2) % 3;
        int64_t dx = X[b] - X[a], dy = Y[b] - Y[a];
        ea[i] = -dy; eb[i] = dx; ec[i] = dy * X[a] - dx * Y[a];
        bias[i] = incluir_borda(dx, dy) ? 0 : -1;
    }
    /* planos */
    float fx[3], fy[3];
    for (int i = 0; i < 3; i++) { fx[i] = (float)X[i] / 16.0f; fy[i] = (float)Y[i] / 16.0f; }
    float inv = 256.0f / (float)area;
    plano_t pl[2 + 4 * S2_MAX_VARY];
    plano_de(&pl[0], v[0]->z, v[1]->z, v[2]->z, fx[0], fy[0], fx[1], fy[1], fx[2], fy[2], inv);
    plano_de(&pl[1], v[0]->iw, v[1]->iw, v[2]->iw, fx[0], fy[0], fx[1], fy[1], fx[2], fy[2], inv);
    int na = d->p->nvary * 4;
    for (int k = 0; k < na; k++) plano_de(&pl[2 + k], v[0]->at[k], v[1]->at[k], v[2]->at[k], fx[0], fy[0], fx[1], fy[1], fx[2], fy[2], inv);
    if (poligono && d->c->off_on) {
        float mx = pl[0].dx < 0 ? -pl[0].dx : pl[0].dx, my = pl[0].dy < 0 ? -pl[0].dy : pl[0].dy;
        float r = d->a.zbits >= 24 ? 1.0f / 16777216.0f : 1.0f / 65536.0f;
        pl[0].c += d->c->off_fator * (mx > my ? mx : my) + d->c->off_unid * r;
    }
    /* los bordes se avanzan sumando: E(x + 2, y) = E(x, y) + 32 * ea (sin multiplicar por cuadrado) */
    int64_t lin[3], passo_x[3], passo_y[3], off[3][4];
    for (int i = 0; i < 3; i++) {
        lin[i] = ea[i] * ((int64_t)x0 * 16 + 8) + eb[i] * ((int64_t)y0 * 16 + 8) + ec[i] + bias[i];
        passo_x[i] = ea[i] * 32; passo_y[i] = eb[i] * 32;
        for (int q = 0; q < 4; q++) off[i][q] = (q & 1 ? ea[i] * 16 : 0) + (q & 2 ? eb[i] * 16 : 0);
    }
    int xl = d->r[0], yl = d->r[1];
    for (int y = y0; y < y1; y += 2) {
        int64_t e0 = lin[0], e1 = lin[1], e2 = lin[2];
        int mlin = (y >= yl ? 3 : 0) | (y + 1 < y1 ? 12 : 0);
        for (int x = x0; x < x1; x += 2, e0 += passo_x[0], e1 += passo_x[1], e2 += passo_x[2]) {
            int cob = 0;
            for (int q = 0; q < 4; q++)
                if (e0 + off[0][q] >= 0 && e1 + off[1][q] >= 0 && e2 + off[2][q] >= 0) cob |= 1 << q;
            if (!cob) continue;
            int m = mlin & (y < yl ? 12 : 15);
            if (x < xl) m &= 0xA;
            if (x + 1 >= x1) m &= 0x5;
            cob &= m;
            if (cob) quad(d, x, y, cob, pl, frente);
        }
        lin[0] += passo_y[0]; lin[1] += passo_y[1]; lin[2] += passo_y[2];
    }
}

/* vértices: [0..3] posición de recorte, [4] tamaño del punto, [5..] varyings */
static void projetar(ds_t *d, const float *v, jv_t *o)
{
    float w = v[3];
    float iw = w != 0 ? 1.0f / w : 0;
    o->x = d->vx + (v[0] * iw + 1.0f) * d->vl2;
    o->y = d->vy + (v[1] * iw + 1.0f) * d->va2;
    o->z = d->zm + d->zs * (v[2] * iw);
    o->iw = iw;
    int na = d->p->nvary * 4;
    for (int k = 0; k < na; k++) o->at[k] = v[5 + k] * iw;
}
static float distancia(const float *v, int plano)
{
    switch (plano) {
    case 0: return v[3] + v[0]; case 1: return v[3] - v[0];
    case 2: return v[3] + v[1]; case 3: return v[3] - v[1];
    case 4: return v[3] + v[2]; default: return v[3] - v[2];
    }
}
static int codigo(const float *v) { int c = 0; for (int p = 0; p < 6; p++) if (distancia(v, p) < 0) c |= 1 << p; return c; }
static int face_frente(ds_t *d, const jv_t *a, const jv_t *b, const jv_t *c)
{
    float area = (b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y);
    return d->c->frente == GL_CCW ? area > 0 : area < 0;
}
static int descartar_face(ds_t *d, int frente)
{
    if (!d->c->cull_on) return 0;
    GLenum f = d->c->cull_face;
    return f == GL_FRONT_AND_BACK || (f == GL_FRONT && frente) || (f == GL_BACK && !frente);
}
#define MAX_POLI 12
static void triangulo(ds_t *d, const float *v0, const float *v1, const float *v2)
{
    int c0 = codigo(v0), c1 = codigo(v1), c2 = codigo(v2);
    if (c0 & c1 & c2) return;
    jv_t j[MAX_POLI];
    if (!(c0 | c1 | c2)) {
        projetar(d, v0, &j[0]); projetar(d, v1, &j[1]); projetar(d, v2, &j[2]);
        int fr = face_frente(d, &j[0], &j[1], &j[2]);
        if (descartar_face(d, fr)) return;
        triangulo_janela(d, &j[0], &j[1], &j[2], fr, 1);
        return;
    }
    /* recorte de Sutherland-Hodgman en el espacio de recorte */
    int nv = d->nv;
    float buf[2][MAX_POLI][5 + 4 * S2_MAX_VARY];
    int n = 3;
    memcpy(buf[0][0], v0, (size_t)nv * 4); memcpy(buf[0][1], v1, (size_t)nv * 4); memcpy(buf[0][2], v2, (size_t)nv * 4);
    int cur = 0;
    for (int p = 0; p < 6 && n >= 3; p++) {
        if (!((c0 | c1 | c2) & (1 << p))) continue;
        int m = 0;
        for (int i = 0; i < n; i++) {
            float *a = buf[cur][i], *b = buf[cur][(i + 1) % n];
            float da = distancia(a, p), db = distancia(b, p);
            if (da >= 0 && m < MAX_POLI) memcpy(buf[!cur][m++], a, (size_t)nv * 4);
            if ((da >= 0) != (db >= 0) && m < MAX_POLI) {
                float t = da / (da - db);
                for (int k = 0; k < nv; k++) buf[!cur][m][k] = a[k] + (b[k] - a[k]) * t;
                m++;
            }
        }
        n = m; cur = !cur;
    }
    if (n < 3) return;
    for (int i = 0; i < n; i++) projetar(d, buf[cur][i], &j[i]);
    int fr = -1;
    for (int i = 1; i + 1 < n; i++) {
        if (fr < 0) {
            float area = (j[i].x - j[0].x) * (j[i + 1].y - j[0].y) - (j[i + 1].x - j[0].x) * (j[i].y - j[0].y);
            if (area == 0) continue;
            fr = d->c->frente == GL_CCW ? area > 0 : area < 0;
            if (descartar_face(d, fr)) return;
        }
        triangulo_janela(d, &j[0], &j[i], &j[i + 1], fr, 1);
    }
}
static void linha(ds_t *d, const float *v0, const float *v1)
{
    int c0 = codigo(v0), c1 = codigo(v1);
    if (c0 & c1) return;
    int nv = d->nv;
    float a[5 + 4 * S2_MAX_VARY], b[5 + 4 * S2_MAX_VARY];
    memcpy(a, v0, (size_t)nv * 4); memcpy(b, v1, (size_t)nv * 4);
    if (c0 | c1) {
        float t0 = 0, t1 = 1;
        for (int p = 0; p < 6; p++) {
            float da = distancia(v0, p), db = distancia(v1, p);
            if (da < 0 && db < 0) return;
            if (da < 0) { float t = da / (da - db); if (t > t0) t0 = t; }
            else if (db < 0) { float t = da / (da - db); if (t < t1) t1 = t; }
        }
        if (t0 > t1) return;
        for (int k = 0; k < nv; k++) { a[k] = v0[k] + (v1[k] - v0[k]) * t0; b[k] = v0[k] + (v1[k] - v0[k]) * t1; }
    }
    jv_t A, B;
    projetar(d, a, &A); projetar(d, b, &B);
    float w = d->c->linha_larg;
    w = w < 1 ? 1 : w > 16 ? 16 : (float)(int)(w + 0.5f);
    float dx = B.x - A.x, dy = B.y - A.y;
    float ox = 0, oy = 0;
    if ((dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy)) oy = w * 0.5f; else ox = w * 0.5f;
    jv_t q[4] = { A, A, B, B };
    q[0].x -= ox; q[0].y -= oy; q[1].x += ox; q[1].y += oy;
    q[2].x -= ox; q[2].y -= oy; q[3].x += ox; q[3].y += oy;
    triangulo_janela(d, &q[0], &q[2], &q[3], 1, 0);
    triangulo_janela(d, &q[0], &q[3], &q[1], 1, 0);
}
static void ponto(ds_t *d, const float *v)
{
    if (codigo(v)) return;
    jv_t j;
    projetar(d, v, &j);
    float tam = d->p->sai_ptam >= 0 ? v[4] : 1.0f;
    tam = tam < 1 ? 1 : tam > 64 ? 64 : tam;
    plano_t pl[2 + 4 * S2_MAX_VARY];
    pl[0].c = j.z; pl[0].dx = pl[0].dy = 0;
    pl[1].c = j.iw; pl[1].dx = pl[1].dy = 0;
    for (int k = 0; k < d->p->nvary * 4; k++) { pl[2 + k].c = j.at[k]; pl[2 + k].dx = pl[2 + k].dy = 0; }
    float x0f = j.x - tam * 0.5f, y0f = j.y - tam * 0.5f;
    int x0 = i_piso(x0f), y0 = i_piso(y0f), x1 = i_piso(x0f + tam) + 1, y1 = i_piso(y0f + tam) + 1;
    if (x0 < d->r[0]) x0 = d->r[0];
    if (y0 < d->r[1]) y0 = d->r[1];
    if (x1 > d->r[2]) x1 = d->r[2];
    if (y1 > d->r[3]) y1 = d->r[3];
    if (x0 >= x1 || y0 >= y1) return;
    x0 &= ~1; y0 &= ~1;
    d->pt = 1; d->pt_x = j.x; d->pt_y = j.y; d->pt_tam = tam;
    for (int y = y0; y < y1; y += 2)
        for (int x = x0; x < x1; x += 2) {
            int cob = 0;
            for (int q = 0; q < 4; q++) {
                int sx = x + (q & 1), sy = y + (q >> 1);
                if (sx >= x1 || sy >= y1 || sx < d->r[0] || sy < d->r[1]) continue;
                float cx = (float)sx + 0.5f, cy = (float)sy + 0.5f;
                if (cx >= x0f && cx < x0f + tam && cy >= y0f && cy < y0f + tam) cob |= 1 << q;
            }
            if (cob) quad(d, x, y, cob, pl, 1);
        }
    d->pt = 0;
}

/* ------------------------------------------------------------ vértices */
static void atrib_ler(const at_fonte_t *f, int i, float *o)
{
    if (!f->ptr) { o[0] = f->atual[0]; o[1] = f->atual[1]; o[2] = f->atual[2]; o[3] = f->atual[3]; return; }
    o[0] = o[1] = o[2] = 0; o[3] = 1;
    const uint8_t *p = f->ptr + (size_t)i * (size_t)f->passo;
    int bytes = f->tipo == GL_FLOAT || f->tipo == GL_FIXED ? 4 : f->tipo == GL_SHORT || f->tipo == GL_UNSIGNED_SHORT ? 2 : 1;
    if (f->lim && (p < f->ptr || p + bytes * f->tam > f->lim)) return;
    for (int k = 0; k < f->tam; k++) {
        const uint8_t *q = p + k * bytes;
        switch (f->tipo) {
        case GL_FLOAT: { float x; memcpy(&x, q, 4); o[k] = x; break; }
        case GL_FIXED: { int32_t x; memcpy(&x, q, 4); o[k] = (float)x / 65536.0f; break; }
        case GL_BYTE: { int x = (int8_t)q[0]; o[k] = f->norm ? (2.0f * x + 1.0f) / 255.0f : (float)x; break; }
        case GL_UNSIGNED_BYTE: o[k] = f->norm ? TAB255[q[0]] : (float)q[0]; break;
        case GL_SHORT: { int16_t x; memcpy(&x, q, 2); o[k] = f->norm ? (2.0f * x + 1.0f) / 65535.0f : (float)x; break; }
        case GL_UNSIGNED_SHORT: { uint16_t x; memcpy(&x, q, 2); o[k] = f->norm ? (float)x / 65535.0f : (float)x; break; }
        }
    }
}
static int atrib_preparar(ds_t *d)
{
    sgl2_ctx_t *c = d->c;
    for (int l = 0; l < S2_MAX_ATRIB; l++) {
        at_fonte_t *f = &d->at[l];
        s2_atrib_t *a = &c->at[l];
        f->atual = c->atual[l]; f->ptr = NULL; f->lim = NULL;
        if (!a->ativo) continue;
        int bytes = a->tipo == GL_FLOAT || a->tipo == GL_FIXED ? 4 : a->tipo == GL_SHORT || a->tipo == GL_UNSIGNED_SHORT ? 2 : 1;
        f->tam = a->tam; f->tipo = a->tipo; f->norm = a->norm; f->passo = a->passo ? a->passo : a->tam * bytes;
        if (a->buf) {
            s2_buf_t *b = sgl2_buf(a->buf);
            if (!b || !b->dados) continue;
            f->ptr = b->dados + (unsigned long)a->ptr; f->lim = b->dados + b->tam;
        } else {
            if (!a->ptr) continue;
            f->ptr = a->ptr;
        }
    }
    return 1;
}
/* D480: para el backend de GPU (sgl2_gpu.c) */
void sgl2_at_preparar(sgl2_ctx_t *c, at_fonte_t *at)
{
    ds_t *d = aloca_zerado(sizeof *d);
    tab_iniciar();
    if (!d) { memset(at, 0, sizeof(at_fonte_t) * S2_MAX_ATRIB); return; }
    d->c = c;
    atrib_preparar(d);
    memcpy(at, d->at, sizeof(at_fonte_t) * S2_MAX_ATRIB);
    libera(d);
}
void sgl2_at_ler(const at_fonte_t *f, int i, float *o) { tab_iniciar(); atrib_ler(f, i, o); }
/* sombrea n vértices (índices «ind», o base + i) y deja cada uno en d->c->vsai + slot * nv */
static int sombrear(ds_t *d, const int *ind, int base, int n)
{
    sgl2_ctx_t *c = d->c; s2_prog_t *p = d->p;
    size_t precisa = (size_t)n * d->nv;
    if ((int)precisa > c->vsai_cap) {
        float *nb = aloca(precisa * sizeof(float));
        if (!nb) return 0;
        if (c->vsai) libera(c->vsai);
        c->vsai = nb; c->vsai_cap = (int)precisa;
    }
    ri_prog_t *pv = p->pv;
    float (*ent)[4][4] = ri_exec_ent(p->xv);
    float (*sai)[4][4] = ri_exec_sai(p->xv);
    for (int i = 0; i < n; i += 4) {
        int k = n - i < 4 ? n - i : 4;
        for (int r = 0; r < pv->nent; r++) {
            int loc = p->atr_loc[r];
            for (int v = 0; v < 4; v++) {
                float o[4] = { 0, 0, 0, 1 };
                if (v < k && loc >= 0) atrib_ler(&d->at[loc], ind ? ind[i + v] : base + i + v, o);
                ent[r][0][v] = o[0]; ent[r][1][v] = o[1]; ent[r][2][v] = o[2]; ent[r][3][v] = o[3];
            }
        }
        ri_exec_rodar(p->xv, (1 << k) - 1, sgl2_amostrar, &d->ax[0]);
        for (int v = 0; v < k; v++) {
            float *o = c->vsai + (size_t)(i + v) * d->nv;
            for (int cc = 0; cc < 4; cc++) o[cc] = p->sai_pos >= 0 ? sai[p->sai_pos][cc][v] : (cc == 3 ? 1.0f : 0.0f);
            o[4] = p->sai_ptam >= 0 ? sai[p->sai_ptam][0][v] : 1.0f;
            for (int q = 0; q < p->nvary; q++) {
                int r = p->mapa_vary[p->vary_ent[q]];
                for (int cc = 0; cc < 4; cc++) o[5 + q * 4 + cc] = sai[r][cc][v];
            }
        }
    }
    return 1;
}

static int ler_indice(const void *idx, GLenum tipo, int i)
{
    if (tipo == GL_UNSIGNED_BYTE) return ((const uint8_t *)idx)[i];
    if (tipo == GL_UNSIGNED_SHORT) { uint16_t v; memcpy(&v, (const uint8_t *)idx + 2 * i, 2); return v; }
    uint32_t v; memcpy(&v, (const uint8_t *)idx + 4 * i, 4); return v > 0x7FFFFFFFu ? 0x7FFFFFFF : (int)v;
}

static int preparar(ds_t *d, sgl2_ctx_t *c)
{
    memset(d, 0, sizeof *d);
    tab_iniciar();
    d->c = c;
    s2_prog_t *p = NULL;
    if (c->prog_uso && (int)c->prog_uso < c->nobj && c->obj_tipo[c->prog_uso] == 2) p = c->obj[c->prog_uso];
    if (!p || !p->ligado) return 0;
    d->p = p;
    if (!sgl2_alvo(c, &d->a)) return 0;
    sgl2_uniformes_subir(p);
    d->nvf = p->nvary; d->nv = 5 + 4 * p->nvary;
    d->r[0] = 0; d->r[1] = 0; d->r[2] = d->a.larg; d->r[3] = d->a.alt;
    if (c->tij_on) {
        if (c->tij[0] > d->r[0]) d->r[0] = c->tij[0];
        if (c->tij[1] > d->r[1]) d->r[1] = c->tij[1];
        if (c->tij[0] + c->tij[2] < d->r[2]) d->r[2] = c->tij[0] + c->tij[2];
        if (c->tij[1] + c->tij[3] < d->r[3]) d->r[3] = c->tij[1] + c->tij[3];
    }
    d->vx = (float)c->vp[0]; d->vy = (float)c->vp[1]; d->vl2 = (float)c->vp[2] * 0.5f; d->va2 = (float)c->vp[3] * 0.5f;
    d->zm = (c->z_longe + c->z_perto) * 0.5f; d->zs = (c->z_longe - c->z_perto) * 0.5f;
    d->ax[0].d = d; d->ax[0].st = 0; d->ax[1].d = d; d->ax[1].st = 1;
    d->tarde = p->pf->usa_kill;
    atrib_preparar(d);
    return d->r[0] < d->r[2] && d->r[1] < d->r[3];
}

void sgl2_desenhar(sgl2_ctx_t *c, GLenum modo, GLint primeiro, GLsizei n, GLenum tipo_idx, const void *idx)
{
    ds_t d;
    /* D480: el backend de GPU (gpu3d v4, sgl2_gpu.c) recibe el dibujo entero antes de que la CPU lo prepare: con el
     * programa ya enlazado traduce su RI a TGSI (una vez), sube lo que haya cambiado y manda atributos, uniformes,
     * texturas y estado. Si no lo acepta (puntos y líneas, un programa que no se traduce, un destino que la GPU no
     * lleva, un error), sigue la CPU como siempre y antes lee lo que la GPU tuviera pendiente. */
    if (n > 0 && sgl2_gpu_desenhar(c, modo, primeiro, n, tipo_idx, idx)) return;
    if (n <= 0 || !preparar(&d, c)) return;
    /* índices */
    const void *ip = idx;
    int tam_i = tipo_idx == GL_UNSIGNED_BYTE ? 1 : tipo_idx == GL_UNSIGNED_SHORT ? 2 : 4;
    if (tipo_idx && c->buf_elem) {
        s2_buf_t *b = sgl2_buf(c->buf_elem);
        if (!b || !b->dados || (unsigned long)idx + (size_t)n * tam_i > (size_t)b->tam) { sgl2_erro(GL_INVALID_OPERATION); return; }
        ip = b->dados + (unsigned long)idx;
    }
    if (tipo_idx && !ip) return;
    /* los vértices que se usan, sombreados una vez */
    int *ind = NULL, base = primeiro, nsomb = n;
    int *slot = NULL;
    if (tipo_idx) {
        int mn = 0x7FFFFFFF, mx = -1;
        for (int i = 0; i < n; i++) { int k = ler_indice(ip, tipo_idx, i); if (k < mn) mn = k; if (k > mx) mx = k; }
        slot = aloca((size_t)n * sizeof(int));
        if (!slot) { sgl2_erro(GL_OUT_OF_MEMORY); return; }
        if ((long)mx - mn + 1 <= (long)n * 4 + 4096) {
            base = mn; nsomb = mx - mn + 1;
            for (int i = 0; i < n; i++) slot[i] = ler_indice(ip, tipo_idx, i) - mn;
        } else {
            ind = aloca((size_t)n * sizeof(int));
            if (!ind) { libera(slot); sgl2_erro(GL_OUT_OF_MEMORY); return; }
            for (int i = 0; i < n; i++) { ind[i] = ler_indice(ip, tipo_idx, i); slot[i] = i; }
        }
    }
    if (!sombrear(&d, ind, base, nsomb)) { if (slot) libera(slot); if (ind) libera(ind); sgl2_erro(GL_OUT_OF_MEMORY); return; }
    #define VERT(i) (c->vsai + (size_t)(slot ? slot[(i)] : (i)) * d.nv)
    switch (modo) {
    case GL_POINTS: for (int i = 0; i < n; i++) ponto(&d, VERT(i)); break;
    case GL_LINES: for (int i = 0; i + 1 < n; i += 2) linha(&d, VERT(i), VERT(i + 1)); break;
    case GL_LINE_STRIP: for (int i = 0; i + 1 < n; i++) linha(&d, VERT(i), VERT(i + 1)); break;
    case GL_LINE_LOOP: for (int i = 0; i + 1 < n; i++) linha(&d, VERT(i), VERT(i + 1)); if (n > 2) linha(&d, VERT(n - 1), VERT(0)); break;
    case GL_TRIANGLES: for (int i = 0; i + 2 < n; i += 3) triangulo(&d, VERT(i), VERT(i + 1), VERT(i + 2)); break;
    case GL_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < n; i++) { if (i & 1) triangulo(&d, VERT(i + 1), VERT(i), VERT(i + 2)); else triangulo(&d, VERT(i), VERT(i + 1), VERT(i + 2)); }
        break;
    case GL_TRIANGLE_FAN: for (int i = 1; i + 1 < n; i++) triangulo(&d, VERT(0), VERT(i), VERT(i + 1)); break;
    }
    #undef VERT
    if (slot) libera(slot);
    if (ind) libera(ind);
    if (d.a.ver) (*d.a.ver)++;      /* D480: la CPU escribió el destino: la copia de la GPU ya no vale */
}

void sgl2_limpar(sgl2_ctx_t *c, GLbitfield m)
{
    s2_alvo_t a;
    if (sgl2_gpu_limpar(c, m)) return;       /* D480 */
    if (!sgl2_alvo(c, &a)) return;
    if (a.ver) (*a.ver)++;
    int x0 = 0, y0 = 0, x1 = a.larg, y1 = a.alt;
    if (c->tij_on) {
        if (c->tij[0] > x0) x0 = c->tij[0];
        if (c->tij[1] > y0) y0 = c->tij[1];
        if (c->tij[0] + c->tij[2] < x1) x1 = c->tij[0] + c->tij[2];
        if (c->tij[1] + c->tij[3] < y1) y1 = c->tij[1] + c->tij[3];
    }
    if (x0 >= x1 || y0 >= y1) return;
    if ((m & GL_COLOR_BUFFER_BIT) && a.cor) {
        uint32_t A = a.alfa ? empacotar(c->cor_limpa[3]) : 0;
        uint32_t v = (A << 24) | (empacotar(c->cor_limpa[0]) << 16) | (empacotar(c->cor_limpa[1]) << 8) | empacotar(c->cor_limpa[2]);
        uint32_t mm = (c->masc_cor[0] ? 0x00FF0000u : 0) | (c->masc_cor[1] ? 0x0000FF00u : 0) | (c->masc_cor[2] ? 0x000000FFu : 0) | (c->masc_cor[3] && a.alfa ? 0xFF000000u : 0);
        if (!a.alfa) mm |= 0;   /* el byte alto del framebuffer por defecto se queda */
        for (int y = y0; y < y1; y++) {
            uint32_t *l = a.cor + (long)y * a.passo;
            if (mm == 0xFFFFFFFFu || (!a.alfa && mm == 0x00FFFFFFu)) { uint32_t vv = a.alfa ? v : v & 0x00FFFFFFu; for (int x = x0; x < x1; x++) l[x] = (l[x] & ~mm) | vv; }
            else for (int x = x0; x < x1; x++) l[x] = (l[x] & ~mm) | (v & mm);
        }
    }
    if ((m & GL_DEPTH_BUFFER_BIT) && a.z && c->masc_z)
        for (int y = y0; y < y1; y++) { float *l = a.z + (size_t)y * a.passo_z; for (int x = x0; x < x1; x++) l[x] = c->z_limpo; }
    if ((m & GL_STENCIL_BUFFER_BIT) && a.est) {
        uint8_t mm = (uint8_t)c->est[0].masc_e, v = (uint8_t)c->est_limpo;
        for (int y = y0; y < y1; y++) { uint8_t *l = a.est + (size_t)y * a.passo_e; for (int x = x0; x < x1; x++) l[x] = (uint8_t)((l[x] & ~mm) | (v & mm)); }
    }
}
