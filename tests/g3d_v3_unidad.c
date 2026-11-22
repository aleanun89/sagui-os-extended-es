// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_v3_unidad.c - prueba de unidad en el PC de gpu3d v3 por la CPU (kernel/gui/gpu3d_v3.c, D457).
 *
 *   1. la coma flotante por software (multiplicar, sumar, dividir y raíz) contra la del procesador,
 *      bit a bit, con millones de valores;
 *   2. la transformación entera (g3d_v3_cpu) contra una referencia de OpenGL ES 1.1 escrita aquí con
 *      coma flotante de verdad: matrices, recorte al volumen de vista, división por w, vista, descarte
 *      de caras, niebla lineal (las dos salidas), matriz de textura, color por vértice (bytes y floats)
 *      y constante, triángulos, tiras y abanicos, con y sin índices. Las dos listas de triángulos se
 *      pintan con un rasterizador pequeño y se comparan las imágenes (color, Z, niebla y coordenadas);
 *   3. los límites: índices y vértices fuera del búfer, búferes que no están, infinitos y NaN en los
 *      datos: nada se lee fuera y lo que sale es siempre un triángulo válido para la capa.
 *
 * gcc -O2 -fno-builtin -Wall -Wextra -Wno-builtin-declaration-mismatch -I kernel/inc tests/g3d_v3_unidad.c \
 *     kernel/gui/gpu3d_v3.c kernel/gui/gpu3d_mem.c -lm
 *
 * Usa las cabeceras del kernel (con sus tipos): por eso no incluye las de la libc y declara a mano lo poco
 * que usa (como tests/gen3_3d_unidad.c). */
#include "gpu3d.h"

int printf(const char *, ...);
float sqrtf(float), tanf(float), cosf(float), sinf(float), fabsf(float);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *o, size_t n) { unsigned char *p = d; const unsigned char *q = o; while (n--) *p++ = *q++; return d; }
#define isfinite(x) __builtin_isfinite(x)

static int falhas;
#define VERIFICAR(c, ...) do { if (!(c)) { if (falhas < 20) { printf("FALLO: " __VA_ARGS__); printf("\n"); } falhas++; } } while (0)

static uint32_t semente = 12345;
static uint32_t azar(void) { semente = semente * 1664525u + 1013904223u; return semente >> 8; }
static float azar_f(float a, float b) { return a + (b - a) * (float)(azar() & 0xFFFF) / 65535.0f; }
static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float de_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

/* ------------------------------------------------------------ 1. coma flotante */

static int normal_ou_zero(float f) { return f == 0.0f || (isfinite(f) && fabsf(f) >= 1.17549435e-38f); }

static void prova_flutuante(void)
{
    int n = 0;
    for (int i = 0; i < 3000000; i++) {
        uint32_t a = (azar() << 8) ^ azar(), b = (azar() << 8) ^ azar();
        /* exponentes cercanos para que las sumas y restas tengan cancelación */
        if (i & 1) b = (b & 0x807FFFFFu) | ((((a >> 23) & 0xFF) + (azar() % 5) - 2) & 0xFF) << 23;
        volatile float fa = de_bits(a), fb = de_bits(b);
        if (!normal_ou_zero(fa) || !normal_ou_zero(fb)) continue;
        volatile float m = fa * fb, s = fa + fb, d = fa / fb, r = sqrtf(fa);
        if (normal_ou_zero(m) && (m != 0.0f || fa == 0.0f || fb == 0.0f)) { VERIFICAR(g3d_fl_mul(a, b) == bits(m) || (m == 0.0f && !(g3d_fl_mul(a, b) & 0x7FFFFFFFu)), "mul %08x %08x: %08x y no %08x", a, b, g3d_fl_mul(a, b), bits(m)); n++; }
        if (normal_ou_zero(s)) { uint32_t q = g3d_fl_soma(a, b); VERIFICAR(q == bits(s) || (s == 0.0f && !(q & 0x7FFFFFFFu)), "soma %08x %08x: %08x y no %08x", a, b, q, bits(s)); n++; }
        if (fb != 0.0f && normal_ou_zero(d) && (d != 0.0f || fa == 0.0f)) { uint32_t q = g3d_fl_div(a, b); VERIFICAR(q == bits(d) || (d == 0.0f && !(q & 0x7FFFFFFFu)), "div %08x %08x: %08x y no %08x", a, b, q, bits(d)); n++; }
        if (fa > 0.0f) { VERIFICAR(g3d_fl_raiz(a) == bits(r), "raiz %08x: %08x y no %08x", a, g3d_fl_raiz(a), bits(r)); n++; }
    }
    /* casos a mano */
    VERIFICAR(g3d_fl_mul(bits(3.0f), bits(-0.5f)) == bits(-1.5f), "3 x -0.5");
    VERIFICAR(g3d_fl_soma(bits(1.0f), bits(-1.0f)) == 0, "1 - 1");
    VERIFICAR(g3d_fl_div(bits(1.0f), bits(3.0f)) == bits(1.0f / 3.0f), "1 / 3");
    VERIFICAR(g3d_fl_raiz(bits(2.0f)) == bits(sqrtf(2.0f)), "raiz de 2");
    VERIFICAR(g3d_fl_raiz(bits(-4.0f)) == 0, "raiz de un negativo");
    VERIFICAR(((g3d_fl_div(bits(1.0f), 0) >> 23) & 0xFF) == 0xFF, "1 / 0 es infinito");
    VERIFICAR(((g3d_fl_mul(0x7F800000u, bits(2.0f)) >> 23) & 0xFF) == 0xFF, "infinito x 2");
    printf("  coma flotante por software: %d operaciones iguales bit a bit a las del procesador\n", n);
}

