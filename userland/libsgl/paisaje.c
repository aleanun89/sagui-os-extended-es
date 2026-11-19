// SPDX-License-Identifier: GPL-2.0-or-later
/* paisaje.c - escena realista y de carga de SaguiGL (D404). Ver paisaje.h.
 *
 * Todo sale de un ruido de valores con enteros (hash) y de sgl_sen/sgl_cos:
 * nada de imágenes de fuera, y el mismo resultado en el anfitrión y en el
 * SaguiOS. El terreno se pinta en tres pasadas sobre los mismos vértices
 * (hierba opaca; roca y arena mezcladas con el alfa del vértice, con
 * GL_LEQUAL), así las transiciones son suaves sin multitextura. */
#include "paisaje.h"

#ifdef SGL_ANFITRIAO
#include <stdlib.h>
#define P_ALOCAR(n)  calloc(1, (n))
#define P_LIBERAR(p) free(p)
#else
#include "sagui.h"
#define P_ALOCAR(n)  aloca_zerado(n)
#define P_LIBERAR(p) libera(p)
#endif

#define MAXG   128            /* celdas del terreno por lado, como mucho */
#define MUNDO  64.0f          /* lado del terreno */
#define AGUA   0.9f           /* altura del agua */
#define AG     24             /* celdas del agua por lado */
#define MAX_CARGA (PAIS_MAX_MB * 4)

static float raizf(float x) { return (float)__builtin_sqrt((double)x); }

/* ------------------------------------------------------------ ruido */

static uint32_t hash2(int x, int y, uint32_t s)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static int pisof(float v) { int i = (int)v; return v < (float)i ? i - 1 : i; }

