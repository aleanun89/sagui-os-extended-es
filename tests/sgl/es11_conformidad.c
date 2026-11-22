// SPDX-License-Identifier: GPL-2.0-or-later
/* es11_conformidad.c - pruebas de conformidad de OpenGL ES 1.1 de SaguiGL (D425).
 *
 * Escrito SOLO con las cabeceras estándar (GLES/gl.h y GLES/glext.h) más las
 * dos funciones de SaguiGL que crean un contexto en memoria. Cada prueba dibuja
 * en un destino pequeño y mira píxeles concretos contra lo que dice la
 * especificación (con ±2 de margen de redondeo). Es determinista: al final da
 * la suma de una escena con todo, que el mismo programa compilado para el
 * SaguiOS debe repetir (tests/h424_sgl_es11.py).
 *
 * Salida: una línea por fallo y, al final, «conformidad: N bien, M mal» y
 * «suma <hex>». Código de salida 0 si no hay fallos. */
#include <GLES/gl.h>
#include <GLES/glext.h>

typedef struct sgl_ctx sgl_ctx_t;
sgl_ctx_t *sgl_criar_contexto_mem(unsigned int *px, int larg, int alt, int zbits);
void       sgl_destruir_contexto(sgl_ctx_t *c);
unsigned int sgl_soma(const unsigned int *px, int n);

#if defined(SGL_ANFITRIAO)
#include <stdio.h>
#include <string.h>
#define IMPRIMIR(...) printf(__VA_ARGS__)
#else
#include "sagui.h"
#define IMPRIMIR(...) imprimir(__VA_ARGS__)
static int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
static const char *strstr(const char *h, const char *n)
{
    for (; *h; h++) { const char *a = h, *b = n; while (*a && *b && *a == *b) { a++; b++; } if (!*b) return h; }
    return 0;
}
#endif

#define W 16
#define H 16
static unsigned int px[W * H];
static int bien, mal;

static void ver(int ok, const char *que)
{
    if (ok) bien++;
    else { mal++; IMPRIMIR("MAL: %s\n", que); }
}

/* Pixel (x, y) de GL (origen abajo a la izquierda). */
static unsigned int px_em(int x, int y) { return px[(H - 1 - y) * W + x] & 0xFFFFFF; }

static int perto(unsigned int c, int r, int g, int b)
{
    int cr = (int)((c >> 16) & 255), cg = (int)((c >> 8) & 255), cb = (int)(c & 255);
    int dr = cr - r, dg = cg - g, db = cb - b;
    return dr >= -2 && dr <= 2 && dg >= -2 && dg <= 2 && db >= -2 && db <= 2;
}

static void verc(int x, int y, int r, int g, int b, const char *que)
{
    unsigned int c = px_em(x, y);
    if (!perto(c, r, g, b)) { mal++; IMPRIMIR("MAL: %s (%d,%d): %06x, se esperaba %02x%02x%02x\n", que, x, y, c, r, g, b); }
    else bien++;
}

static sgl_ctx_t *C0;

static void novo(void)
{
    if (C0) sgl_destruir_contexto(C0);
    C0 = sgl_criar_contexto_mem(px, W, H, 32);
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
}

/* Un rectángulo con coordenadas de textura 0..1 en las unidades 0 y 1. */
static void quad(float x0, float y0, float x1, float y1, float z)
{
    GLfloat v[12] = { x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z };
    GLfloat t[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    for (int u = 0; u < 2; u++) {
        glClientActiveTexture(GL_TEXTURE0 + u);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, t);
    }
    glClientActiveTexture(GL_TEXTURE0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static GLuint tex1x1(GLenum fmt, const unsigned char *d)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)fmt, 1, 1, 0, fmt, GL_UNSIGNED_BYTE, d);
    return id;
}

static int b8(float f) { int v = (int)(f * 255.0f + 0.5f); return v < 0 ? 0 : v > 255 ? 255 : v; }