/* ------------------------------------------------------------ 2. la tubería contra la referencia */

#define L 96
#define A 72
typedef struct { float x, y, z, rhw, cor[4], esp[3], f, tc[4]; } rv_t;      /* un vértice ya en pantalla */
typedef struct { float c[4], cor[4], esp[3], f, tc[4]; } cv_t;              /* un vértice en recorte */
typedef struct { float cor[4], esp[3], f, z, tc[2]; int n; } px_t;
static px_t img[2][L * A];

static float aresta(const rv_t *a, const rv_t *b, float x, float y) { return (b->x - a->x) * (y - a->y) - (b->y - a->y) * (x - a->x); }

/* Un rasterizador de juguete: acumula (sin Z) lo que cada triángulo deja en cada píxel. */
static void pintar(px_t *im, const rv_t *a, const rv_t *b, const rv_t *c)
{
    float area = aresta(a, b, c->x, c->y);
    if (fabsf(area) < 1e-6f) return;
    for (int y = 0; y < A; y++)
        for (int x = 0; x < L; x++) {
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = aresta(b, c, px, py) / area, w1 = aresta(c, a, px, py) / area, w2 = aresta(a, b, px, py) / area;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            px_t *p = &im[y * L + x];
            float q = w0 * a->rhw + w1 * b->rhw + w2 * c->rhw;
            #define PERSP(campo) ((w0 * a->campo * a->rhw + w1 * b->campo * b->rhw + w2 * c->campo * c->rhw) / q)
            for (int k = 0; k < 4; k++) p->cor[k] += PERSP(cor[k]);
            for (int k = 0; k < 3; k++) p->esp[k] += PERSP(esp[k]);
            p->f += PERSP(f);
            p->tc[0] += PERSP(tc[0]) / PERSP(tc[3]); p->tc[1] += PERSP(tc[1]) / PERSP(tc[3]);
            #undef PERSP
            p->z += w0 * a->z + w1 * b->z + w2 * c->z;
            p->n++;
        }
}

/* ---- la referencia: ES 1.1 con coma flotante ---- */

typedef struct {
    float mvp[16], mv[16], tex[16];
    int   com_tex_mat, neblina, mezclada, descarte;
    float neb_ini, neb_fim, neb_cor[3];
    float vx, vy, vw, vh, perto, longe;      /* glViewport (y hacia arriba, en un destino de L x A) y glDepthRangef */
    float cor_const[4], tc_const[4];
} cena_t;

static void mat_vec(const float *m, const float *v, float *r) { for (int l = 0; l < 4; l++) r[l] = m[l] * v[0] + m[4 + l] * v[1] + m[8 + l] * v[2] + m[12 + l] * v[3]; }