/* Ruido de valores en [0, 1]; con periodo p > 0 (potencia de 2) se repite (texturas). */
static float ruido(float x, float y, int p, uint32_t s)
{
    int xi = pisof(x), yi = pisof(y);
    float fx = x - (float)xi, fy = y - (float)yi;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    int x1 = xi + 1, y1 = yi + 1;
    if (p > 0) { xi &= p - 1; yi &= p - 1; x1 &= p - 1; y1 &= p - 1; }
    float a = (float)(hash2(xi, yi, s) & 0xFFFF) * (1.0f / 65535.0f);
    float b = (float)(hash2(x1, yi, s) & 0xFFFF) * (1.0f / 65535.0f);
    float c = (float)(hash2(xi, y1, s) & 0xFFFF) * (1.0f / 65535.0f);
    float d = (float)(hash2(x1, y1, s) & 0xFFFF) * (1.0f / 65535.0f);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static float fbm(float x, float y, int oct, int p, uint32_t s)
{
    float soma = 0, amp = 1, tot = 0;
    for (int o = 0; o < oct; o++) {
        soma += amp * ruido(x, y, p, s + (uint32_t)o * 101u);
        tot += amp; amp *= 0.5f; x *= 2.0f; y *= 2.0f;
        if (p > 0) p *= 2;
    }
    return soma / tot;
}

static float sat(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static unsigned char b8(float v) { return (unsigned char)(v <= 0.0f ? 0 : v >= 255.0f ? 255 : (int)(v + 0.5f)); }

/* La altura del terreno en (x, z). */
static float altura(float x, float z)
{
    float h = 8.0f * fbm(x * 0.04f + 3.1f, z * 0.04f + 7.7f, 5, 0, 11u) - 3.2f;
    float d = (x * x + z * z) * (1.0f / (32.0f * 32.0f));
    return h + 7.0f * d * d;
}

static void normal_terreno(float x, float z, float *n)
{
    const float e = 0.25f;
    float nx = altura(x - e, z) - altura(x + e, z), nz = altura(x, z - e) - altura(x, z + e), ny = 2.0f * e;
    float l = raizf(nx * nx + ny * ny + nz * nz);
    n[0] = nx / l; n[1] = ny / l; n[2] = nz / l;
}

/* ------------------------------------------------------------ texturas */

enum { T_HIERBA, T_ROCA, T_ARENA, T_AGUA, T_NUBES, T_MADERA, T_TEJADO, T_HOJAS, N_TEX };
static GLuint tex_id[N_TEX];
static GLuint carga_id[MAX_CARGA];
static int n_carga;
static int usar_mip;
static unsigned char buf_tex[256 * 256 * 4];

static void subir(int k, int lado, GLenum formato, GLenum tipo, const void *px)
{
    glBindTexture(GL_TEXTURE_2D, tex_id[k]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, usar_mip ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    if (usar_mip) glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)formato, lado, lado, 0, formato, tipo, px);
}

static void criar_texturas(void)
{
    unsigned char *b = buf_tex;
    uint16_t *b16 = (uint16_t *)buf_tex;
    glGenTextures(N_TEX, tex_id);
    /* hierba: 256, RGB */
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float n1 = fbm(x / 32.0f, y / 32.0f, 4, 8, 21u), n2 = ruido(x / 2.0f, y / 2.0f, 128, 22u);
            unsigned char *q = b + (y * 256 + x) * 3;
            q[0] = b8(40 + 70 * n1 + 25 * n2); q[1] = b8(80 + 80 * n1 + 35 * n2); q[2] = b8(22 + 30 * n1);
        }
    subir(T_HIERBA, 256, GL_RGB, GL_UNSIGNED_BYTE, b);
    /* roca: 256, luminancia (el color lo pone el vértice) */
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float n1 = fbm(x / 64.0f, y / 64.0f, 5, 4, 31u), g = ruido(x / 24.0f, y / 24.0f, 0, 32u);
            float l = 70 + 130 * n1 + 20 * ruido(x / 1.5f, y / 1.5f, 0, 33u);
            float r = ruido(x / 24.0f, y / 24.0f, 0, 32u) - 0.5f;
            if (r > -0.02f && r < 0.02f) l *= 0.55f;   /* grietas */
            (void)g;
            b[y * 256 + x] = b8(l);
        }
    subir(T_ROCA, 256, GL_LUMINANCE, GL_UNSIGNED_BYTE, b);
    /* arena: 128, RGB565 */
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float n = ruido(x / 1.7f, y / 1.7f, 0, 41u) * 0.6f + fbm(x / 16.0f, y / 16.0f, 3, 8, 42u) * 0.4f;
            unsigned r = b8(170 + 60 * n), g = b8(150 + 55 * n), bl = b8(100 + 40 * n);
            b16[y * 128 + x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (bl >> 3));
        }
    subir(T_ARENA, 128, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, b16);
    /* agua: 128, RGB */
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float n = fbm(x / 16.0f, y / 32.0f, 4, 8, 51u);
            unsigned char *q = b + (y * 128 + x) * 3;
            q[0] = b8(30 + 60 * n); q[1] = b8(80 + 70 * n); q[2] = b8(120 + 80 * n);
        }
    subir(T_AGUA, 128, GL_RGB, GL_UNSIGNED_BYTE, b);
    /* nubes: 256, luminancia y alfa */
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float n = fbm(x / 64.0f, y / 64.0f, 5, 4, 61u);
            b[(y * 256 + x) * 2] = b8(255 - 50 * n);
            b[(y * 256 + x) * 2 + 1] = b8(sat((n - 0.45f) * 3.0f) * 230.0f);
        }
    subir(T_NUBES, 256, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, b);
    /* madera: 128, RGB (tablas horizontales) */
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float veta = ruido(x / 12.0f, y * 1.5f, 0, 71u);
            float k = (y % 16) < 1 ? 0.45f : 0.8f + 0.25f * veta;
            unsigned char *q = b + (y * 128 + x) * 3;
            q[0] = b8(150 * k); q[1] = b8(100 * k); q[2] = b8(58 * k);
        }
    subir(T_MADERA, 128, GL_RGB, GL_UNSIGNED_BYTE, b);
    /* tejado: 128, RGBA4444 (tejas) */
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            int fila = y / 16, col = (x + (fila & 1) * 8) % 16;
            float k = (y % 16) > 12 || col == 0 ? 0.5f : 0.85f + 0.2f * ruido(x / 3.0f, y / 3.0f, 0, 81u);
            unsigned r = b8(170 * k), g = b8(70 * k), bl = b8(45 * k);
            b16[y * 128 + x] = (uint16_t)(((r >> 4) << 12) | ((g >> 4) << 8) | ((bl >> 4) << 4) | 15u);
        }
    subir(T_TEJADO, 128, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, b16);
    /* hojas: 128, RGBA */
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float n = fbm(x / 8.0f, y / 8.0f, 3, 16, 91u);
            unsigned char *q = b + (y * 128 + x) * 4;
            q[0] = b8(25 + 60 * n); q[1] = b8(60 + 100 * n); q[2] = b8(20 + 35 * n); q[3] = 255;
        }
    subir(T_HOJAS, 128, GL_RGBA, GL_UNSIGNED_BYTE, b);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* --texturas: N texturas de 256x256 distintas (cuadros, rayas y color por índice). */