/* ---- TexEnv clásico: la tabla 3.15 ---- */
static void prueba_texenv(void)
{
    /* color del fragmento (0.8, 0.4, 0.2, 0.6), textura (0.2, 0.6, 1.0, 0.5), constante (0.1, 0.9, 0.5, 0.3) */
    static const unsigned char t_rgba[4] = { 51, 153, 255, 128 };
    static const float cf[4] = { 0.8f, 0.4f, 0.2f, 0.6f }, cc[4] = { 0.1f, 0.9f, 0.5f, 0.3f };
    const float ts[4] = { 51 / 255.0f, 153 / 255.0f, 1.0f, 128 / 255.0f };
    static const GLenum modos[5] = { GL_MODULATE, GL_REPLACE, GL_DECAL, GL_BLEND, GL_ADD };
    static const char *nomes[5] = { "MODULATE", "REPLACE", "DECAL", "BLEND", "ADD" };
    for (int m = 0; m < 5; m++) {
        novo();
        tex1x1(GL_RGBA, t_rgba);
        glEnable(GL_TEXTURE_2D);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, (GLint)modos[m]);
        glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, cc);
        glColor4f(cf[0], cf[1], cf[2], cf[3]);
        quad(0, 0, W, H, 0);
        float r[3];
        for (int k = 0; k < 3; k++) {
            switch (modos[m]) {
            case GL_MODULATE: r[k] = cf[k] * ts[k]; break;
            case GL_REPLACE: r[k] = ts[k]; break;
            case GL_DECAL: r[k] = cf[k] * (1 - ts[3]) + ts[k] * ts[3]; break;
            case GL_BLEND: r[k] = cf[k] * (1 - ts[k]) + cc[k] * ts[k]; break;
            default: r[k] = cf[k] + ts[k]; if (r[k] > 1) r[k] = 1; break;
            }
        }
        char q[64];
        for (int i = 0; i < 64; i++) q[i] = 0;
        { const char *a = "texenv RGBA "; int i = 0; while (*a) q[i++] = *a++; a = nomes[m]; while (*a) q[i++] = *a++; }
        verc(5, 5, b8(r[0]), b8(r[1]), b8(r[2]), q);
    }
    /* formatos: REPLACE con GL_ALPHA deja el color del fragmento; con GL_LUMINANCE pone L */
    novo();
    unsigned char a = 100;
    tex1x1(GL_ALPHA, &a);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(0.8f, 0.4f, 0.2f, 1.0f);
    quad(0, 0, W, H, 0);
    verc(3, 3, b8(0.8f), b8(0.4f), b8(0.2f), "texenv REPLACE con GL_ALPHA: el color del fragmento");
    novo();
    unsigned char l = 77;
    tex1x1(GL_LUMINANCE, &l);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    quad(0, 0, W, H, 0);
    verc(3, 3, 77, 77, 77, "texenv REPLACE con GL_LUMINANCE");
    /* BLEND con luminancia: Cp(1-L) + Cc L */
    novo();
    tex1x1(GL_LUMINANCE, &l);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
    glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, cc);
    glColor4f(cf[0], cf[1], cf[2], 1);
    quad(0, 0, W, H, 0);
    float lv = 77 / 255.0f;
    verc(3, 3, b8(cf[0] * (1 - lv) + cc[0] * lv), b8(cf[1] * (1 - lv) + cc[1] * lv), b8(cf[2] * (1 - lv) + cc[2] * lv), "texenv BLEND con GL_LUMINANCE");
}

/* ---- GL_COMBINE ---- */
static float comb(GLenum f, float a0, float a1, float a2)
{
    switch (f) {
    case GL_REPLACE: return a0;
    case GL_MODULATE: return a0 * a1;
    case GL_ADD: return a0 + a1;
    case GL_ADD_SIGNED: return a0 + a1 - 0.5f;
    case GL_INTERPOLATE: return a0 * a2 + a1 * (1 - a2);
    default: return a0 - a1;
    }
}
static float sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static void prueba_combine(void)
{
    static const unsigned char t[4] = { 200, 100, 50, 150 };
    const float ts[4] = { 200 / 255.0f, 100 / 255.0f, 50 / 255.0f, 150 / 255.0f };
    static const float cf[4] = { 0.3f, 0.6f, 0.9f, 0.4f }, cc[4] = { 0.5f, 0.25f, 0.75f, 0.8f };
    static const GLenum fs[6] = { GL_REPLACE, GL_MODULATE, GL_ADD, GL_ADD_SIGNED, GL_INTERPOLATE, GL_SUBTRACT };
    for (int i = 0; i < 6; i++)
        for (int esc = 1; esc <= 4; esc *= 2) {
            novo();
            tex1x1(GL_RGBA, t);
            glEnable(GL_TEXTURE_2D);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, (GLint)fs[i]);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, (GLint)fs[i]);
            /* arg0 = textura (color), arg1 = primario (1 - color), arg2 = constante (alfa) */
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_ONE_MINUS_SRC_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, (float)esc);
            glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, cc);
            glColor4f(cf[0], cf[1], cf[2], cf[3]);
            /* el alfa se ve con la mezcla: destino negro, GL_SRC_ALPHA, GL_ZERO -> color × alfa */
            quad(0, 0, W, H, 0);
            float r[3];
            for (int k = 0; k < 3; k++) r[k] = sat(comb(fs[i], ts[k], 1 - cf[k], cc[3]) * (float)esc);
            verc(4, 4, b8(r[0]), b8(r[1]), b8(r[2]), "combine RGB");
            /* el alfa: se dibuja otra vez multiplicando el color blanco por el alfa resultante */
            float al = sat(comb(fs[i], ts[3], 1 - cf[3], cc[3]));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ZERO);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_CONSTANT);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_ALPHA);   /* 1 - 0.8 = 0.2 */
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 4.0f);                       /* 0.8 */
            quad(0, 0, W, H, 0);
            glDisable(GL_BLEND);
            float e = sat(0.2f * 4.0f) * al;
            verc(4, 4, b8(e), b8(e), b8(e), "combine alfa");
        }
    /* DOT3: (0.75, 0.5, 0.25) · (0.75, 0.75, 0.5) => 4((.25)(.25) + 0 + (-.25)(0)) = 0.25 */
    novo();
    static const unsigned char tn[4] = { 191, 128, 64, 255 };
    tex1x1(GL_RGBA, tn);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_DOT3_RGB);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_PRIMARY_COLOR);
    glColor4f(0.75f, 0.75f, 0.5f, 1.0f);
    quad(0, 0, W, H, 0);
    float d = 4.0f * ((191 / 255.0f - 0.5f) * 0.25f + (128 / 255.0f - 0.5f) * 0.25f + (64 / 255.0f - 0.5f) * 0.0f);
    verc(2, 2, b8(d), b8(d), b8(d), "combine DOT3_RGB");
}