static void ref_vertice(const cena_t *s, const float pos[4], const float cor[4], const float tc[4], cv_t *o)
{
    float e[4];
    mat_vec(s->mvp, pos, o->c);
    mat_vec(s->mv, pos, e);
    for (int k = 0; k < 4; k++) o->cor[k] = cor[k] * 255.0f;
    o->esp[0] = o->esp[1] = o->esp[2] = 0;
    o->f = 1.0f;
    if (s->neblina) {
        float d = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
        float f = (s->neb_fim - d) / (s->neb_fim - s->neb_ini);
        f = f < 0 ? 0 : f > 1 ? 1 : f;
        if (s->mezclada) { for (int k = 0; k < 3; k++) { o->cor[k] *= f; o->esp[k] = s->neb_cor[k] * 255.0f * (1.0f - f); } }
        else o->f = f;
    }
    if (s->com_tex_mat) mat_vec(s->tex, tc, o->tc); else memcpy(o->tc, tc, sizeof o->tc);
}

static void ref_triangulo(const cena_t *s, px_t *im, const cv_t *a, const cv_t *b, const cv_t *c)
{
    cv_t pol[2][12];
    int n = 3, cur = 0;
    pol[0][0] = *a; pol[0][1] = *b; pol[0][2] = *c;
    for (int p = 0; p < 6 && n >= 3; p++) {
        cv_t *in = pol[cur], *out = pol[cur ^ 1];
        int m = 0;
        for (int i = 0; i < n; i++) {
            const cv_t *x = &in[i], *y = &in[(i + 1) % n];
            float dx = p & 1 ? x->c[3] - x->c[p >> 1] : x->c[3] + x->c[p >> 1];
            float dy = p & 1 ? y->c[3] - y->c[p >> 1] : y->c[3] + y->c[p >> 1];
            if (dx >= 0) out[m++] = *x;
            if ((dx >= 0) != (dy >= 0)) {
                float t = dx / (dx - dy);
                const float *fa = (const float *)x, *fb = (const float *)y;
                float *fr = (float *)&out[m++];
                for (unsigned k = 0; k < sizeof(cv_t) / sizeof(float); k++) fr[k] = fa[k] + (fb[k] - fa[k]) * t;
            }
        }
        n = m; cur ^= 1;
    }
    if (n < 3) return;
    rv_t r[12];
    float area = 0;
    for (int i = 0; i < n; i++) {
        const cv_t *v = &pol[cur][i];
        float iw = 1.0f / v->c[3];
        r[i].x = s->vx + (v->c[0] * iw + 1.0f) * 0.5f * s->vw;
        r[i].y = ((float)A - s->vy) - (v->c[1] * iw + 1.0f) * 0.5f * s->vh;
        r[i].z = s->perto + (s->longe - s->perto) * (v->c[2] * iw + 1.0f) * 0.5f;
        r[i].rhw = iw;
        memcpy(r[i].cor, v->cor, sizeof r[i].cor); memcpy(r[i].esp, v->esp, sizeof r[i].esp);
        r[i].f = v->f; memcpy(r[i].tc, v->tc, sizeof r[i].tc);
    }
    for (int i = 0; i < n; i++) { int j = (i + 1) % n; area += r[i].x * r[j].y - r[j].x * r[i].y; }
    if (area == 0) return;
    if (s->descarte == G3D_DESC_POS && area > 0) return;
    if (s->descarte == G3D_DESC_NEG && area < 0) return;
    for (int i = 1; i + 1 < n; i++) pintar(im, &r[0], &r[i], &r[i + 1]);
}

/* ---- lo que se prueba ---- */