static void criar_carga(int mb)
{
    n_carga = mb * 4;
    if (n_carga > MAX_CARGA) n_carga = MAX_CARGA;
    if (n_carga <= 0) { n_carga = 0; return; }
    glGenTextures(n_carga, carga_id);
    for (int i = 0; i < n_carga; i++) {
        uint32_t h = hash2(i, 7, 99u);
        int r0 = (int)(h & 255), g0 = (int)((h >> 8) & 255), b0 = (int)((h >> 16) & 255);
        int pas = 4 << (i % 4), rot = (int)((h >> 24) & 7);
        for (int y = 0; y < 256; y++)
            for (int x = 0; x < 256; x++) {
                int k = (((x + rot * y) / pas) ^ (y / pas)) & 1;
                unsigned char *q = buf_tex + (y * 256 + x) * 4;
                q[0] = (unsigned char)(k ? r0 : 255 - r0); q[1] = (unsigned char)(k ? g0 : (x + i) & 255);
                q[2] = (unsigned char)(k ? b0 : y); q[3] = 255;
            }
        glBindTexture(GL_TEXTURE_2D, carga_id[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, usar_mip ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
        if (usar_mip) glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf_tex);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* ------------------------------------------------------------ terreno y agua */

static int G;                                     /* celdas por lado */
static float t_pos[(MAXG + 1) * (MAXG + 1) * 3], t_nor[(MAXG + 1) * (MAXG + 1) * 3], t_uv[(MAXG + 1) * (MAXG + 1) * 2];
static unsigned char c_base[(MAXG + 1) * (MAXG + 1) * 4], c_roca[(MAXG + 1) * (MAXG + 1) * 4], c_arena[(MAXG + 1) * (MAXG + 1) * 4];
static uint16_t i_base[MAXG * MAXG * 6], i_roca[MAXG * MAXG * 6], i_arena[MAXG * MAXG * 6];
static int n_base, n_roca, n_arena;
static float a_pos[(AG + 1) * (AG + 1) * 3], a_nor[(AG + 1) * (AG + 1) * 3], a_uv[(AG + 1) * (AG + 1) * 2];
static uint16_t a_idx[AG * AG * 6];

static void criar_terreno(void)
{
    int nl = G + 1;
    float cel = MUNDO / (float)G;
    for (int j = 0; j <= G; j++)
        for (int i = 0; i <= G; i++) {
            int k = j * nl + i;
            float x = -MUNDO * 0.5f + (float)i * cel, z = -MUNDO * 0.5f + (float)j * cel, h = altura(x, z);
            t_pos[k * 3] = x; t_pos[k * 3 + 1] = h; t_pos[k * 3 + 2] = z;
            normal_terreno(x, z, &t_nor[k * 3]);
            t_uv[k * 2] = x * 0.25f; t_uv[k * 2 + 1] = z * 0.25f;
            float tinte = 0.8f + 0.35f * ruido(x * 0.25f, z * 0.25f, 0, 5u);
            c_base[k * 4] = b8(235 * tinte); c_base[k * 4 + 1] = b8(240 * tinte); c_base[k * 4 + 2] = b8(220 * tinte); c_base[k * 4 + 3] = 255;
            float ar = sat((1.0f - t_nor[k * 3 + 1] - 0.16f) * 5.0f), alt = sat((h - 5.5f) * 0.5f);
            if (alt > ar) ar = alt;
            c_roca[k * 4] = 190; c_roca[k * 4 + 1] = 180; c_roca[k * 4 + 2] = 168; c_roca[k * 4 + 3] = b8(ar * 255);
            float aa = sat((AGUA + 0.7f - h) * 1.6f);
            c_arena[k * 4] = 255; c_arena[k * 4 + 1] = 250; c_arena[k * 4 + 2] = 240; c_arena[k * 4 + 3] = b8(aa * 255);
        }
    n_base = n_roca = n_arena = 0;
    for (int j = 0; j < G; j++)
        for (int i = 0; i < G; i++) {
            uint16_t a = (uint16_t)(j * nl + i), b = (uint16_t)(a + 1), d = (uint16_t)(a + nl), c = (uint16_t)(d + 1);
            uint16_t tri[6] = { a, d, c, a, c, b };
            for (int t = 0; t < 2; t++) {
                const uint16_t *v = &tri[t * 3];
                for (int q = 0; q < 3; q++) i_base[n_base + q] = v[q];
                n_base += 3;
                if (c_roca[v[0] * 4 + 3] || c_roca[v[1] * 4 + 3] || c_roca[v[2] * 4 + 3]) {
                    for (int q = 0; q < 3; q++) i_roca[n_roca + q] = v[q];
                    n_roca += 3;
                }
                if (c_arena[v[0] * 4 + 3] || c_arena[v[1] * 4 + 3] || c_arena[v[2] * 4 + 3]) {
                    for (int q = 0; q < 3; q++) i_arena[n_arena + q] = v[q];
                    n_arena += 3;
                }
            }
        }
    int na = 0;
    for (int j = 0; j <= AG; j++)
        for (int i = 0; i <= AG; i++) {
            int k = j * (AG + 1) + i;
            a_pos[k * 3] = -MUNDO * 0.5f + MUNDO * (float)i / AG; a_pos[k * 3 + 1] = AGUA;
            a_pos[k * 3 + 2] = -MUNDO * 0.5f + MUNDO * (float)j / AG;
        }
    for (int j = 0; j < AG; j++)
        for (int i = 0; i < AG; i++) {
            uint16_t a = (uint16_t)(j * (AG + 1) + i), b = (uint16_t)(a + 1), d = (uint16_t)(a + AG + 1), c = (uint16_t)(d + 1);
            a_idx[na++] = a; a_idx[na++] = d; a_idx[na++] = c;
            a_idx[na++] = a; a_idx[na++] = c; a_idx[na++] = b;
        }
}

/* Olas: normales y coordenadas que se mueven con el cuadro. */
static void mover_agua(int quadro)
{
    float t = (float)quadro * 0.08f;
    for (int k = 0; k < (AG + 1) * (AG + 1); k++) {
        float x = a_pos[k * 3], z = a_pos[k * 3 + 2];
        float dx = 0.10f * sgl_cos(x * 0.9f + t) + 0.06f * sgl_cos(x * 0.5f + z * 0.7f + t * 0.6f);
        float dz = 0.08f * sgl_cos(z * 1.3f + t * 0.7f) + 0.06f * sgl_cos(x * 0.5f + z * 0.7f + t * 0.6f);
        float l = raizf(dx * dx + 1.0f + dz * dz);
        a_nor[k * 3] = -dx / l; a_nor[k * 3 + 1] = 1.0f / l; a_nor[k * 3 + 2] = -dz / l;
        a_uv[k * 2] = x * 0.15f + t * 0.02f; a_uv[k * 2 + 1] = z * 0.15f + t * 0.01f;
    }
}

/* ------------------------------------------------------------ objetos */

typedef struct { float *p, *n, *t; int n_vert, cap; } malla_t;

#define CAP_TRONCO 2400
#define CAP_COPA   3600
#define CAP_ROCA   1800
#define CAP_PARED  400
#define CAP_TEJADO 200
#define CAP_AVIAO  600
static float m_buf[(CAP_TRONCO + CAP_COPA + CAP_ROCA + CAP_PARED + CAP_TEJADO + CAP_AVIAO * 2) * 8];
static malla_t troncos, copas, rocas, paredes, tejados, aviao_corpo, aviao_asas;
static float *m_livre;

static void malla_nova(malla_t *m, int cap)
{
    m->p = m_livre; m->n = m->p + cap * 3; m->t = m->n + cap * 3; m_livre = m->t + cap * 2;
    m->cap = cap; m->n_vert = 0;
}

static void vertice(malla_t *m, float x, float y, float z, float nx, float ny, float nz, float u, float v)
{
    if (m->n_vert >= m->cap) return;
    int k = m->n_vert++;
    m->p[k * 3] = x; m->p[k * 3 + 1] = y; m->p[k * 3 + 2] = z;
    m->n[k * 3] = nx; m->n[k * 3 + 1] = ny; m->n[k * 3 + 2] = nz;
    m->t[k * 2] = u; m->t[k * 2 + 1] = v;
}

/* Triángulo con la normal de su cara. */
static void tri_plano(malla_t *m, const float *a, const float *b, const float *c, float s)
{
    float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx, l = raizf(nx * nx + ny * ny + nz * nz);
    if (l > 0) { nx /= l; ny /= l; nz /= l; }
    vertice(m, a[0], a[1], a[2], nx, ny, nz, (a[0] + a[2]) * s, a[1] * s);
    vertice(m, b[0], b[1], b[2], nx, ny, nz, (b[0] + b[2]) * s, b[1] * s);
    vertice(m, c[0], c[1], c[2], nx, ny, nz, (c[0] + c[2]) * s, c[1] * s);
}

static void arvore(float x, float z, float esc)
{
    float y = altura(x, z) - 0.05f;
    const int L = 6;
    float r = 0.12f * esc, h = 0.9f * esc;
    for (int k = 0; k < L; k++) {
        float a0 = 6.2831853f * (float)k / L, a1 = 6.2831853f * (float)(k + 1) / L;
        float c0 = sgl_cos(a0), s0 = sgl_sen(a0), c1 = sgl_cos(a1), s1 = sgl_sen(a1);
        float u0 = (float)k / L, u1 = (float)(k + 1) / L;
        vertice(&troncos, x + r * c0, y, z + r * s0, c0, 0, s0, u0, 0);
        vertice(&troncos, x + r * c1, y + h, z + r * s1, c1, 0, s1, u1, h);
        vertice(&troncos, x + r * c1, y, z + r * s1, c1, 0, s1, u1, 0);
        vertice(&troncos, x + r * c0, y, z + r * s0, c0, 0, s0, u0, 0);
        vertice(&troncos, x + r * c0, y + h, z + r * s0, c0, 0, s0, u0, h);
        vertice(&troncos, x + r * c1, y + h, z + r * s1, c1, 0, s1, u1, h);
    }
    /* dos conos */
    for (int c = 0; c < 2; c++) {
        float base = y + (c ? 1.3f : 0.6f) * esc, topo = y + (c ? 2.7f : 2.0f) * esc, rb = (c ? 0.5f : 0.75f) * esc;
        const int S = 8;
        float inc = rb / (topo - base);
        for (int k = 0; k < S; k++) {
            float a0 = 6.2831853f * (float)k / S, a1 = 6.2831853f * (float)(k + 1) / S, am = (a0 + a1) * 0.5f;
            float c0 = sgl_cos(a0), s0 = sgl_sen(a0), c1 = sgl_cos(a1), s1 = sgl_sen(a1);
            float l = raizf(1.0f + inc * inc);
            float n0x = c0 / l, n0z = s0 / l, n1x = c1 / l, n1z = s1 / l, ny = inc / l;
            vertice(&copas, x + rb * c0, base, z + rb * s0, n0x, ny, n0z, a0, 0);
            vertice(&copas, x, topo, z, sgl_cos(am) / l, ny, sgl_sen(am) / l, am, 2.0f);
            vertice(&copas, x + rb * c1, base, z + rb * s1, n1x, ny, n1z, a1, 0);
        }
    }
}

static void pedra(float x, float z, float tam, uint32_t s)
{
    /* octaedro dividido una vez, con el radio de cada vértice movido */
    static const float o[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    static const int f[8][3] = { { 0, 2, 4 }, { 4, 2, 1 }, { 1, 2, 5 }, { 5, 2, 0 }, { 4, 3, 0 }, { 1, 3, 4 }, { 5, 3, 1 }, { 0, 3, 5 } };
    float y = altura(x, z) + tam * 0.25f;
    for (int i = 0; i < 8; i++) {
        float v[6][3];
        for (int k = 0; k < 3; k++) for (int q = 0; q < 3; q++) v[k][q] = o[f[i][k]][q];
        for (int k = 0; k < 3; k++)
            for (int q = 0; q < 3; q++) v[3 + k][q] = (o[f[i][k]][q] + o[f[i][(k + 1) % 3]][q]) * 0.5f;
        float w[6][3];
        for (int k = 0; k < 6; k++) {
            float l = raizf(v[k][0] * v[k][0] + v[k][1] * v[k][1] + v[k][2] * v[k][2]);
            int hx = (int)(v[k][0] * 4), hy = (int)(v[k][1] * 4), hz = (int)(v[k][2] * 4);
            float j = 0.75f + 0.5f * (float)(hash2(hx * 7 + hz, hy, s) & 255) / 255.0f;
            w[k][0] = x + v[k][0] / l * tam * j; w[k][1] = y + v[k][1] / l * tam * j * 0.6f; w[k][2] = z + v[k][2] / l * tam * j;
        }
        tri_plano(&rocas, w[0], w[3], w[5], 0.7f);
        tri_plano(&rocas, w[3], w[1], w[4], 0.7f);
        tri_plano(&rocas, w[5], w[4], w[2], 0.7f);
        tri_plano(&rocas, w[3], w[4], w[5], 0.7f);
    }
}

/* Punto girado `a` sobre el eje Y alrededor de (x, z). */
static void girar(float *p, float x, float y, float z, float dx, float dy, float dz, float c, float s)
{
    p[0] = x + dx * c - dz * s; p[1] = y + dy; p[2] = z + dx * s + dz * c;
}

static void casa(float x, float z, float ang)
{
    float c = sgl_cos(ang), s = sgl_sen(ang), y = altura(x, z) - 0.2f;
    const float W = 1.1f, D = 0.85f, H = 1.4f, T = 2.3f;
    float q[8][3];
    for (int i = 0; i < 8; i++)
        girar(q[i], x, y, z, (i & 1) ? W : -W, (i & 4) ? H : 0.0f, (i & 2) ? D : -D, c, s);
    /* paredes (sentido antihorario visto desde fuera) */
    static const int pa[4][4] = { { 0, 1, 5, 4 }, { 1, 3, 7, 5 }, { 3, 2, 6, 7 }, { 2, 0, 4, 6 } };
    for (int k = 0; k < 4; k++) {
        tri_plano(&paredes, q[pa[k][0]], q[pa[k][1]], q[pa[k][2]], 0.9f);
        tri_plano(&paredes, q[pa[k][0]], q[pa[k][2]], q[pa[k][3]], 0.9f);
    }
    float cu[2][3];
    girar(cu[0], x, y, z, -W - 0.15f, T, 0, c, s);
    girar(cu[1], x, y, z, W + 0.15f, T, 0, c, s);
    float e[4][3];   /* aleros */
    girar(e[0], x, y, z, -W - 0.15f, H - 0.1f, -D - 0.3f, c, s);
    girar(e[1], x, y, z, W + 0.15f, H - 0.1f, -D - 0.3f, c, s);
    girar(e[2], x, y, z, -W - 0.15f, H - 0.1f, D + 0.3f, c, s);
    girar(e[3], x, y, z, W + 0.15f, H - 0.1f, D + 0.3f, c, s);
    tri_plano(&tejados, e[0], cu[0], cu[1], 1.0f);
    tri_plano(&tejados, e[0], cu[1], e[1], 1.0f);
    tri_plano(&tejados, e[3], cu[1], cu[0], 1.0f);
    tri_plano(&tejados, e[3], cu[0], e[2], 1.0f);
    float g[2][3];   /* hastiales */
    girar(g[0], x, y, z, -W, T - 0.1f, 0, c, s);
    girar(g[1], x, y, z, W, T - 0.1f, 0, c, s);
    tri_plano(&paredes, q[4], q[6], g[0], 0.9f);
    tri_plano(&paredes, q[7], q[5], g[1], 0.9f);
}

/* Una caja (avión hecho por código): centro y medios lados; el morro hacia +x. */
static void caixa(malla_t *m, float cx, float cy, float cz, float hx, float hy, float hz)
{
    float q[8][3];
    for (int i = 0; i < 8; i++) {
        q[i][0] = cx + ((i & 1) ? hx : -hx); q[i][1] = cy + ((i & 4) ? hy : -hy); q[i][2] = cz + ((i & 2) ? hz : -hz);
    }
    static const int fa[6][4] = { { 0, 1, 5, 4 }, { 1, 3, 7, 5 }, { 3, 2, 6, 7 }, { 2, 0, 4, 6 }, { 4, 5, 7, 6 }, { 0, 2, 3, 1 } };
    for (int k = 0; k < 6; k++) {
        tri_plano(m, q[fa[k][0]], q[fa[k][1]], q[fa[k][2]], 1.0f);
        tri_plano(m, q[fa[k][0]], q[fa[k][2]], q[fa[k][3]], 1.0f);
    }
}

static void criar_aviao(void)
{
    caixa(&aviao_corpo, 0.0f, 0.0f, 0.0f, 1.2f, 0.17f, 0.17f);     /* fuselaje */
    caixa(&aviao_corpo, 1.25f, 0.0f, 0.0f, 0.08f, 0.12f, 0.12f);   /* morro */
    caixa(&aviao_corpo, -1.05f, 0.32f, 0.0f, 0.22f, 0.18f, 0.03f); /* deriva */
    caixa(&aviao_corpo, 1.36f, 0.0f, 0.0f, 0.02f, 0.45f, 0.05f);   /* hélice */
    caixa(&aviao_asas, 0.3f, -0.05f, 0.0f, 0.32f, 0.03f, 1.7f);    /* ala baja */
    caixa(&aviao_asas, 0.35f, 0.45f, 0.0f, 0.32f, 0.03f, 1.6f);    /* ala alta */
    caixa(&aviao_asas, -1.0f, 0.05f, 0.0f, 0.18f, 0.02f, 0.55f);   /* timón de profundidad */
    caixa(&aviao_asas, 0.3f, 0.2f, 0.9f, 0.03f, 0.22f, 0.03f);     /* montantes */
    caixa(&aviao_asas, 0.3f, 0.2f, -0.9f, 0.03f, 0.22f, 0.03f);
}

/* ------------------------------------------------------------ estado */

static const sgl_obj_t *modelo;
static float giro_modelo;
static int camara, pronto;

int paisaje_texturas_carga(void) { return n_carga; }

int paisaje_iniciar(const pais_opcoes_t *o)
{
    usar_mip = o->mip;
    G = o->detalle > 0 ? o->detalle : 64;
    if (G < 16) G = 16;
    if (G > MAXG) G = MAXG;
    modelo = o->aviao;
    giro_modelo = o->giro_aviao;
    camara = o->camara;
    criar_texturas();
    criar_carga(o->texturas_mb);
    criar_terreno();
    m_livre = m_buf;
    malla_nova(&troncos, CAP_TRONCO); malla_nova(&copas, CAP_COPA); malla_nova(&rocas, CAP_ROCA);
    malla_nova(&paredes, CAP_PARED); malla_nova(&tejados, CAP_TEJADO);
    malla_nova(&aviao_corpo, CAP_AVIAO); malla_nova(&aviao_asas, CAP_AVIAO);
    int na = 0, nr = 0, nc = 0;
    for (int i = 0; i < 4000 && (na < 50 || nr < 18 || nc < 5); i++) {
        uint32_t h = hash2(i, 3, 777u);
        float x = -28.0f + 56.0f * (float)(h & 0xFFF) / 4095.0f, z = -28.0f + 56.0f * (float)((h >> 12) & 0xFFF) / 4095.0f;
        float y = altura(x, z), n[3];
        normal_terreno(x, z, n);
        int tipo = (int)((h >> 24) % 10);
        if (tipo < 7 && na < 50 && y > AGUA + 0.6f && y < 5.5f && n[1] > 0.85f) { arvore(x, z, 0.8f + 0.5f * (float)((h >> 28) & 7) / 7.0f); na++; }
        else if (tipo < 9 && nr < 18 && y > AGUA + 0.1f) { pedra(x, z, 0.35f + 0.5f * (float)((h >> 28) & 7) / 7.0f, h); nr++; }
        else if (tipo == 9 && nc < 5 && y > AGUA + 0.5f && y < 4.5f && n[1] > 0.95f) { casa(x, z, (float)(h & 63) * 0.1f); nc++; }
    }
    criar_aviao();
    pronto = 1;
    return 0;
}

void paisaje_fim(void)
{
    if (!pronto) return;
    glDeleteTextures(N_TEX, tex_id);
    if (n_carga) glDeleteTextures(n_carga, carga_id);
    n_carga = 0;
    pronto = 0;
}

/* ------------------------------------------------------------ cuadro */

static void olhar(float ex, float ey, float ez, float cx, float cy, float cz)
{
    float fx = cx - ex, fy = cy - ey, fz = cz - ez, l = raizf(fx * fx + fy * fy + fz * fz);
    fx /= l; fy /= l; fz /= l;
    float sx = -fz, sy = 0, sz = fx;   /* f x (0,1,0) */
    l = raizf(sx * sx + sz * sz); sx /= l; sz /= l;
    float ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
    float m[16] = { sx, ux, -fx, 0, sy, uy, -fy, 0, sz, uz, -fz, 0, 0, 0, 0, 1 };
    glMultMatrixf(m);
    glTranslatef(-ex, -ey, -ez);
}

static void desenhar_malla(const malla_t *m, int tex)
{
    if (!m->n_vert) return;
    if (tex >= 0) {
        glBindTexture(GL_TEXTURE_2D, tex_id[tex]);
        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, m->t);
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, m->p);
    glNormalPointer(GL_FLOAT, 0, m->n);
    glDrawArrays(GL_TRIANGLES, 0, m->n_vert);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (tex >= 0) { glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisable(GL_TEXTURE_2D); }
}

static void material(float r, float g, float b, float a, float esp, float brilho)
{
    float d[4] = { r, g, b, a }, e[4] = { esp, esp, esp, 1 };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, d);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, e);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, brilho);
}

