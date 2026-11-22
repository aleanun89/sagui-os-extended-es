// SPDX-License-Identifier: GPL-2.0-or-later
/* es11_gpu_falsa.c - el camino de SaguiGL por la GPU (gpu3d v1 y v2, D425), probado en el PC.
 *
 * sgl.c se compila con -DSGL_GPU_FALSA: las llamadas al kernel van a sgl_gpu_falsa(), que hace de capa y
 * de driver «perfecto» por la CPU (flotantes, la tubería de docs/gpu3d.md «gpu3d v2», con la combinación de
 * referencia de tests/g3d_combinar_ref.h). Cada escena se dibuja por software y por la GPU falsa y se
 * comparan los píxeles: deben parecerse (no bit a bit: bordes y redondeo). Con todos los bits anunciados
 * los dibujos tienen que ir por v2; sin ninguno (un driver v1), por v1 o por software, y con el mismo
 * resultado.
 *
 *   gcc -O2 -msse2 -mfpmath=sse -DSGL_ANFITRIAO -DSGL_GPU_FALSA -Iuserland/inc -Iuserland/libsgl \
 *       -o es11_gpu_falsa tests/sgl/es11_gpu_falsa.c userland/libsgl/sgl.c userland/libsgl/sgl_fixo.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../kernel/inc/abi.h"
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "../g3d_combinar_ref.h"

typedef struct sgl_ctx sgl_ctx_t;
sgl_ctx_t *sgl_criar_contexto_mem(uint32_t *px, int larg, int alt, int zbits);
void sgl_destruir_contexto(sgl_ctx_t *c);
int  sgl_gpu_ligar(sgl_ctx_t *c);
int  sgl_gpu_ativa(void);

const void *sgl_falsa_dados;

/* ------------------------------------------------------------ la GPU falsa */

#define W 64
#define H 64
static uint32_t bits_anunciados;
static int n_v1, n_v2, n_limpar2, n_rechazos;
static uint32_t g_cor[W * H];
static float g_z[W * H];
static unsigned char g_est[W * H];
static int GL_, GA_;
typedef struct { int l, a; uint32_t *px; } nivel_t;
static struct { int usada, cubo, niveis; nivel_t n[6][12]; } vagas[32];
static gpu3d_estado_t e1;
static gpu3d_estado2_t e2;

static float fb(uint32_t b) { union { uint32_t u; float f; } x; x.u = b; return x.f; }
static float canal(uint32_t c, int sh) { return (float)((c >> sh) & 0xFF) / 255.0f; }
static ref_v4 v4(uint32_t c) { ref_v4 r = { { canal(c, 16), canal(c, 8), canal(c, 0), canal(c, 24) } }; return r; }

static void vaga_soltar(int v)
{
    for (int f = 0; f < 6; f++) for (int k = 0; k < 12; k++) { free(vagas[v].n[f][k].px); vagas[v].n[f][k].px = 0; }
    vagas[v].usada = 0;
}

static int envolver(int i, int n, int env)
{
    if (env == G3D_ENV_REPETIR) { i %= n; return i < 0 ? i + n : i; }
    if (env == G3D_ENV_ESPELHO) { int p = 2 * n; i %= p; if (i < 0) i += p; return i < n ? i : p - 1 - i; }
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}

static ref_v4 amostra(const nivel_t *n, float s, float t, int lin, int es, int et)
{
    if (!n->px) { ref_v4 z = { { 0, 0, 0, 1 } }; return z; }
    if (!lin) {
        float fs = s * (float)n->l, ft = t * (float)n->a;
        int x = (int)fs - (fs < (float)(int)fs), y = (int)ft - (ft < (float)(int)ft);
        return v4(n->px[envolver(y, n->a, et) * n->l + envolver(x, n->l, es)]);
    }
    float fs = s * (float)n->l - 0.5f, ft = t * (float)n->a - 0.5f;
    int x0 = (int)fs - (fs < (float)(int)fs), y0 = (int)ft - (ft < (float)(int)ft);
    float ax = fs - (float)x0, ay = ft - (float)y0;
    ref_v4 r;
    for (int c = 0; c < 4; c++) r.c[c] = 0;
    for (int k = 0; k < 4; k++) {
        int dx = k & 1, dy = k >> 1;
        float w = (dx ? ax : 1 - ax) * (dy ? ay : 1 - ay);
        ref_v4 v = v4(n->px[envolver(y0 + dy, n->a, et) * n->l + envolver(x0 + dx, n->l, es)]);
        for (int c = 0; c < 4; c++) r.c[c] += w * v.c[c];
    }
    return r;
}