static const uint8_t *mem_p[8];
static uint32_t mem_n[8];
static g3d_v3_mem_t mem(void *ctx, int vaga) { (void)ctx; return vaga >= 0 && vaga < 8 ? (g3d_v3_mem_t){ mem_p[vaga], mem_n[vaga] } : (g3d_v3_mem_t){ 0, 0 }; }
static int n_saida, saida_mal;
static float cx0, cx1, cy0, cy1;   /* la caja de la vista */
static int sair(void *ctx, const gpu3d_estado2_t *e, const gpu3d_vert2_t *v, int n)
{
    (void)e;
    px_t *im = ctx;
    if (n % 3) saida_mal++;
    for (int i = 0; i + 2 < n; i += 3) {
        rv_t r[3];
        for (int k = 0; k < 3; k++) {
            const gpu3d_vert2_t *g = &v[i + k];
            r[k].x = de_bits(g->x); r[k].y = de_bits(g->y); r[k].z = de_bits(g->z); r[k].rhw = de_bits(g->rhw);
            /* lo que la capa exige a un vértice v2 */
            if (!isfinite(r[k].x) || !isfinite(r[k].y) || r[k].x < cx0 - 0.01f || r[k].x > cx1 + 0.01f || r[k].y < cy0 - 0.01f ||
                r[k].y > cy1 + 0.01f || !(r[k].z >= 0.0f && r[k].z <= 1.0f) || !(r[k].rhw > 0.0f) || !isfinite(r[k].rhw)) saida_mal++;
            r[k].cor[0] = (float)((g->cor >> 16) & 0xFF); r[k].cor[1] = (float)((g->cor >> 8) & 0xFF);
            r[k].cor[2] = (float)(g->cor & 0xFF); r[k].cor[3] = (float)(g->cor >> 24);
            r[k].esp[0] = (float)((g->espec >> 16) & 0xFF); r[k].esp[1] = (float)((g->espec >> 8) & 0xFF); r[k].esp[2] = (float)(g->espec & 0xFF);
            r[k].f = (float)(g->espec >> 24) / 255.0f;
            for (int c = 0; c < 4; c++) { r[k].tc[c] = de_bits(g->tc[0][c]); if (!isfinite(r[k].tc[c])) saida_mal++; }
        }
        if (im) pintar(im, &r[0], &r[1], &r[2]);
        n_saida++;
    }
    return G3D_OK;
}

static void perspectiva(float *m, float fovy, float asp, float n, float f)
{
    float t = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    memset(m, 0, 64);
    m[0] = t / asp; m[5] = t; m[10] = (f + n) / (n - f); m[11] = -1.0f; m[14] = 2.0f * f * n / (n - f);
}
static void mul(float *c, const float *a, const float *b)
{
    float r[16];
    for (int col = 0; col < 4; col++) for (int l = 0; l < 4; l++) { r[col * 4 + l] = 0; for (int k = 0; k < 4; k++) r[col * 4 + l] += a[k * 4 + l] * b[col * 4 + k]; }
    memcpy(c, r, sizeof r);
}
static void rotacao(float *m, float ang, int eixo)
{
    float c = cosf(ang), s = sinf(ang);
    int a = (eixo + 1) % 3, b = (eixo + 2) % 3;
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    m[a * 4 + a] = c; m[b * 4 + b] = c; m[a * 4 + b] = s; m[b * 4 + a] = -s;
}

typedef struct { float pos[4]; uint8_t corb[4]; float corf[4]; float tc[4]; } vert_t;   /* 56 bytes */

