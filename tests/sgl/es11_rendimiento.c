// SPDX-License-Identifier: GPL-2.0-or-later
/* es11_rendimiento.c - medida del rasterizador por software de SaguiGL en el
 * anfitrión (D425): el camino de siempre (una textura con MODULATE, sin
 * esténcil ni prueba alfa) y el color plano, con el sgl.c que se compile.
 * Solo usa la API de antes de la D425, para poder compilarlo también con el
 * sgl.c de antes y comparar (tests/h424_sgl_es11.py «rendimiento»).
 *
 *   es11_rendimiento [cuadros]   ->  «textura <ms> plano <ms> suma <hex>» */
#include "sgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define L 640
#define A 480

static uint32_t px[L * A];
static unsigned char tex[64 * 64 * 4];

static double agora_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1.0e6;
}

/* Un cubo de 12 triángulos que llena buena parte de la ventana. */
static void cubo(float ang)
{
    static const float v[8][3] = { {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1} };
    static const int f[6][4] = { {4,5,6,7},{1,0,3,2},{0,4,7,3},{5,1,2,6},{7,6,2,3},{0,1,5,4} };
    glLoadIdentity();
    glTranslatef(0, 0, -3.2f);
    glRotatef(ang, 1, 1, 0);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 6; i++) {
        const float uv[4][2] = { {0,0},{2,0},{2,2},{0,2} };
        int t[6] = { 0, 1, 2, 0, 2, 3 };
        for (int k = 0; k < 6; k++) {
            const float *p = v[f[i][t[k]]];
            glColor4f(0.5f + 0.08f * (float)i, 0.9f - 0.1f * (float)i, 0.7f, 1.0f);
            glTexCoord2f(uv[t[k]][0], uv[t[k]][1]);
            glVertex3f(p[0], p[1], p[2]);
        }
    }
    glEnd();
}

static double medir(int textura, int quadros)
{
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    if (!c) { printf("sin contexto\n"); exit(1); }
    glViewport(0, 0, L, A);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-0.8f, 0.8f, -0.6f, 0.6f, 1.0f, 20.0f);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    if (textura) {
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
        glEnable(GL_TEXTURE_2D);
    }
    double t0 = agora_ms();
    for (int q = 0; q < quadros; q++) {
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        cubo((float)q * 3.0f);
    }
    double t = agora_ms() - t0;
    sgl_destruir_contexto(c);
    return t;
}

int main(int argc, char **argv)
{
    int quadros = argc > 1 ? atoi(argv[1]) : 60;
    for (int i = 0; i < 64 * 64; i++) {
        int x = i % 64, y = i / 64, q = ((x >> 3) ^ (y >> 3)) & 1;
        tex[i * 4] = (unsigned char)(q ? 230 : 40); tex[i * 4 + 1] = (unsigned char)(x * 4);
        tex[i * 4 + 2] = (unsigned char)(y * 4); tex[i * 4 + 3] = 255;
    }
    double tt = 1e30, tp = 1e30;
    for (int r = 0; r < 3; r++) {   /* el mejor de tres, para quitar ruido */
        double a = medir(1, quadros), b = medir(0, quadros);
        if (a < tt) tt = a;
        if (b < tp) tp = b;
    }
    printf("textura %.1f plano %.1f suma %08x\n", tt / quadros, tp / quadros, (unsigned)sgl_soma(px, L * A));
    return 0;
}