static int cmpf(int f, float a, float b)
{
    switch (f) { case 0: return 0; case 1: return a < b; case 2: return a == b; case 3: return a <= b;
    case 4: return a > b; case 5: return a != b; case 6: return a >= b; default: return 1; }
}

static float fator(int f, ref_v4 s, ref_v4 d, int c)
{
    switch (f) {
    case G3D_F_ZERO: return 0; case G3D_F_UM: return 1; case G3D_F_ALFA: return s.c[3]; case G3D_F_UM_MENOS_ALFA: return 1 - s.c[3];
    case G3D_F_COR_ORIG: return s.c[c]; case G3D_F_UM_MENOS_COR_ORIG: return 1 - s.c[c];
    case G3D_F_COR_DEST: return d.c[c]; case G3D_F_UM_MENOS_COR_DEST: return 1 - d.c[c];
    case G3D_F_ALFA_DEST: return d.c[3]; case G3D_F_UM_MENOS_ALFA_DEST: return 1 - d.c[3];
    default: return c == 3 ? 1 : (s.c[3] < 1 - d.c[3] ? s.c[3] : 1 - d.c[3]);
    }
}

static unsigned char est_op(int op, unsigned char v, unsigned char ref)
{
    switch (op) { case G3D_EST_ZERO: return 0; case G3D_EST_SUBSTITUIR: return ref; case G3D_EST_INCR: return v < 255 ? v + 1 : 255;
    case G3D_EST_DECR: return v ? v - 1 : 0; case G3D_EST_INVERTER: return (unsigned char)~v; default: return v; }
}