static void prova_tuberia(void)
{
    static gpu3d_vert2_t lote[96];
    static vert_t vs[64];
    static uint16_t idx[96];
    int cenas = 0, total_mal = 0, total_px = 0, pintadas = 0;
    for (int it = 0; it < 400; it++) {
        cena_t s;
        memset(&s, 0, sizeof s);
        float P[16], R[16], T[16];
        perspectiva(P, azar_f(40, 90), 4.0f / 3.0f, azar_f(0.3f, 1.5f), azar_f(20, 80));
        rotacao(R, azar_f(-1.2f, 1.2f), (int)(azar() % 3));
        memset(T, 0, sizeof T); T[0] = T[5] = T[10] = T[15] = 1.0f;
        T[12] = azar_f(-1, 1); T[13] = azar_f(-1, 1); T[14] = azar_f(-7, -1.5f);
        mul(s.mv, T, R);
        if (it % 7 == 3) { memset(P, 0, sizeof P); P[0] = 0.3f; P[5] = 0.4f; P[10] = -0.1f; P[15] = 1.0f; }   /* ortográfica */
        mul(s.mvp, P, s.mv);
        s.com_tex_mat = it & 1;
        memset(s.tex, 0, sizeof s.tex); s.tex[0] = azar_f(0.5f, 3); s.tex[5] = azar_f(0.5f, 3); s.tex[10] = 1; s.tex[15] = 1; s.tex[12] = azar_f(-1, 1); s.tex[13] = azar_f(-1, 1);
        s.neblina = (it >> 1) & 1; s.mezclada = (it >> 2) & 1;
        s.neb_ini = azar_f(0.5f, 3); s.neb_fim = s.neb_ini + azar_f(1, 8);
        for (int k = 0; k < 3; k++) s.neb_cor[k] = (float)(azar() & 0xFF) / 255.0f;
        s.descarte = (int)(azar() % 3);
        s.vx = (float)(azar() % 20); s.vy = (float)(azar() % 16); s.vw = (float)L - s.vx - (float)(azar() % 20); s.vh = (float)A - s.vy - (float)(azar() % 16);
        s.perto = it % 5 == 0 ? 0.2f : 0.0f; s.longe = it % 5 == 0 ? 0.9f : 1.0f;
        int modo = it % 3, com_idx = (it >> 3) & 1, cor_tipo = it % 4;   /* 0 bytes, 1 floats, 2 y 3 constante */
        int formato_pos = 2 + (int)(azar() % 3);
        int nv = 12 + (int)(azar() % 3) * 3;
        for (int i = 0; i < nv; i++) {
            vert_t *v = &vs[i];
            v->pos[0] = azar_f(-4, 4); v->pos[1] = azar_f(-4, 4); v->pos[2] = formato_pos > 2 ? azar_f(-4, 4) : 0.0f;
            v->pos[3] = formato_pos > 3 ? azar_f(0.5f, 2) : 1.0f;
            for (int k = 0; k < 4; k++) { v->corb[k] = (uint8_t)azar(); v->corf[k] = (float)v->corb[k] / 255.0f; }
            v->tc[0] = azar_f(-2, 2); v->tc[1] = azar_f(-2, 2); v->tc[2] = 0; v->tc[3] = 1;
        }
        int ni = nv;
        for (int i = 0; i < ni; i++) idx[i] = (uint16_t)(azar() % (unsigned)nv);
        for (int k = 0; k < 4; k++) s.cor_const[k] = (float)(azar() & 0xFF) / 255.0f;
        s.tc_const[0] = 0.25f; s.tc_const[1] = 0.75f; s.tc_const[2] = 0; s.tc_const[3] = 1;
        int com_tc = it % 9 != 4;
        /* ---- la referencia ---- */
        memset(img, 0, sizeof img);
        #define REF(i, j, k) do { cv_t q[3]; int e3[3] = { (i), (j), (k) }; \
            for (int z = 0; z < 3; z++) { const vert_t *v = &vs[com_idx ? idx[e3[z]] : e3[z]]; \
                float p4[4] = { v->pos[0], v->pos[1], formato_pos > 2 ? v->pos[2] : 0.0f, formato_pos > 3 ? v->pos[3] : 1.0f }; \
                ref_vertice(&s, p4, cor_tipo >= 2 ? s.cor_const : v->corf, com_tc ? v->tc : s.tc_const, &q[z]); } \
            ref_triangulo(&s, img[0], &q[0], &q[1], &q[2]); } while (0)
        if (modo == 0) for (int i = 0; i + 2 < ni; i += 3) REF(i, i + 1, i + 2);
        else if (modo == 1) for (int i = 0; i + 2 < ni; i++) { if (i & 1) REF(i + 1, i, i + 2); else REF(i, i + 1, i + 2); }
        else for (int i = 1; i + 1 < ni; i++) REF(0, i, i + 1);
        #undef REF
        /* ---- gpu3d_v3.c ---- */
        gpu3d_transf_t t;
        gpu3d_estado2_t e;
        memset(&t, 0, sizeof t); memset(&e, 0, sizeof e);
        e.versao = 2; e.unidades = 1;
        t.versao = 3;
        for (int k = 0; k < 16; k++) { t.mvp[k] = bits(s.mvp[k]); t.mv[k] = bits(s.mv[k]); t.tex[0][k] = bits(s.tex[k]); }
        if (s.com_tex_mat) t.flags |= G3D3_TEX_MAT(0);
        if (s.neblina) {
            t.flags |= G3D3_NEBLINA | (s.mezclada ? G3D3_NEB_MEZCLADA : 0);
            t.neb_a = bits(s.neb_fim / (s.neb_fim - s.neb_ini)); t.neb_b = bits(1.0f / (s.neb_fim - s.neb_ini));
            t.neb_cor = ((uint32_t)(s.neb_cor[0] * 255.0f + 0.5f) << 16) | ((uint32_t)(s.neb_cor[1] * 255.0f + 0.5f) << 8) | (uint32_t)(s.neb_cor[2] * 255.0f + 0.5f);
        }
        t.descarte = (unsigned char)s.descarte;
        t.vp_escala[0] = bits(s.vw * 0.5f); t.vp_desloc[0] = bits(s.vx + s.vw * 0.5f);
        t.vp_escala[1] = bits(-s.vh * 0.5f); t.vp_desloc[1] = bits((float)A - s.vy - s.vh * 0.5f);
        t.vp_escala[2] = bits((s.longe - s.perto) * 0.5f); t.vp_desloc[2] = bits((s.longe + s.perto) * 0.5f);
        cx0 = s.vx; cx1 = s.vx + s.vw; cy0 = (float)A - s.vy - s.vh; cy1 = (float)A - s.vy;
        t.cor = ((uint32_t)(s.cor_const[3] * 255.0f + 0.5f) << 24) | ((uint32_t)(s.cor_const[0] * 255.0f + 0.5f) << 16) |
                ((uint32_t)(s.cor_const[1] * 255.0f + 0.5f) << 8) | (uint32_t)(s.cor_const[2] * 255.0f + 0.5f);
        for (int k = 0; k < 4; k++) t.tc_const[0][k] = bits(s.tc_const[k]);
        mem_p[0] = (const uint8_t *)vs; mem_n[0] = (uint32_t)nv * sizeof(vert_t);
        mem_p[1] = (const uint8_t *)idx; mem_n[1] = (uint32_t)ni * 2;
        g3d_desenho3_t g = { &e, &t, NULL, { 0 }, (uint32_t)nv - 1, (uint32_t)ni * 2 };
        g.d.modo = (unsigned)modo; g.d.n = (unsigned)ni; g.d.indices = com_idx ? 1u : GPU3D_SEM_INDICES;
        g.d.pos = (gpu3d_atrib_t){ 0, (unsigned char)formato_pos, sizeof(vert_t), 0 };
        if (cor_tipo == 0) g.d.cor_a = (gpu3d_atrib_t){ 0, G3D_AT_UBYTE4N, sizeof(vert_t), 16 };
        if (cor_tipo == 1) g.d.cor_a = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT4, sizeof(vert_t), 20 };
        if (com_tc) g.d.tc[0] = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT2, sizeof(vert_t), 36 };
        g3d_v3_cpu_t c = { mem, sair, img[1], lote, 96 };
        saida_mal = 0;
        int r = g3d_v3_cpu(&g, &c);
        VERIFICAR(r == G3D_OK && !saida_mal, "escena %d: g3d_v3_cpu devuelve %d, %d vertices fuera de lo que admite la capa", it, r, saida_mal);
        /* ---- comparar ---- */
        int mal = 0, cheios = 0;
        for (int i = 0; i < L * A; i++) {
            const px_t *a = &img[0][i], *b = &img[1][i];
            if (a->n) cheios++;
            int dif = a->n != b->n;
            for (int k = 0; k < 4 && !dif; k++) if (fabsf(a->cor[k] - b->cor[k]) > 3.0f * (float)(a->n + 1)) dif = 1;
            for (int k = 0; k < 3 && !dif; k++) if (fabsf(a->esp[k] - b->esp[k]) > 3.0f * (float)(a->n + 1)) dif = 1;
            if (!dif && (fabsf(a->f - b->f) > 0.02f * (float)(a->n + 1) || fabsf(a->z - b->z) > 0.002f * (float)(a->n + 1))) dif = 1;
            if (!dif && (fabsf(a->tc[0] - b->tc[0]) > 0.02f * (float)(a->n + 1) || fabsf(a->tc[1] - b->tc[1]) > 0.02f * (float)(a->n + 1))) dif = 1;
            mal += dif;
        }
        total_mal += mal; total_px += cheios; cenas++;
        if (cheios > 50) pintadas++;
        /* la coma flotante por software es la misma aritmética: solo cambian los píxeles justo en una arista */
        VERIFICAR(mal <= 6 + cheios / 100, "escena %d (modo %d, indices %d, color %d, niebla %d/%d): %d pixeles distintos de %d", it, modo, com_idx, cor_tipo, s.neblina, s.mezclada, mal, cheios);
    }
    VERIFICAR(pintadas > cenas / 2, "solo %d de %d escenas pintan algo: la prueba no prueba nada", pintadas, cenas);
    printf("  transformacion contra la referencia de ES 1.1: %d escenas (%d pintan), %d pixeles distintos de %d\n", cenas, pintadas, total_mal, total_px);
}