/* ---- multitextura ---- */
static void prueba_multitextura(void)
{
    novo();
    static const unsigned char a[4] = { 255, 128, 0, 255 }, b[4] = { 128, 255, 255, 255 };
    glActiveTexture(GL_TEXTURE0);
    tex1x1(GL_RGBA, a);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1);
    tex1x1(GL_RGBA, b);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, 1);
    quad(0, 0, W, H, 0);
    verc(7, 7, 128, 128, 0, "multitextura: unidad 0 × unidad 1");
    GLint n = 0;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &n);
    ver(n == 4, "GL_MAX_TEXTURE_UNITS = 4");
    glGetIntegerv(GL_ACTIVE_TEXTURE, &n);
    ver(n == GL_TEXTURE1, "GL_ACTIVE_TEXTURE");
    /* ADD en la unidad 1 */
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
    quad(0, 0, W, H, 0);
    verc(7, 7, 255, 255, 255, "multitextura: ADD en la unidad 1");
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    /* lightmap: la 1 con glMultiTexCoord (sin array) */
    glMultiTexCoord4f(GL_TEXTURE1, 0.5f, 0.5f, 0, 1);
}

/* ---- prueba alfa, esténcil, tijera, máscaras, Z ---- */
static void prueba_fragmentos(void)
{
    novo();
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glColor4f(1, 0, 0, 0.4f);
    quad(0, 0, 8, H, 0);
    glColor4f(0, 1, 0, 0.6f);
    quad(8, 0, W, H, 0);
    glDisable(GL_ALPHA_TEST);
    verc(2, 2, 0, 0, 0, "prueba alfa: 0.4 > 0.5 no pasa");
    verc(12, 2, 0, 255, 0, "prueba alfa: 0.6 > 0.5 pasa");
    /* esténcil: marcar la mitad izquierda, luego dibujar solo donde hay 1 */
    novo();
    GLint bits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &bits);
    ver(bits == 8, "GL_STENCIL_BITS = 8");
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    quad(0, 0, 8, H, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    glColor4f(0, 0, 1, 1);
    quad(0, 0, W, H, 0);
    verc(2, 2, 0, 0, 255, "esténcil EQUAL 1: dentro");
    verc(12, 2, 0, 0, 0, "esténcil EQUAL 1: fuera");
    glStencilFunc(GL_EQUAL, 2, 0xFF);   /* el INCR anterior dejó 2 a la izquierda (y este lo deja en 3) */
    glColor4f(1, 1, 0, 1);
    quad(0, 0, W, H, 0);
    verc(2, 2, 255, 255, 0, "esténcil INCR");
    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);   /* ~3 = 252 */
    quad(0, 0, W, H, 0);
    glStencilFunc(GL_EQUAL, 252, 0xFF);
    glColor4f(1, 0, 1, 1);
    quad(0, 0, W, H, 0);
    verc(2, 2, 255, 0, 255, "esténcil INVERT");
    glDisable(GL_STENCIL_TEST);
    /* tijera */
    novo();
    glEnable(GL_SCISSOR_TEST);
    glScissor(4, 4, 4, 4);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(0, 1, 0, 1);
    quad(0, 0, W, 6, 0);
    glDisable(GL_SCISSOR_TEST);
    verc(5, 5, 0, 255, 0, "tijera: dibujo dentro");
    verc(5, 7, 255, 0, 0, "tijera: limpiar dentro");
    verc(1, 1, 0, 0, 0, "tijera: fuera");
    /* máscara de color */
    novo();
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    glColor4f(1, 1, 1, 1);
    quad(0, 0, W, H, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    verc(3, 3, 255, 0, 255, "glColorMask sin el verde");
    /* Z: funciones, máscara, glClearDepthf y glDepthRangef */
    novo();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glColor4f(1, 0, 0, 1);
    quad(0, 0, W, H, 0.0f);          /* z ventana 0.5 */
    glDepthFunc(GL_GREATER);
    glColor4f(0, 1, 0, 1);
    quad(0, 0, 8, H, -0.5f);          /* z ventana 0.75: pasa GREATER */
    quad(8, 0, W, H, 0.5f);           /* z ventana 0.25: no pasa */
    verc(2, 2, 0, 255, 0, "Z GREATER pasa");
    verc(12, 2, 255, 0, 0, "Z GREATER no pasa");
    glDepthFunc(GL_EQUAL);
    glColor4f(0, 0, 1, 1);
    quad(8, 0, W, H, 0.0f);
    verc(12, 2, 0, 0, 255, "Z EQUAL");
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_ALWAYS);
    glColor4f(1, 1, 1, 1);
    quad(0, 0, W, H, 0.9f);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_EQUAL);
    glColor4f(1, 1, 0, 1);
    quad(8, 0, W, H, 0.0f);
    verc(12, 2, 255, 255, 0, "glDepthMask(GL_FALSE) no escribe Z");
    glDepthFunc(GL_NEVER);
    glColor4f(0, 0, 0, 1);
    quad(0, 0, W, H, 0.0f);
    verc(12, 2, 255, 255, 0, "Z NEVER");
    glClearDepthf(0.2f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LESS);
    glColor4f(0, 1, 1, 1);
    quad(0, 0, W, H, 0.7f);           /* 0.15 < 0.2 */
    verc(3, 3, 0, 255, 255, "glClearDepthf(0.2)");
    glColor4f(1, 0, 0, 1);
    quad(0, 0, W, H, 0.5f);           /* 0.25: no */
    verc(3, 3, 0, 255, 255, "glClearDepthf: lo de detrás no pasa");
    glClearDepthf(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthRangef(0.5f, 1.0f);       /* z = -1 -> 0.5 */
    glColor4f(0, 1, 0, 1);
    quad(0, 0, W, H, 1.0f);
    glDepthRangef(0.0f, 1.0f);
    glColor4f(0, 0, 1, 1);
    quad(0, 0, 8, H, 0.1f);           /* 0.45 < 0.5 */
    quad(8, 0, W, H, -0.1f);          /* 0.55: no */
    verc(2, 2, 0, 0, 255, "glDepthRangef delante");
    verc(12, 2, 0, 255, 0, "glDepthRangef detrás");
    glDisable(GL_DEPTH_TEST);
    /* desplazamiento del polígono: el mismo plano gana con LESS */
    novo();
    glEnable(GL_DEPTH_TEST);
    glColor4f(1, 0, 0, 1);
    quad(0, 0, W, H, 0.0f);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.0f, -1.0f);
    glColor4f(0, 1, 0, 1);
    quad(0, 0, W, H, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    verc(5, 5, 0, 255, 0, "glPolygonOffset");
    glDisable(GL_DEPTH_TEST);
}