static void passada_terreno(int tex, const unsigned char *cor, const uint16_t *idx, int n)
{
    glBindTexture(GL_TEXTURE_2D, tex_id[tex]);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, cor);
    glDrawElements(GL_TRIANGLES, n, GL_UNSIGNED_SHORT, idx);
}

void paisaje_cuadro(int i, int l, int a)
{
    static const float horiz[4] = { 0.74f, 0.80f, 0.86f, 1 };
    float asp = (float)l / (float)a;
    glViewport(0, 0, l, a);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glClearColor(horiz[0], horiz[1], horiz[2], 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* cielo: degradado en la pantalla */
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrthof(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_CULL_FACE);
    glBegin(GL_TRIANGLE_FAN);
    glColor3f(horiz[0], horiz[1], horiz[2]); glVertex2f(0, 0.2f); glVertex2f(1, 0.2f);
    glColor3f(0.28f, 0.48f, 0.82f); glVertex2f(1, 1); glVertex2f(0, 1);
    glEnd();

    /* el avión da vueltas por encima del valle */
    float aa = (float)i * 0.03f;
    float avx = 3.0f + 13.0f * sgl_cos(aa), avy = 9.5f + 0.8f * sgl_sen(aa * 2.0f), avz = -2.0f + 13.0f * sgl_sen(aa);
    /* cámara: da vueltas al valle a ras del suelo, o va detrás del avión */
    float ang = 0.3f + (float)i * 0.012f;
    float ex = 19.0f * sgl_cos(ang), ez = 19.0f * sgl_sen(ang);
    float hy = altura(ex, ez);
    if (hy < AGUA) hy = AGUA;
    float ey = hy + 3.2f;
    float cx = 15.0f * sgl_cos(ang + 0.45f), cz = 15.0f * sgl_sen(ang + 0.45f), cy = ey - 1.6f;
    if (camara == 1) {
        float tx = -sgl_sen(aa), tz = sgl_cos(aa);   /* hacia donde vuela */
        ex = avx - 6.0f * tx + 2.0f * tz; ez = avz - 6.0f * tz - 2.0f * tx; ey = avy + 2.2f;
        cx = avx + 2.0f * tx; cy = avy - 0.6f; cz = avz + 2.0f * tz;
    }
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustumf(-0.18f * asp, 0.18f * asp, -0.18f, 0.18f, 0.25f, 90.0f);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    olhar(ex, ey, ez, cx, cy, cz);

    /* luces: el sol y el cielo, fijas en el mundo */
    static const float sol[4] = { 0.45f, 0.75f, -0.35f, 0 }, sol_c[4] = { 1.0f, 0.93f, 0.80f, 1 };
    static const float ceu[4] = { -0.4f, 0.6f, 0.5f, 0 }, ceu_c[4] = { 0.22f, 0.28f, 0.40f, 1 }, nada[4] = { 0, 0, 0, 1 };
    static const float amb[4] = { 0.30f, 0.32f, 0.36f, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, sol); glLightfv(GL_LIGHT0, GL_DIFFUSE, sol_c); glLightfv(GL_LIGHT0, GL_SPECULAR, sol_c);
    glLightfv(GL_LIGHT1, GL_POSITION, ceu); glLightfv(GL_LIGHT1, GL_DIFFUSE, ceu_c); glLightfv(GL_LIGHT1, GL_SPECULAR, nada);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1);
    glEnable(GL_FOG);
    glFogf(GL_FOG_MODE, (float)GL_EXP2); glFogf(GL_FOG_DENSITY, 0.024f); glFogfv(GL_FOG_COLOR, horiz);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);

    /* terreno: hierba, y encima roca y arena con el alfa de cada vértice */
    glEnable(GL_COLOR_MATERIAL);
    material(1, 1, 1, 1, 0.05f, 8);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, t_pos); glNormalPointer(GL_FLOAT, 0, t_nor); glTexCoordPointer(2, GL_FLOAT, 0, t_uv);
    passada_terreno(T_HIERBA, c_base, i_base, n_base);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    passada_terreno(T_ROCA, c_roca, i_roca, n_roca);
    passada_terreno(T_ARENA, c_arena, i_arena, n_arena);
    glDisable(GL_BLEND);
    glDisableClientState(GL_COLOR_ARRAY); glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_COLOR_MATERIAL);

    /* objetos */
    material(0.85f, 0.75f, 0.65f, 1, 0.05f, 8); desenhar_malla(&troncos, T_MADERA);
    material(0.95f, 1.0f, 0.9f, 1, 0.10f, 12); desenhar_malla(&copas, T_HOJAS);
    material(0.75f, 0.72f, 0.68f, 1, 0.15f, 20); desenhar_malla(&rocas, T_ROCA);
    material(1.0f, 0.95f, 0.88f, 1, 0.05f, 8); desenhar_malla(&paredes, T_MADERA);
    material(1.0f, 1.0f, 1.0f, 1, 0.30f, 30); desenhar_malla(&tejados, T_TEJADO);

    /* el avión: da vueltas por encima, inclinado en el giro */
    glPushMatrix();
    glTranslatef(avx, avy, avz);
    glRotatef(-aa * 57.29578f - 90.0f, 0, 1, 0);
    glRotatef(25.0f, 1, 0, 0);
    if (modelo && modelo->n_vert) { glRotatef(giro_modelo, 0, 1, 0); sgl_obj_desenhar(modelo, 3.0f); }
    else {
        material(0.80f, 0.12f, 0.10f, 1, 0.6f, 40); desenhar_malla(&aviao_corpo, -1);
        material(0.92f, 0.88f, 0.75f, 1, 0.4f, 30); desenhar_malla(&aviao_asas, -1);
    }
    glPopMatrix();

    /* agua: translúcida, con el brillo del sol en las olas */
    mover_agua(i);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    material(0.35f, 0.55f, 0.75f, 0.72f, 0.9f, 60);
    glBindTexture(GL_TEXTURE_2D, tex_id[T_AGUA]); glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, a_pos); glNormalPointer(GL_FLOAT, 0, a_nor); glTexCoordPointer(2, GL_FLOAT, 0, a_uv);
    glDrawElements(GL_TRIANGLES, AG * AG * 6, GL_UNSIGNED_SHORT, a_idx);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_LIGHTING);

    /* --texturas: carteles en corro, cada uno con su textura */
    for (int k = 0; k < n_carga; k++) {
        float ak = 6.2831853f * (float)(k % 48) / 48.0f + ang * 0.2f, rk = 24.0f;
        float px = rk * sgl_cos(ak), pz = rk * sgl_sen(ak), py = 6.0f + 1.8f * (float)(k / 48);
        float tx = -sgl_sen(ak) * 0.8f, tz = sgl_cos(ak) * 0.8f;
        glBindTexture(GL_TEXTURE_2D, carga_id[k]);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 1); glVertex3f(px - tx, py - 0.8f, pz - tz);
        glTexCoord2f(1, 1); glVertex3f(px + tx, py - 0.8f, pz + tz);
        glTexCoord2f(1, 0); glVertex3f(px + tx, py + 0.8f, pz + tz);
        glTexCoord2f(0, 0); glVertex3f(px - tx, py + 0.8f, pz - tz);
        glEnd();
    }

    /* nubes: un plano alto que se desplaza, con su alfa */
    glBindTexture(GL_TEXTURE_2D, tex_id[T_NUBES]);
    glColor4f(1, 1, 1, 1);
    float d = (float)i * 0.002f;
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(d, 0); glVertex3f(-80, 24, -80);
    glTexCoord2f(d, 4); glVertex3f(-80, 24, 80);
    glTexCoord2f(4 + d, 4); glVertex3f(80, 24, 80);
    glTexCoord2f(4 + d, 0); glVertex3f(80, 24, -80);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}