/* ------------------------------------------------------------ 3. límites */

static void prova_limites(void)
{
    static gpu3d_vert2_t lote[24];
    static float vs[9 * 3];
    static uint16_t idx[9];
    gpu3d_transf_t t;
    gpu3d_estado2_t e;
    memset(&t, 0, sizeof t); memset(&e, 0, sizeof e);
    e.versao = 2; t.versao = 3;
    t.mvp[0] = t.mvp[5] = t.mvp[10] = t.mvp[15] = bits(1.0f);
    t.vp_escala[0] = bits(48); t.vp_desloc[0] = bits(48); t.vp_escala[1] = bits(-36); t.vp_desloc[1] = bits(36);
    t.vp_escala[2] = bits(0.5f); t.vp_desloc[2] = bits(0.5f);
    cx0 = 0; cx1 = 96; cy0 = 0; cy1 = 72;
    t.cor = 0xFFFFFFFFu;
    for (int i = 0; i < 9; i++) { vs[i * 3] = (float)(i % 3) * 0.4f - 0.5f; vs[i * 3 + 1] = (float)(i / 3) * 0.4f - 0.5f + (i % 3 == 1 ? 0.3f : 0.0f); vs[i * 3 + 2] = 0; idx[i] = (uint16_t)i; }
    mem_p[0] = (const uint8_t *)vs; mem_n[0] = sizeof vs; mem_p[1] = (const uint8_t *)idx; mem_n[1] = sizeof idx;
    g3d_v3_cpu_t c = { mem, sair, NULL, lote, 24 };
    g3d_desenho3_t g = { &e, &t, NULL, { 0 }, 8, 0 };
    g.d.modo = G3D_PRIM_TRIANGULOS; g.d.n = 9; g.d.indices = GPU3D_SEM_INDICES;
    g.d.pos = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT3, 12, 0 };
    n_saida = saida_mal = 0;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_OK && n_saida == 3 && !saida_mal, "tres triangulos: salen %d", n_saida);
    /* el búfer más corto de lo que el dibujo pide: los triángulos que no se pueden leer no salen */
    mem_n[0] = 12 * 5;
    n_saida = 0;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_OK && n_saida == 1 && !saida_mal, "bufer corto: salen %d", n_saida);
    mem_n[0] = sizeof vs;
    /* índices fuera del búfer de vértices y del de índices */
    g.d.indices = 1; g.bytes_indices = sizeof idx;
    idx[4] = 60000;
    n_saida = 0;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_OK && n_saida == 2 && !saida_mal, "indice fuera: salen %d", n_saida);
    idx[4] = 4;
    g.bytes_indices = 6;
    n_saida = 0;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_OK && n_saida == 1 && !saida_mal, "bufer de indices corto: salen %d", n_saida);
    g.bytes_indices = sizeof idx;
    mem_p[1] = NULL;
    n_saida = 0;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_OK && n_saida == 0, "sin bufer de indices: salen %d", n_saida);
    mem_p[1] = (const uint8_t *)idx; g.d.indices = GPU3D_SEM_INDICES;
    /* basura en los datos: infinitos, NaN, desnormalizados y números enormes */
    for (int it = 0; it < 20000; it++) {
        for (int i = 0; i < 27; i++) {
            uint32_t r = azar() % 8;
            vs[i] = r == 0 ? de_bits(0x7F800000u) : r == 1 ? de_bits(0x7FC00000u) : r == 2 ? de_bits(azar()) : r == 3 ? 1e30f : azar_f(-3, 3);
        }
        for (int k = 0; k < 16; k++) t.mvp[k] = azar() % 5 ? bits(azar_f(-2, 2)) : (azar() << 8) ^ azar();
        for (int k = 0; k < 16; k++) if (((t.mvp[k] >> 23) & 0xFF) == 0xFF) t.mvp[k] = 0;   /* la capa no deja pasar matrices no finitas */
        g.d.modo = (unsigned)(it % 3);
        VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_OK, "basura: no devuelve G3D_OK");
    }
    VERIFICAR(!saida_mal, "con basura en los datos salen %d vertices que la capa rechazaria", saida_mal);
    /* lo que no hace */
    t.flags = G3D3_LUZ;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_NAO, "con luces debe decir G3D_NAO");
    t.flags = G3D3_NEBLINA; t.neb_modo = G3D_NEB_EXP;
    VERIFICAR(g3d_v3_cpu(&g, &c) == G3D_NAO, "con niebla exponencial debe decir G3D_NAO");
    printf("  limites: buferes cortos, indices fuera y basura en los datos, sin leer fuera ni sacar vertices invalidos\n");
}

int main(void)
{
    prova_flutuante();
    prova_tuberia();
    prova_limites();
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("todo bien\n");
    return 0;
}