/* ---- mezcla y lógica ---- */
static void prueba_mezcla(void)
{
    novo();
    glClearColor(0.6f, 0.6f, 0.6f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquationOES(GL_FUNC_REVERSE_SUBTRACT_OES);
    glColor4f(0.2f, 0.4f, 0.8f, 1);
    quad(0, 0, W, H, 0);
    verc(3, 3, b8(0.4f), b8(0.2f), 0, "OES_blend_subtract (destino - origen)");
    glBlendEquationOES(GL_FUNC_ADD_OES);
    glBlendFuncSeparateOES(GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ZERO);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(0.5f, 0.5f, 0.5f, 1);
    quad(0, 0, W, H, 0);
    verc(3, 3, b8(0.3f), b8(0.3f), b8(0.3f), "OES_blend_func_separate (GL_DST_COLOR)");
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_XOR);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(1, 1, 0, 1);
    quad(0, 0, W, H, 0);
    glDisable(GL_COLOR_LOGIC_OP);
    verc(3, 3, 0, 255, 0, "glLogicOp(GL_XOR)");
}

/* ---- VBO, índices de 32 bits, mapbuffer ---- */
static void prueba_vbo(void)
{
    novo();
    GLfloat v[8] = { 0, 0, W, 0, W, H, 0, H };
    GLubyte cor[16] = { 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255 };
    GLuint idx[6] = { 0, 1, 2, 0, 2, 3 };
    GLuint b[3];
    glGenBuffers(3, b);
    glBindBuffer(GL_ARRAY_BUFFER, b[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glVertexPointer(2, GL_FLOAT, 0, (const void *)0);
    glBindBuffer(GL_ARRAY_BUFFER, b[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof cor, 0, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof cor, cor);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, (const void *)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void *)0);
    verc(8, 8, 0, 255, 0, "VBO + índices GL_UNSIGNED_INT en búfer");
    GLint t = 0;
    glBindBuffer(GL_ARRAY_BUFFER, b[1]);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &t);
    ver(t == 16, "glGetBufferParameteriv(GL_BUFFER_SIZE)");
    GLubyte *m = (GLubyte *)glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    ver(m != 0, "glMapBufferOES");
    if (m) for (int i = 0; i < 4; i++) { m[i * 4] = 255; m[i * 4 + 1] = 0; }
    ver(glUnmapBufferOES(GL_ARRAY_BUFFER) == GL_TRUE, "glUnmapBufferOES");
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void *)0);
    verc(8, 8, 255, 0, 0, "OES_mapbuffer: lo escrito se dibuja");
    ver(glIsBuffer(b[0]) == GL_TRUE, "glIsBuffer");
    glDeleteBuffers(3, b);
    ver(glIsBuffer(b[0]) == GL_FALSE, "glDeleteBuffers");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisableClientState(GL_COLOR_ARRAY);
    /* coma fija y bytes en los arrays */
    novo();
    GLshort vs[8] = { 0, 0, W, 0, W, H, 0, H };
    glVertexPointer(2, GL_SHORT, 0, vs);
    glColor4x(0, 65536, 65536, 65536);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    verc(4, 4, 0, 255, 255, "arrays GL_SHORT y glColor4x");
}