/* Un triángulo con el estado v2 (o el v1 convertido): baricéntricas en flotante, perspectiva por rhw. */
static void tri(const gpu3d_estado2_t *e, const gpu3d_vert2_t *v)
{
    float x[3], y[3], z[3], q[3];
    for (int i = 0; i < 3; i++) { x[i] = fb(v[i].x); y[i] = fb(v[i].y); z[i] = fb(v[i].z); q[i] = fb(v[i].rhw); }
    float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (area == 0) return;
    for (int py = 0; py < GA_; py++)
        for (int px = 0; px < GL_; px++) {
            float cx = (float)px + 0.5f, cy = (float)py + 0.5f;
            float w0 = ((x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy)) / area;
            float w1 = ((x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            int i = py * GL_ + px;
            if ((e->flags & G3D2_TIJERA) && (px < e->tijera_x || py < e->tijera_y || px >= e->tijera_x + e->tijera_l || py >= e->tijera_y + e->tijera_a)) continue;
            float zz = w0 * z[0] + w1 * z[1] + w2 * z[2];
            float p0 = w0 * q[0], p1 = w1 * q[1], p2 = w2 * q[2], ps = p0 + p1 + p2;
            p0 /= ps; p1 /= ps; p2 /= ps;
            ref_entrada_t in;
            ref_v4 c0 = v4(v[0].cor), c1 = v4(v[1].cor), c2 = v4(v[2].cor), s0 = v4(v[0].espec), s1 = v4(v[1].espec), s2 = v4(v[2].espec);
            for (int c = 0; c < 4; c++) { in.prim.c[c] = p0 * c0.c[c] + p1 * c1.c[c] + p2 * c2.c[c]; in.espec.c[c] = p0 * s0.c[c] + p1 * s1.c[c] + p2 * s2.c[c]; }
            in.neblina = v4(e->cor_neblina | 0xFF000000u);
            for (int u = 0; u < e->unidades && u < 4; u++) {
                const gpu3d_unidade_t *un = &e->u[u];
                in.cons[u] = v4(un->constante);
                if (!un->ativa) continue;
                float tc[4];
                for (int k = 0; k < 4; k++) tc[k] = p0 * fb(v[0].tc[u][k]) + p1 * fb(v[1].tc[u][k]) + p2 * fb(v[2].tc[u][k]);
                int lin = (un->filtro & (G3D_FIL_LINEAR_MAG | G3D_FIL_LINEAR_MIN)) != 0;
                if (un->tipo == G3D_TEX_CUBO) {
                    float ax = tc[0] < 0 ? -tc[0] : tc[0], ay = tc[1] < 0 ? -tc[1] : tc[1], az = tc[2] < 0 ? -tc[2] : tc[2], sc, tt, ma; int f;
                    if (ax >= ay && ax >= az) { f = tc[0] >= 0 ? 0 : 1; ma = ax; sc = tc[0] >= 0 ? -tc[2] : tc[2]; tt = -tc[1]; }
                    else if (ay >= az) { f = tc[1] >= 0 ? 2 : 3; ma = ay; sc = tc[0]; tt = tc[1] >= 0 ? tc[2] : -tc[2]; }
                    else { f = tc[2] >= 0 ? 4 : 5; ma = az; sc = tc[2] >= 0 ? tc[0] : -tc[0]; tt = -tc[1]; }
                    in.tex[u] = amostra(&vagas[un->vaga].n[f][0], (sc / ma + 1) * 0.5f, (tt / ma + 1) * 0.5f, lin, G3D_ENV_BORDE, G3D_ENV_BORDE);
                } else
                    in.tex[u] = amostra(&vagas[un->vaga].n[0][0], tc[0] / tc[3], tc[1] / tc[3], lin, un->env_s, un->env_t);
            }
            ref_v4 c = ref_combinar(e, &in);
            if ((e->flags & G3D2_ALFA_TESTE) && !cmpf(e->func_alfa, (float)(int)(c.c[3] * 255 + 0.5f), (float)e->ref_alfa)) continue;
            if (e->flags & G3D2_ESTENCIL) {
                unsigned m = e->est_mascara;
                if (!cmpf(e->est_func, (float)(e->est_ref & m), (float)(g_est[i] & m))) {
                    g_est[i] = (unsigned char)((g_est[i] & ~e->est_escrita) | (est_op(e->est_falha, g_est[i], e->est_ref) & e->est_escrita));
                    continue;
                }
            }
            if (e->flags & G3D2_Z_TESTE) {
                if (!cmpf(e->func_z, zz, g_z[i])) {
                    if (e->flags & G3D2_ESTENCIL) g_est[i] = (unsigned char)((g_est[i] & ~e->est_escrita) | (est_op(e->est_zfalha, g_est[i], e->est_ref) & e->est_escrita));
                    continue;
                }
                if (e->flags & G3D2_Z_ESCRITA) g_z[i] = zz;
            }
            if (e->flags & G3D2_ESTENCIL) g_est[i] = (unsigned char)((g_est[i] & ~e->est_escrita) | (est_op(e->est_zpassa, g_est[i], e->est_ref) & e->est_escrita));
            ref_v4 d = v4(g_cor[i] | 0xFF000000u), o = c;
            if (e->flags & G3D2_MEZCLA) {
                for (int k = 0; k < 4; k++) {
                    float sf = fator(k < 3 ? e->mez_orig_rgb : e->mez_orig_alfa, c, d, k), df = fator(k < 3 ? e->mez_dest_rgb : e->mez_dest_alfa, c, d, k);
                    int eq = k < 3 ? e->mez_eq_rgb : e->mez_eq_alfa;
                    o.c[k] = ref_sat(eq == G3D_EQ_SUBTRAI ? c.c[k] * sf - d.c[k] * df : eq == G3D_EQ_SUBTRAI_INV ? d.c[k] * df - c.c[k] * sf : c.c[k] * sf + d.c[k] * df);
                }
            }
            uint32_t r = 0, old = g_cor[i];
            int sh[3] = { 16, 8, 0 };
            for (int k = 0; k < 3; k++) {
                uint32_t b = (e->mascara_cor >> k) & 1 ? (uint32_t)(o.c[k] * 255 + 0.5f) : (old >> sh[k]) & 0xFF;
                r |= b << sh[k];
            }
            if (e->flags & G3D2_LOGICA) {
                uint32_t s = r, dd = old & 0xFFFFFF;
                switch (e->op_logica) { case 6: r = s ^ dd; break; case 0: r = 0; break; case 15: r = 0xFFFFFF; break; default: r = s; }
                r &= 0xFFFFFF;
            }
            g_cor[i] = r;
        }
}

/* El estado v1 en forma de v2 (una textura que modula, el especular sumado, como dice v1). */
static void v1_a_v2(gpu3d_estado2_t *e)
{
    memset(e, 0, sizeof *e);
    e->versao = 2;
    e->mascara_cor = 15;
    if (e1.flags & G3D_Z_TESTE) e->flags |= G3D2_Z_TESTE;
    if (e1.flags & G3D_Z_ESCRITA) e->flags |= G3D2_Z_ESCRITA;
    e->func_z = e1.func_z == G3D_Z_MENOR ? 1 : e1.func_z == G3D_Z_MENOR_IGUAL ? 3 : 7;
    if (e1.flags & G3D_MEZCLA) { e->flags |= G3D2_MEZCLA; e->mez_orig_rgb = e->mez_orig_alfa = e1.mez_orig; e->mez_dest_rgb = e->mez_dest_alfa = e1.mez_dest; }
    if (e1.flags & G3D_TEXTURA) {
        e->unidades = 1;
        gpu3d_unidade_t *u = &e->u[0];
        u->ativa = 1; u->vaga = e1.textura; u->tipo = G3D_TEX_2D; u->filtro = e1.flags & G3D_TEX_LINEAR ? 3 : 0;
        u->env_s = e1.flags & G3D_TEX_REPETE_S ? G3D_ENV_REPETIR : G3D_ENV_BORDE; u->env_t = e1.flags & G3D_TEX_REPETE_T ? G3D_ENV_REPETIR : G3D_ENV_BORDE;
        u->rgb_func = u->alfa_func = G3D_CB_MODULAR; u->rgb_escala = u->alfa_escala = 1;
        u->rgb_fonte[0] = u->alfa_fonte[0] = G3D_FT_TEXTURA; u->rgb_fonte[1] = u->alfa_fonte[1] = G3D_FT_ANTERIOR;
        u->rgb_opd[0] = u->rgb_opd[1] = G3D_OPD_COR; u->alfa_opd[0] = u->alfa_opd[1] = G3D_OPD_ALFA;
    }
}

int sgl_gpu_falsa(unsigned op, unsigned a, unsigned b, unsigned c, const void *ptr)
{
    switch (op) {
    case GPU3D_OP_ABRIR: GL_ = (int)a; GA_ = (int)b; (void)c; return GL_ <= W && GA_ <= H ? 0 : GPU3D_SEM_SUPORTE;
    case GPU3D_OP_FECHAR: for (int i = 0; i < 32; i++) vaga_soltar(i); return 0;
    case GPU3D_OP_CAPACIDADES: {
        gpu3d_capas_t *k = (gpu3d_capas_t *)ptr;
        memset(k, 0, sizeof *k);
        k->versao = 2; k->bits = bits_anunciados; k->unidades = bits_anunciados ? 4 : 1; k->max_lado = 2048; k->max_aniso = 1;
        k->formatos = 1; k->zbits = 3; k->estencil_bits = bits_anunciados & G3D_CAP_ESTENCIL ? 8 : 0;
        return 0;
    }
    case GPU3D_OP_LIMPAR:
        for (int i = 0; i < GL_ * GA_; i++) { if (a & 1) g_cor[i] = b & 0xFFFFFF; if (a & 2) g_z[i] = 1.0f; }
        return 0;
    case GPU3D_OP_LIMPAR2: {
        if (!(bits_anunciados & G3D_CAP_LIMPAR2)) { n_rechazos++; return GPU3D_SEM_SUPORTE; }
        const gpu3d_limpar2_t *l = (const gpu3d_limpar2_t *)ptr;
        n_limpar2++;
        for (int y = 0; y < GA_; y++)
            for (int x = 0; x < GL_; x++) {
                if (l->tijera && (x < l->tx || y < l->ty || x >= l->tx + l->tl || y >= l->ty + l->ta)) continue;
                int i = y * GL_ + x;
                if (l->mascara & G3D_LIMPA_COR) {
                    uint32_t m = (l->mascara_cor & 1 ? 0xFF0000u : 0) | (l->mascara_cor & 2 ? 0xFF00u : 0) | (l->mascara_cor & 4 ? 0xFFu : 0);
                    g_cor[i] = (g_cor[i] & ~m) | (l->cor & m);
                }
                if (l->mascara & G3D_LIMPA_Z) g_z[i] = (float)l->z / 16777215.0f;
                if (l->mascara & G3D_LIMPA_EST) g_est[i] = (unsigned char)((g_est[i] & ~l->mascara_est) | (l->estencil & l->mascara_est));
            }
        return 0;
    }
    case GPU3D_OP_TEXTURA:
        if (a >= 32) return -1;
        vaga_soltar((int)a);
        if (!ptr) return 0;
        vagas[a].usada = 1; vagas[a].cubo = 0; vagas[a].niveis = 1;
        vagas[a].n[0][0].l = (int)b; vagas[a].n[0][0].a = (int)c;
        vagas[a].n[0][0].px = malloc((size_t)b * c * 4);
        memcpy(vagas[a].n[0][0].px, ptr, (size_t)b * c * 4);
        return 0;
    case GPU3D_OP_TEXTURA2: {
        const gpu3d_textura2_t *t = (const gpu3d_textura2_t *)ptr;
        if (t->nivel == 0 && t->face == 0) { vaga_soltar((int)t->vaga); vagas[t->vaga].usada = 1; vagas[t->vaga].cubo = t->tipo == G3D_TEX_CUBO; }
        nivel_t *n = &vagas[t->vaga].n[t->face][t->nivel];
        free(n->px);
        n->l = (int)t->larg; n->a = (int)t->alt; n->px = malloc(t->bytes);
        memcpy(n->px, sgl_falsa_dados, t->bytes);
        return 0;
    }
    case GPU3D_OP_ESTADO: e1 = *(const gpu3d_estado_t *)ptr; return 0;
    case GPU3D_OP_TRIANGULOS: {
        gpu3d_estado2_t e;
        v1_a_v2(&e);
        const gpu3d_vert_t *v = (const gpu3d_vert_t *)ptr;
        for (unsigned i = 0; i + 2 < a; i += 3) {
            gpu3d_vert2_t w[3];
            memset(w, 0, sizeof w);
            for (int k = 0; k < 3; k++) {
                w[k].x = v[i + k].x; w[k].y = v[i + k].y; w[k].z = v[i + k].z; w[k].rhw = v[i + k].rhw;
                w[k].cor = v[i + k].cor; w[k].espec = v[i + k].espec;
                w[k].tc[0][0] = v[i + k].u; w[k].tc[0][1] = v[i + k].v; float um = 1.0f; memcpy(&w[k].tc[0][3], &um, 4);
            }
            tri(&e, w);
        }
        n_v1++;
        return 0;
    }
    case GPU3D_OP_ESTADO2:
        if (!(bits_anunciados & G3D_CAP_V2)) { n_rechazos++; return GPU3D_SEM_SUPORTE; }
        e2 = *(const gpu3d_estado2_t *)ptr;
        return 0;
    case GPU3D_OP_TRIANGULOS2: {
        const gpu3d_vert2_t *v = (const gpu3d_vert2_t *)ptr;
        for (unsigned i = 0; i + 2 < a; i += 3) tri(&e2, v + i);
        n_v2++;
        return 0;
    }
    case GPU3D_OP_LER: {
        uint32_t *d = (uint32_t *)ptr;
        for (int y = 0; y < GA_; y++) for (int x = 0; x < GL_; x++) d[y * (int)a + x] = g_cor[y * GL_ + x];
        return 0;
    }
    default: return -1;
    }
}

/* ------------------------------------------------------------ las escenas */

static uint32_t px[W * H];

static void base_(int est)
{
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustumf(-1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); glTranslatef(0, 0, -3); glRotatef(20, 1, 0.3f, 0);
    glClearColor(0.1f, 0.2f, 0.3f, 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (est ? GL_STENCIL_BUFFER_BIT : 0));
}
static void base(void) { base_(1); }

static void quad(float z)
{
    static const GLfloat v[12] = { -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0 };
    static const GLfloat t[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    glPushMatrix(); glTranslatef(0, 0, z);
    glEnableClientState(GL_VERTEX_ARRAY); glVertexPointer(3, GL_FLOAT, 0, v);
    for (int u = 0; u < 2; u++) { glClientActiveTexture(GL_TEXTURE0 + u); glEnableClientState(GL_TEXTURE_COORD_ARRAY); glTexCoordPointer(2, GL_FLOAT, 0, t); }
    glClientActiveTexture(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glPopMatrix();
}

static GLuint textura(int semente, GLenum fmt)
{
    static unsigned char d[8 * 8 * 4];
    for (int i = 0; i < 64; i++) for (int k = 0; k < 4; k++) d[i * 4 + k] = (unsigned char)((i * (37 + semente * 11) + k * 71 + semente * 29) & 255);
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)fmt, 8, 8, 0, fmt, GL_UNSIGNED_BYTE, d);
    return id;
}

static void cena_combine(void)
{
    base();
    glActiveTexture(GL_TEXTURE0); textura(1, GL_RGBA); glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1); textura(2, GL_RGBA); glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_PRIMARY_COLOR);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
    glColor4f(0.7f, 0.3f, 0.5f, 1);
    quad(0);
    glDisable(GL_TEXTURE_2D); glActiveTexture(GL_TEXTURE0); glDisable(GL_TEXTURE_2D);
}

