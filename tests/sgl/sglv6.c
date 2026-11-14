// SPDX-License-Identifier: GPL-2.0-or-later
/* sglv6.c - prueba de píxeles de la ruta v6 del SaguiGL (mejora 054, D130).
 *
 * Dibuja la misma escena (un quad con textura, luz, niebla y mezcla) con la
 * ruta de referencia y con la ruta SIMD, y comprueba que el píxel es el mismo
 * bit a bit. También dice cuántos píxeles se han pintado y cuántos colores
 * distintos hay, para que la prueba vea que la textura se ve.
 *
 * El programa se compila de dos formas: en el anfitrión (-DSGL_ANFITRIAO, con
 * printf) y dentro del SaguiOS (con la libsagui), para que la prueba
 * tests/h101_sagui_gl_v6.py compare los dos.
 *
 * Salida:
 *   textura pintados <n> cores <n>
 *   completa pintados <n> cores <n> dif <n> soma <hex>
 */
#include "sgl.h"

#if defined(SGL_ANFITRIAO)
#include <stdio.h>
#include <stdlib.h>
#define IMPRIMIR(...) printf(__VA_ARGS__)
#else
#include "sagui.h"
#include "gui.h"
#define IMPRIMIR(...) imprimir(__VA_ARGS__)
#endif

#define L 160
#define A 120

/* El quad ocupa de x=16 a x=143 y de y=24 a y=95: 128x72 = 9216 píxeles. */
#define PINTADOS_ESPERADOS 9216

static uint32_t px_ref[L * A], px_simd[L * A];

/* Textura de 8x8 generada aquí mismo (sin ficheros de fuera). */
static unsigned char tex[8 * 8 * 4];

static void criar_textura(void)
{
    for (int i = 0; i < 64; i++) {
        tex[i * 4 + 0] = (unsigned char)(i * 4);
        tex[i * 4 + 1] = (unsigned char)(255 - i * 4);
        tex[i * 4 + 2] = (unsigned char)(i * 2);
        tex[i * 4 + 3] = (unsigned char)((i & 1) ? 255 : 128);
    }
}

/* Dibuja la escena. Devuelve 0 si todo va bien y 1 si no hay contexto; deja
 * en *pintados los píxeles distintos del fondo y en *cores los colores
 * distintos que se han visto. */
static int cena(uint32_t *px, int textura, int luz, int niebla, int mistura,
                int *pintados, int *cores)
{
    sgl_ctx_t *c = sgl_criar_contexto_mem(px, L, A, 32);
    if (!c) { IMPRIMIR("sin contexto\n"); return 1; }
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, L, A);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1.0f, 1.0f, -0.75f, 0.75f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (textura) {
        GLuint id;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glEnable(GL_TEXTURE_2D);
    }
    if (luz) {
        float pos[4] = { 0.5f, 0.8f, 1.0f, 0.0f };
        float dif[4] = { 1.0f, 0.9f, 0.7f, 1.0f };
        float amb[4] = { 0.2f, 0.2f, 0.25f, 1.0f };
        glLightfv(GL_LIGHT0, GL_LIGHT_POSITION, pos);
        glLightfv(GL_LIGHT0, GL_LIGHT_DIFFUSE, dif);
        glLightfv(GL_LIGHT0, GL_LIGHT_AMBIENT, amb);
        glEnable(GL_LIGHTING);
    }
    if (niebla) {
        float cor[4] = { 0.2f, 0.2f, 0.3f, 1.0f };
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, 1.0f);
        glFogfv(GL_FOG_COLOR, cor);
        glEnable(GL_FOG);
    }
    if (mistura) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    static const float v[4][3] = {
        { -0.8f, -0.6f, 0.0f }, { 0.8f, -0.6f, 0.0f },
        { 0.8f, 0.6f, 0.0f }, { -0.8f, 0.6f, 0.0f } };
    static const float tc[4][2] = { { 0, 0 }, { 2, 0 }, { 2, 2 }, { 0, 2 } };
    static const float cor[4][4] = {
        { 1, 1, 1, 1 }, { 1, 1, 1, 0.5f }, { 1, 1, 1, 1 }, { 1, 1, 1, 0.8f } };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    glTexCoordPointer(2, GL_FLOAT, 0, tc);
    glColorPointer(4, GL_FLOAT, 0, cor);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);

    /* Cuenta los píxeles pintados y los colores distintos (el fondo no cuenta
     * como color de la escena). */
    uint32_t fundo = px[0];
    int pint = 0, ncores = 0;
    static uint32_t vistos[L * A];
    for (int i = 0; i < L * A; i++) {
        if (px[i] == fundo) continue;
        pint++;
        int visto = 0;
        for (int k = 0; k < ncores; k++) if (vistos[k] == px[i]) { visto = 1; break; }
        if (!visto) vistos[ncores++] = px[i];
    }
    *pintados = pint;
    *cores = ncores;
    sgl_destruir_contexto(c);
    return 0;
}

static int rodar(void)
{
    criar_textura();
    int pintados = 0, cores = 0, dif = 0, pintados2 = 0, cores2 = 0;
    int falha = 0;

    /* Escena con solo la textura: se tienen que ver los 64 texels (y solo
     * ellos), que es lo que demuestra que la textura se aplica. */
    sgl_motor_simd(0);
    if (cena(px_ref, 1, 0, 0, 0, &pintados, &cores)) return 1;
    IMPRIMIR("textura pintados %d cores %d\n", pintados, cores);
    if (pintados != PINTADOS_ESPERADOS) { IMPRIMIR("FALLO: pintados %d\n", pintados); falha = 1; }
    if (cores != 64) { IMPRIMIR("FALLO: cores %d (se esperaban 64)\n", cores); falha = 1; }

    /* Escena completa: la ruta SIMD tiene que dar el mismo píxel que la de
     * referencia, y la imagen tiene que tener muchos colores (la textura se ve
     * con la luz, la niebla y la mezcla encima). */
    sgl_motor_simd(0);
    if (cena(px_ref, 1, 1, 1, 1, &pintados, &cores)) return 1;
    sgl_motor_simd(1);
    if (cena(px_simd, 1, 1, 1, 1, &pintados2, &cores2)) return 1;
    for (int i = 0; i < L * A; i++) if (px_ref[i] != px_simd[i]) dif++;
    IMPRIMIR("completa pintados %d cores %d dif %d soma %08x\n",
            pintados2, cores2, dif, sgl_soma(px_simd, L * A));
    if (pintados2 != PINTADOS_ESPERADOS) { IMPRIMIR("FALLO: pintados %d\n", pintados2); falha = 1; }
    if (cores2 < 100) { IMPRIMIR("FALLO: cores %d (se esperaban más de 100)\n", cores2); falha = 1; }
    if (dif != 0) { IMPRIMIR("FALLO: %d píxeles difieren entre SIMD y referencia\n", dif); falha = 1; }
    return falha;
}

/* En el SaguiOS el punto de entrada es "principal"; en el anfitrión, "main". */
#if defined(SGL_ANFITRIAO)
int main(void) { return rodar(); }
#else
int principal(int argc, char **argv) { (void)argc; (void)argv; return rodar(); }
#endif