/* ---- puntos y líneas ---- */
static void prueba_puntos(void)
{
    novo();
    GLfloat p[3] = { 8.0f, 8.0f, 0 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, p);
    glPointSize(4.0f);
    glColor4f(1, 1, 1, 1);
    glDrawArrays(GL_POINTS, 0, 1);
    verc(6, 6, 255, 255, 255, "punto de 4: esquina");
    verc(9, 9, 255, 255, 255, "punto de 4: esquina opuesta");
    verc(5, 5, 0, 0, 0, "punto de 4: fuera");
    verc(10, 10, 0, 0, 0, "punto de 4: fuera (2)");
    /* sprite con coordenadas: textura 2x2 de colores, punto de 8 */
    novo();
    static const unsigned char t[16] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255 };
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glVertexPointer(3, GL_FLOAT, 0, p);
    glEnable(GL_POINT_SPRITE_OES);
    glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
    glPointSize(8.0f);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisable(GL_POINT_SPRITE_OES);
    /* t = 0 arriba: la fila 0 de la textura (rojo, verde) arriba */
    verc(5, 10, 255, 0, 0, "sprite: arriba izquierda = texel (0,0)");
    verc(10, 10, 0, 255, 0, "sprite: arriba derecha = texel (1,0)");
    verc(5, 5, 0, 0, 255, "sprite: abajo izquierda = texel (0,1)");
    verc(10, 5, 255, 255, 0, "sprite: abajo derecha = texel (1,1)");
    glDisable(GL_TEXTURE_2D);
    /* OES_point_size_array */
    novo();
    GLfloat pp[6] = { 4, 4, 0, 12, 12, 0 }, tam[2] = { 1, 3 };
    glVertexPointer(3, GL_FLOAT, 0, pp);
    glEnableClientState(GL_POINT_SIZE_ARRAY_OES);
    glPointSizePointerOES(GL_FLOAT, 0, tam);
    glColor4f(1, 0, 0, 1);
    glDrawArrays(GL_POINTS, 0, 2);
    glDisableClientState(GL_POINT_SIZE_ARRAY_OES);
    verc(4, 4, 255, 0, 0, "tamaño por punto: 1");
    verc(5, 4, 0, 0, 0, "tamaño por punto: 1 (vecino)");
    verc(11, 11, 255, 0, 0, "tamaño por punto: 3");
    verc(13, 13, 255, 0, 0, "tamaño por punto: 3 (borde: píxeles 11 a 13)");
    verc(14, 14, 0, 0, 0, "tamaño por punto: 3 (fuera)");
    /* línea de 3 de ancho */
    novo();
    GLfloat l[6] = { 1, 8.5f, 0, 15, 8.5f, 0 };
    glVertexPointer(3, GL_FLOAT, 0, l);
    glLineWidth(3.0f);
    glColor4f(0, 1, 0, 1);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);
    verc(8, 7, 0, 255, 0, "línea de 3: arriba");
    verc(8, 9, 0, 255, 0, "línea de 3: abajo");
    verc(8, 11, 0, 0, 0, "línea de 3: fuera");
}

/* ---- texturas: formatos, comprimidas, espejo, subimagen, copia ---- */
static void prueba_texturas(void)
{
    novo();
    /* DXT1: c0 = rojo puro (F800), c1 = azul (001F); índices 0,1,2,3 en la primera fila */
    static const unsigned char dxt1[8] = { 0x00, 0xF8, 0x1F, 0x00, 0xE4, 0xE4, 0xE4, 0xE4 };
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, dxt1);
    ver(glGetError() == GL_NO_ERROR, "glCompressedTexImage2D DXT1 sin error");
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    quad(0, 0, W, H, 0);   /* 4 píxeles por texel */
    verc(1, 1, 255, 0, 0, "DXT1 índice 0");
    verc(5, 1, 0, 0, 255, "DXT1 índice 1");
    verc(9, 1, 170, 0, 85, "DXT1 índice 2 (2/3 + 1/3)");
    verc(13, 1, 85, 0, 170, "DXT1 índice 3");
    /* DXT5: alfa a0 = 255, a1 = 0, índices de alfa 1 -> alfa 0; color blanco */
    static const unsigned char dxt5[16] = { 255, 0, 0x49, 0x92, 0x24, 0x49, 0x92, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 };
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 4, 4, 0, 16, dxt5);
    ver(glGetError() == GL_NO_ERROR, "glCompressedTexImage2D DXT5 sin error");
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    quad(0, 0, W, H, 0);
    glDisable(GL_BLEND);
    verc(1, 1, 0, 0, 0, "DXT5: alfa del índice 1 = 0");
    /* DXT3: alfa explícito 8 (=136) en todo el bloque; color rojo */
    static const unsigned char dxt3[16] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0xF8, 0x00, 0xF8, 0, 0, 0, 0 };
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 4, 4, 0, 16, dxt3);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    quad(0, 0, W, H, 0);
    glDisable(GL_BLEND);
    verc(7, 7, 136, 0, 0, "DXT3: alfa explícito");
    /* paleta de 4 bits, RGB8: 2x2 con índices 0 1 / 1 0 */
    static const unsigned char pal[16 * 3 + 2] = { 255, 0, 0, 0, 0, 255, [48] = 0x01, [49] = 0x10 };
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_PALETTE4_RGB8_OES, 2, 2, 0, sizeof pal, pal);
    ver(glGetError() == GL_NO_ERROR, "glCompressedTexImage2D paleta sin error");
    quad(0, 0, W, H, 0);
    verc(3, 3, 255, 0, 0, "paleta: texel (0,0)");
    verc(12, 3, 0, 0, 255, "paleta: texel (1,0)");
    verc(3, 12, 0, 0, 255, "paleta: texel (0,1)");
    /* espejo: textura de 2x1 (rojo, verde) de 0 a 2 en s -> R G G R */
    novo();
    static const unsigned char rg[8] = { 255, 0, 0, 255, 0, 255, 0, 255 };
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rg);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT_OES);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_TEXTURE);
    glScalef(2, 1, 1);
    glMatrixMode(GL_MODELVIEW);
    quad(0, 0, W, H, 0);
    verc(1, 1, 255, 0, 0, "espejo: 1.º");
    verc(5, 1, 0, 255, 0, "espejo: 2.º");
    verc(9, 1, 0, 255, 0, "espejo: 3.º (reflejado)");
    verc(13, 1, 255, 0, 0, "espejo: 4.º (reflejado)");
    GLfloat mt[16];
    glMatrixMode(GL_TEXTURE);
    glGetFloatv(GL_TEXTURE_MATRIX, mt);
    ver(mt[0] == 2.0f, "glGetFloatv(GL_TEXTURE_MATRIX)");
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    /* glTexSubImage2D sobre el segundo texel */
    static const unsigned char azul[4] = { 0, 0, 255, 255 };
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, azul);
    quad(0, 0, W, H, 0);
    verc(12, 1, 0, 0, 255, "glTexSubImage2D");
    /* glCopyTexImage2D: lo de la ventana a una textura (sin alfa: GL_RGB) */
    novo();
    glClearColor(0.2f, 0.4f, 0.6f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 4, 4, 0);
    ver(glGetError() == GL_NO_ERROR, "glCopyTexImage2D sin error");
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    quad(0, 0, W, H, 0);
    verc(8, 8, b8(0.2f), b8(0.4f), b8(0.6f), "glCopyTexImage2D");
    /* glReadPixels: abajo a la izquierda primero, RGBA */
    unsigned char rp[4 * 4];
    glReadPixels(0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, rp);
    ver(rp[0] == (unsigned char)b8(0.2f) && rp[1] == (unsigned char)b8(0.4f) && rp[3] == 255, "glReadPixels RGBA");
}