static void cena_fragmentos(void)
{
    base();
    glEnable(GL_DEPTH_TEST);
    textura(3, GL_RGBA); glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.4f);
    glEnable(GL_STENCIL_TEST); glStencilFunc(GL_ALWAYS, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColor4f(1, 1, 1, 1);
    quad(0);
    glDisable(GL_ALPHA_TEST); glDisable(GL_TEXTURE_2D);
    glStencilFunc(GL_EQUAL, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE); glBlendEquationOES(GL_FUNC_REVERSE_SUBTRACT_OES);
    glColor4f(0.3f, 0.1f, 0.2f, 1);
    quad(0.2f);
    glBlendEquationOES(GL_FUNC_ADD_OES); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST); glScissor(10, 10, 30, 20);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    glColor4f(0.2f, 0.9f, 0.9f, 1);
    quad(0.4f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDisable(GL_SCISSOR_TEST);
    glDepthFunc(GL_LESS); glDisable(GL_DEPTH_TEST);
}

static void cena_cubo_niebla(void)
{
    base();
    static unsigned char f[6][4 * 4 * 4];
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP_OES, id);
    for (int k = 0; k < 6; k++) {
        for (int i = 0; i < 16; i++) { f[k][i * 4] = (unsigned char)(k * 40 + i * 5); f[k][i * 4 + 1] = (unsigned char)(200 - k * 30); f[k][i * 4 + 2] = (unsigned char)(i * 15); f[k][i * 4 + 3] = 255; }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + k, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, f[k]);
    }
    glEnable(GL_TEXTURE_CUBE_MAP_OES);
    glEnable(GL_TEXTURE_GEN_STR_OES);
    glTexGeniOES(GL_TEXTURE_GEN_STR_OES, GL_TEXTURE_GEN_MODE_OES, GL_REFLECTION_MAP_OES);
    glEnable(GL_FOG); glFogf(GL_FOG_MODE, GL_LINEAR); glFogf(GL_FOG_START, 2); glFogf(GL_FOG_END, 4.5f);
    static const GLfloat fc[4] = { 0.5f, 0.5f, 0.6f, 1 };
    glFogfv(GL_FOG_COLOR, fc);
    glNormal3f(0.2f, 0.3f, 1);
    quad(0);
    glDisable(GL_FOG); glDisable(GL_TEXTURE_GEN_STR_OES); glDisable(GL_TEXTURE_CUBE_MAP_OES);
}