/* ---- cubos y generación de coordenadas ---- */
static void prueba_cubo(void)
{
    novo();
    static const unsigned char cores[6][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 },
                                               { 255, 255, 0, 255 }, { 0, 255, 255, 255 }, { 255, 0, 255, 255 } };
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP_OES, id);
    for (int f = 0; f < 6; f++) {
        unsigned char d[4 * 4 * 4];
        for (int i = 0; i < 16; i++) for (int k = 0; k < 4; k++) d[i * 4 + k] = cores[f][k];
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + f, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    }
    ver(glGetError() == GL_NO_ERROR, "cubo: seis caras sin error");
    glEnable(GL_TEXTURE_CUBE_MAP_OES);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    /* dirección por la coordenada actual (sin array) */
    glClientActiveTexture(GL_TEXTURE0);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    GLfloat v[12] = { 0, 0, 0, W, 0, 0, W, H, 0, 0, H, 0 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    static const float dir[6][3] = { { 1, 0.1f, 0.2f }, { -1, 0.2f, 0.1f }, { 0.1f, 1, 0.2f }, { 0.2f, -1, 0.1f }, { 0.1f, 0.2f, 1 }, { 0.2f, 0.1f, -1 } };
    for (int f = 0; f < 6; f++) {
        glMultiTexCoord4f(GL_TEXTURE0, dir[f][0], dir[f][1], dir[f][2], 1);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        verc(8, 8, cores[f][0], cores[f][1], cores[f][2], "cubo: cara por la dirección");
    }
    /* NORMAL_MAP con la normal (0, 0, 1) -> +Z; REFLECTION_MAP mirando por -Z con normal +Z -> +Z */
    glEnable(GL_TEXTURE_GEN_STR_OES);
    glTexGeniOES(GL_TEXTURE_GEN_STR_OES, GL_TEXTURE_GEN_MODE_OES, GL_NORMAL_MAP_OES);
    glNormal3f(0, 0, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    verc(8, 8, 0, 255, 255, "GL_NORMAL_MAP_OES");
    glNormal3f(1, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    verc(8, 8, 255, 0, 0, "GL_NORMAL_MAP_OES (+X)");
    glTexGeniOES(GL_TEXTURE_GEN_STR_OES, GL_TEXTURE_GEN_MODE_OES, GL_REFLECTION_MAP_OES);
    glLoadIdentity();
    glTranslatef(0, 0, -100.0f);     /* el ojo ve el plano de lejos, por -Z */
    glNormal3f(0, 0, 1);
    GLfloat v2[12] = { 7.9f, 7.9f, 0, 8.1f, 7.9f, 0, 8.1f, 8.1f, 0, 7.9f, 8.1f, 0 };
    (void)v2;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-W / 2, W / 2, -H / 2, H / 2, 0, 200);
    glMatrixMode(GL_MODELVIEW);
    GLfloat v3[12] = { -8, -8, 0, 8, -8, 0, 8, 8, 0, -8, 8, 0 };
    glVertexPointer(3, GL_FLOAT, 0, v3);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    /* u ~ (0,0,-1): r = u - 2n(n·u) ~ (0,0,1) -> +Z (cian) */
    verc(8, 8, 0, 255, 255, "GL_REFLECTION_MAP_OES");
    glDisable(GL_TEXTURE_GEN_STR_OES);
    glDisable(GL_TEXTURE_CUBE_MAP_OES);
    GLint b = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_OES, &b);
    ver(b == (GLint)id, "GL_TEXTURE_BINDING_CUBE_MAP_OES");
}