static void cena_v1(void)
{
    base_(0);   /* sin esténcil: el glClear cabe en v1 */
    glEnable(GL_DEPTH_TEST);
    textura(4, GL_RGBA); glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);   /* cabe en v1 (blanco en el vértice) */
    glDepthMask(GL_FALSE);
    quad(0);
    glDepthMask(GL_TRUE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
}

typedef void (*cena_f)(void);
static const cena_f cenas[] = { cena_combine, cena_fragmentos, cena_cubo_niebla, cena_v1 };
static const char *nomes[] = { "combine", "fragmentos", "cubo_niebla", "v1" };

/* dibuja y devuelve cuántos píxeles se apartan más de 12 en algún canal */
static int comparar(int n, int gpu, uint32_t *saida)
{
    sgl_ctx_t *c = sgl_criar_contexto_mem(saida, W, H, 32);
    if (gpu && !sgl_gpu_ligar(c)) { printf("no se pudo ligar la GPU falsa\n"); exit(1); }
    cenas[n]();
    glFinish();
    sgl_destruir_contexto(c);
    return 0;
}

int main(void)
{
    static uint32_t sw[W * H], g[W * H];
    int mal = 0;
    for (int modo = 0; modo < 2; modo++) {
        bits_anunciados = modo ? 0xFFFFFFFFu & ~(G3D_CAP_TEX_MIPMAP | G3D_CAP_DESTINO_TEX | G3D_CAP_TEX_ANISO | G3D_CAP_TEX_NPOT_MIP) : 0;
        for (int n = 0; n < (int)(sizeof cenas / sizeof cenas[0]); n++) {
            n_v1 = n_v2 = n_limpar2 = n_rechazos = 0;
            comparar(n, 0, sw);
            comparar(n, 1, g);
            int dif = 0;
            for (int i = 0; i < W * H; i++)
                for (int sh = 0; sh < 24; sh += 8) {
                    int d = (int)((sw[i] >> sh) & 255) - (int)((g[i] >> sh) & 255);
                    if (d > 12 || d < -12) { dif++; break; }
                }
            int ok = dif <= W * H / 20;   /* los bordes de los triángulos no coinciden */
            if (modo == 1 && n < 3 && n_v2 == 0) ok = 0;   /* con v2 anunciado, tiene que ir por v2 */
            if (modo == 0 && n_v2) ok = 0;
            if (n == 3 && n_v1 == 0) ok = 0;   /* REPLACE sin escribir Z cabe en v1 */
            printf("%s %s: %d píxeles distintos, %d lotes v1, %d v2, %d LIMPAR2, %d rechazos: %s\n", modo ? "v2" : "v1", nomes[n], dif,
                   n_v1, n_v2, n_limpar2, n_rechazos, ok ? "bien" : "MAL");
            if (!ok) mal++;
        }
    }
    printf("gpu_falsa: %s\n", mal ? "MAL" : "bien");
    return mal ? 1 : 0;
}