/* ---- FBO: dibujar en una textura ---- */
static void prueba_fbo(void)
{
    novo();
    GLuint t, f, rb;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glGenFramebuffersOES(1, &f);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, f);
    glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_TEXTURE_2D, t, 0);
    glGenRenderbuffersOES(1, &rb);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, rb);
    glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT24_OES, 8, 8);
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, rb);
    ver(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) == GL_FRAMEBUFFER_COMPLETE_OES, "FBO completo");
    glViewport(0, 0, 8, 8);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, 8, 0, 8, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glColor4f(1, 0, 0, 1);
    quad(0, 0, 4, 8, 0);
    glColor4f(0, 1, 0, 1);
    quad(0, 0, 8, 8, -0.5f);          /* detrás del rojo a la izquierda (z ventana 0.75) */
    glDisable(GL_DEPTH_TEST);
    GLint bits = 0;
    glGetIntegerv(GL_DEPTH_BITS, &bits);
    ver(bits == 24, "FBO: GL_DEPTH_BITS = 24");
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    quad(0, 0, W, H, 0);
    glDisable(GL_TEXTURE_2D);
    verc(3, 8, 255, 0, 0, "FBO: la textura tiene el rojo");
    verc(12, 8, 0, 255, 0, "FBO: y el verde (Z del FBO)");
    glDeleteFramebuffersOES(1, &f);
    glDeleteRenderbuffersOES(1, &rb);
    ver(glIsFramebufferOES(f) == GL_FALSE, "glDeleteFramebuffersOES");
}

/* ---- planos de recorte, paleta de matrices, draw_texture, consultas, coma fija ---- */
static void prueba_resto(void)
{
    novo();
    GLfloat pl[4] = { -1, 0, 0, 8 };   /* x <= 8 */
    glClipPlanef(GL_CLIP_PLANE0, pl);
    glEnable(GL_CLIP_PLANE0);
    glColor4f(1, 1, 1, 1);
    quad(0, 0, W, H, 0);
    glDisable(GL_CLIP_PLANE0);
    verc(3, 3, 255, 255, 255, "glClipPlanef: dentro");
    verc(12, 3, 0, 0, 0, "glClipPlanef: fuera");
    GLfloat e[4];
    glGetClipPlanef(GL_CLIP_PLANE0, e);
    ver(e[0] == -1.0f && e[3] == 8.0f, "glGetClipPlanef");
    /* paleta: la matriz 1 lleva 8 a la derecha; peso 1 */
    novo();
    glMatrixMode(GL_MATRIX_PALETTE_OES);
    glCurrentPaletteMatrixOES(1);
    glLoadIdentity();
    glTranslatef(8, 0, 0);
    glCurrentPaletteMatrixOES(0);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_MATRIX_PALETTE_OES);
    GLfloat v[8] = { 0, 0, 8, 0, 8, H, 0, H };
    GLubyte mi[4] = { 1, 1, 1, 1 };
    GLfloat pw[4] = { 1, 1, 1, 1 };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, v);
    glEnableClientState(GL_MATRIX_INDEX_ARRAY_OES);
    glEnableClientState(GL_WEIGHT_ARRAY_OES);
    glMatrixIndexPointerOES(1, GL_UNSIGNED_BYTE, 0, mi);
    glWeightPointerOES(1, GL_FLOAT, 0, pw);
    glColor4f(1, 0, 0, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisable(GL_MATRIX_PALETTE_OES);
    glDisableClientState(GL_MATRIX_INDEX_ARRAY_OES);
    glDisableClientState(GL_WEIGHT_ARRAY_OES);
    verc(12, 5, 255, 0, 0, "OES_matrix_palette: movido 8 a la derecha");
    verc(3, 5, 0, 0, 0, "OES_matrix_palette: no queda a la izquierda");
    /* draw_texture: textura 2x2, recorte del texel (1,1) a un rectángulo de 4x4 en (2,2) */
    novo();
    static const unsigned char t[16] = { 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255 };
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, t);
    GLint crop[4] = { 1, 1, 1, 1 };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDrawTexiOES(2, 2, 0, 4, 4);
    glDisable(GL_TEXTURE_2D);
    verc(3, 3, 255, 255, 0, "OES_draw_texture con recorte");
    verc(7, 7, 0, 0, 0, "OES_draw_texture: fuera");
    GLint cr[4];
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cr);
    ver(cr[0] == 1 && cr[3] == 1, "glGetTexParameteriv(GL_TEXTURE_CROP_RECT_OES)");
    /* consultas */
    const char *ver_ = (const char *)glGetString(GL_VERSION), *ext = (const char *)glGetString(GL_EXTENSIONS);
    ver(ver_ && !strcmp(ver_, "OpenGL ES-CM 1.1"), "GL_VERSION");
    static const char *exts[] = { "GL_OES_point_sprite", "GL_OES_point_size_array", "GL_OES_draw_texture", "GL_OES_blend_subtract",
                                  "GL_OES_blend_func_separate", "GL_OES_blend_equation_separate", "GL_OES_texture_mirrored_repeat",
                                  "GL_OES_element_index_uint", "GL_OES_mapbuffer", "GL_OES_texture_npot", "GL_EXT_texture_compression_s3tc",
                                  "GL_EXT_texture_filter_anisotropic", "GL_OES_stencil8", "GL_OES_depth24", "GL_OES_framebuffer_object",
                                  "GL_OES_texture_cube_map", "GL_OES_matrix_palette", "GL_OES_query_matrix", "GL_OES_compressed_paletted_texture" };
    for (unsigned i = 0; i < sizeof exts / sizeof exts[0]; i++) ver(ext && strstr(ext, exts[i]) != 0, exts[i]);
    GLboolean bb = GL_TRUE;
    glGetBooleanv(GL_DEPTH_TEST, &bb);
    ver(bb == GL_FALSE && glIsEnabled(GL_BLEND) == GL_FALSE, "glGetBooleanv y glIsEnabled");
    glEnable(GL_FOG);
    ver(glIsEnabled(GL_FOG) == GL_TRUE, "glIsEnabled(GL_FOG)");
    glDisable(GL_FOG);
    GLint ti = 0;
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &ti);
    ver(ti == GL_DECAL, "glGetTexEnviv");
    GLfixed fx[4];
    glClearColor(0.5f, 0, 0, 1);
    glGetFixedv(GL_COLOR_CLEAR_VALUE, fx);
    ver(fx[0] > 32700 && fx[0] < 32900 && fx[3] == 65536, "glGetFixedv(GL_COLOR_CLEAR_VALUE)");
    GLfloat amb[4] = { 0.1f, 0.2f, 0.3f, 1 }, g[4];
    glLightfv(GL_LIGHT2, GL_AMBIENT, amb);
    glGetLightfv(GL_LIGHT2, GL_AMBIENT, g);
    ver(g[1] == 0.2f, "glGetLightfv");
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 33);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, g);
    ver(g[0] == 33.0f, "glGetMaterialfv");
    GLint mx = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mx);
    ver(mx >= 64, "GL_MAX_TEXTURE_SIZE");
    ver(glGetError() == GL_NO_ERROR, "sin error hasta aquí");
    glEnable(0x1234);
    ver(glGetError() == GL_INVALID_ENUM, "glEnable de algo que no existe: GL_INVALID_ENUM");
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 3.0f);
    ver(glGetError() == GL_INVALID_VALUE, "GL_RGB_SCALE 3: GL_INVALID_VALUE");
    /* OES_query_matrix y coma fija */
    glLoadIdentity();
    glTranslatex(3 << 16, 0, 0);
    GLfixed m[16]; GLint ex[16];
    GLbitfield malos = glQueryMatrixxOES(m, ex);
    float v12 = (float)m[12] / 65536.0f;
    for (int i = 0; i < ex[12]; i++) v12 *= 2.0f;
    for (int i = 0; i > ex[12]; i--) v12 *= 0.5f;
    ver(malos == 0 && v12 > 2.99f && v12 < 3.01f, "glQueryMatrixxOES tras glTranslatex");
    GLint bits[2];
    glGetIntegerv(GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES, bits);
    ver(bits[0] == 0x3F800000, "OES_matrix_get");
}

/* La escena con todo, para la suma (el SaguiOS debe dar la misma). */
static unsigned int escena(void)
{
    novo();
    static unsigned char tx[8 * 8 * 4], lm[4 * 4];
    for (int i = 0; i < 64; i++) { tx[i * 4] = (unsigned char)(i * 4); tx[i * 4 + 1] = (unsigned char)(255 - i * 3); tx[i * 4 + 2] = (unsigned char)(i * 7); tx[i * 4 + 3] = (unsigned char)(128 + i); }
    for (int i = 0; i < 16; i++) lm[i] = (unsigned char)(40 + i * 13);
    GLuint t[2];
    glGenTextures(2, t);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmapOES(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 4, 4, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, lm);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -3);
    glRotatef(30, 1, 0.5f, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glFogf(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 2.0f);
    glFogf(GL_FOG_END, 5.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.9f, 0.8f, 0.7f, 0.9f);
    quad(-1, -1, 1, 1, 0);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    return sgl_soma(px, W * H);
}

static int rodar(void)
{
    prueba_texenv();
    prueba_combine();
    prueba_multitextura();
    prueba_fragmentos();
    prueba_mezcla();
    prueba_vbo();
    prueba_puntos();
    prueba_texturas();
    prueba_cubo();
    prueba_fbo();
    prueba_resto();
    unsigned int s = escena();
    IMPRIMIR("conformidad: %d bien, %d mal\n", bien, mal);
    IMPRIMIR("suma %08x\n", s);
    if (C0) sgl_destruir_contexto(C0);
    return mal ? 1 : 0;
}

/* En el SaguiOS el punto de entrada es «principal»; en el anfitrión, «main». */
#if defined(SGL_ANFITRIAO)
int main(void) { return rodar(); }
#else
int principal(int argc, char **argv) { (void)argc; (void)argv; return rodar(); }
#endif
