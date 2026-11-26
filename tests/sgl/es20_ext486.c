// SPDX-License-Identifier: GPL-3.0-only
/* es20_ext486.c - conformidad de las extensiones de OpenGL ES 2.0 de la D486 en SaguiGL 2.0:
 *   GL_OES_depth_texture, GL_OES_mapbuffer, GL_OES_depth24 y GL_OES_packed_depth_stencil
 * (y que EGL ofrece configuraciones con Z de 24 bits, también en pbuffer).
 *
 * Escrito con las cabeceras estándar (GLES2/gl2.h, GLES2/gl2ext.h, EGL/egl.h). Sirve en el anfitrión (-DSGL_ANFITRIAO), en el SaguiOS por la
 * CPU y por la GPU (VirGL, gpu3d v4). Salida: una línea «MAL:» por fallo, «conformidad: N bien, M mal», «suma <hex>» (la de la CPU: igual en el
 * PC y en el SaguiOS) y, por escena, «gpu-vs-cpu <escena>: N pixeles distintos, gpu G, cpu C», que compara lo que da la GPU con lo que da la CPU
 * leyendo los píxeles (G = dibujos que hizo la GPU, C = los que la GPU devolvió a la CPU). Con «--cpu» no se usa la GPU. */
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#if defined(SGL_ANFITRIAO)
#include <stdio.h>
#include <string.h>
#define IMPRIMIR(...) printf(__VA_ARGS__)
#else
#include "sagui.h"
#define IMPRIMIR(...) imprimir(__VA_ARGS__)
static const char *strstr(const char *h, const char *n)
{
    for (; *h; h++) { const char *a = h, *b = n; while (*a && *b && *a == *b) { a++; b++; } if (!*b) return h; }
    return 0;
}
static int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
#endif

void sgl2_gpu_modo(int modo);
int  sgl2_gpu_estado(void);
void sgl2_gpu_contadores(unsigned *des, unsigned *lim, unsigned *cpu);

#define W 64
#define H 64
static int bien, mal, solo_cpu;
static unsigned soma = 2166136261u;

static void ver(int ok, const char *que)
{
    if (ok) bien++;
    else { mal++; IMPRIMIR("MAL: %s\n", que); }
}
static unsigned char lido[W * H * 4];
static const unsigned char *ler_quadro(void)
{
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, lido);
    for (int i = 0; i < W * H * 4; i++) { soma ^= lido[i]; soma *= 16777619u; }
    return lido;
}
static int perto(int a, int b, int tol) { return a - b <= tol && b - a <= tol; }
static void verpx(int x, int y, int r, int g, int b, int tol, const char *que)
{
    const unsigned char *p = lido + (y * W + x) * 4;
    if (perto(p[0], r, tol) && perto(p[1], g, tol) && perto(p[2], b, tol)) { bien++; return; }
    mal++;
    IMPRIMIR("MAL: %s (%d,%d): %d %d %d, se esperaba %d %d %d\n", que, x, y, p[0], p[1], p[2], r, g, b);
}
static int erro_es(GLenum e, const char *que)
{
    GLenum g = glGetError();
    if (g == e) { bien++; return 1; }
    mal++;
    IMPRIMIR("MAL: %s: error 0x%x, se esperaba 0x%x\n", que, g, e);
    return 0;
}

/* ------------------------------------------------------------ sombreadores */
static GLuint programa(const char *vs, const char *fs)
{
    GLuint p = glCreateProgram(), v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &vs, 0); glCompileShader(v);
    glShaderSource(f, 1, &fs, 0); glCompileShader(f);
    glAttachShader(p, v); glAttachShader(p, f);
    glBindAttribLocation(p, 0, "a_pos");
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) IMPRIMIR("  (no enlaza)\n");
    return p;
}
static const char *VS_QUAD =
    "attribute vec2 a_pos;\n"
    "uniform float u_z;\n"
    "varying vec2 v_tc;\n"
    "void main() { v_tc = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos, u_z, 1.0); }\n";
static void quad_em(GLuint p, float x0, float y0, float x1, float y1, float z)
{
    float q[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    glUseProgram(p);
    GLint lz = glGetUniformLocation(p, "u_z");
    if (lz >= 0) glUniform1f(lz, z);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, q);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
static GLuint p_cor, p_prof;
static void quad_cor(float x0, float y0, float x1, float y1, float z, float r, float g, float b, float a)
{
    glUseProgram(p_cor);
    glUniform4f(glGetUniformLocation(p_cor, "u_cor"), r, g, b, a);
    quad_em(p_cor, x0, y0, x1, y1, z);
}
/* la textura de profundidad de la unidad 0 (muestreada con texture2D, que da (d, d, d, 1)) a la pantalla */
static void quad_prof(void) { glUseProgram(p_prof); glUniform1i(glGetUniformLocation(p_prof, "u_t"), 0); quad_em(p_prof, -1, -1, 1, 1, 0); }

static GLuint tex_cor(void)
{
    GLuint t;
    glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return t;
}
static GLuint tex_prof(GLenum fmt, GLenum tipo, const void *dados, int l, int a)
{
    GLuint t;
    glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, fmt, l, a, 0, fmt, tipo, dados);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

/* ------------------------------------------------------------ EGL */
static EGLDisplay dpy;
static EGLSurface sup;
static EGLContext ctx;
static int egl(void)
{
    EGLint n = 0, v = 0; EGLConfig cfg;
    const EGLint at[] = { EGL_DEPTH_SIZE, 24, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    const EGLint at8[] = { EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    const EGLint pb[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
    const EGLint cx[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, 0, 0)) return 0;
    ver(eglChooseConfig(dpy, at, &cfg, 1, &n) && n >= 1, "EGL: hay una configuración con Z de 24 bits y pbuffer");
    ver(eglGetConfigAttrib(dpy, cfg, EGL_DEPTH_SIZE, &v) && v >= 24, "EGL: la configuración elegida tiene EGL_DEPTH_SIZE >= 24");
    ver(eglChooseConfig(dpy, at8, &cfg, 1, &n) && n >= 1, "EGL: hay una configuración con Z de 24 bits, esténcil de 8 y pbuffer");
    if (n < 1) return 0;
    sup = eglCreatePbufferSurface(dpy, cfg, pb);
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cx);
    if (sup == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, sup, sup, ctx)) return 0;
    glGetIntegerv(GL_DEPTH_BITS, &v);
    ver(v == 24, "GL_DEPTH_BITS de la superficie de 24 bits");
    return 1;
}

/* ------------------------------------------------------------ las extensiones anunciadas */
static void t_extensoes(void)
{
    const char *s = (const char *)glGetString(GL_EXTENSIONS);
    ver(s && strstr(s, "GL_OES_depth_texture"), "GL_EXTENSIONS: GL_OES_depth_texture");
    ver(s && strstr(s, "GL_OES_mapbuffer"), "GL_EXTENSIONS: GL_OES_mapbuffer");
    ver(s && strstr(s, "GL_OES_depth24"), "GL_EXTENSIONS: GL_OES_depth24");
    ver(s && strstr(s, "GL_OES_packed_depth_stencil"), "GL_EXTENSIONS: GL_OES_packed_depth_stencil");
    ver(eglGetProcAddress("glMapBufferOES") != 0 && eglGetProcAddress("glUnmapBufferOES") != 0 && eglGetProcAddress("glGetBufferPointervOES") != 0,
        "eglGetProcAddress conoce glMapBufferOES, glUnmapBufferOES y glGetBufferPointervOES");
}

/* ------------------------------------------------------------ GL_OES_depth24 */
/* tres rectángulos con Z (rojo lejos, verde en medio, azul cerca a la derecha) en el FBO que esté ligado (64x64) */
static void cena_z(void)
{
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1); glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    quad_cor(-1, -1, 1, 1, 0.5f, 1, 0, 0, 1);        /* rojo, el más lejos */
    quad_cor(-1, -1, 0.5f, 1, 0.0f, 0, 1, 0, 1);     /* verde delante del rojo, salvo a la derecha */
    quad_cor(0, -1, 1, 0, -0.5f, 0, 0, 1, 1);        /* azul delante de todo, abajo a la derecha */
    quad_cor(-1, 0.5f, 1, 1, 0.9f, 1, 1, 0, 1);      /* amarillo detrás del rojo: no se ve */
    glDisable(GL_DEPTH_TEST);
}
static void comprobar_z(const char *que)
{
    ler_quadro();
    verpx(8, 40, 0, 255, 0, 1, "Z: el verde tapa al rojo");
    verpx(60, 40, 255, 0, 0, 1, "Z: el rojo donde no llega el verde");
    verpx(48, 16, 0, 0, 255, 1, "Z: el azul está delante de todo");
    verpx(60, 60, 255, 0, 0, 1, "Z: el amarillo, detrás del rojo, no se ve");
    (void)que;
}
static void t_depth24(void)
{
    GLuint fb, rb, cor_rb, tc;
    GLint v = 0;
    glGenFramebuffers(1, &fb); glBindFramebuffer(GL_FRAMEBUFFER, fb);
    tc = tex_cor();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tc, 0);
    glGenRenderbuffers(1, &rb); glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, W, H);
    ver(glGetError() == GL_NO_ERROR, "glRenderbufferStorage(GL_DEPTH_COMPONENT24_OES)");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &v); ver(v == 24, "DEPTH_COMPONENT24: GL_RENDERBUFFER_DEPTH_SIZE = 24");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &v); ver(v == GL_DEPTH_COMPONENT24_OES, "DEPTH_COMPONENT24: GL_RENDERBUFFER_INTERNAL_FORMAT");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &v); ver(v == 0, "DEPTH_COMPONENT24: sin esténcil");
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo (textura RGBA + Z de 24 en un renderbuffer)");
    cena_z(); comprobar_z("depth24");
    /* renderbuffer de color y de Z de 24 a la vez, como el de glmark2 --off-screen */
    glGenRenderbuffers(1, &cor_rb); glBindRenderbuffer(GL_RENDERBUFFER, cor_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8_OES, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cor_rb);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo (renderbuffers RGB8 + Z de 24)");
    cena_z(); comprobar_z("depth24 rb");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb); glDeleteRenderbuffers(1, &rb); glDeleteRenderbuffers(1, &cor_rb); glDeleteTextures(1, &tc);
    ver(glGetError() == GL_NO_ERROR, "depth24: sin errores pendientes");
}

/* ------------------------------------------------------------ GL_OES_packed_depth_stencil */
/* esténcil: la mitad izquierda escribe 1 (rojo) y después el verde solo pasa donde hay 1; el FBO ligado es de 64x64 */
static void cena_est(void)
{
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1); glClearDepthf(1.0f); glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    quad_cor(-1, -1, 0, 1, 0.0f, 1, 0, 0, 1);
    glStencilFunc(GL_EQUAL, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    quad_cor(-1, -1, 1, 1, -0.2f, 0, 1, 0, 1);
    glDisable(GL_STENCIL_TEST); glDisable(GL_DEPTH_TEST);
}
static void comprobar_est(void)
{
    ler_quadro();
    verpx(10, 30, 0, 255, 0, 1, "esténcil: el verde pasa donde el rojo escribió 1");
    verpx(50, 30, 0, 0, 0, 0, "esténcil: el verde no pasa donde el esténcil es 0");
}
static void t_packed(void)
{
    GLuint fb, rb, tc;
    GLint v = 0;
    glGenFramebuffers(1, &fb); glBindFramebuffer(GL_FRAMEBUFFER, fb);
    tc = tex_cor();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tc, 0);
    glGenRenderbuffers(1, &rb); glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, W, H);
    ver(glGetError() == GL_NO_ERROR, "glRenderbufferStorage(GL_DEPTH24_STENCIL8_OES)");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &v); ver(v == 24, "DEPTH24_STENCIL8: profundidad de 24");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &v); ver(v == 8, "DEPTH24_STENCIL8: esténcil de 8");
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &v); ver(v == GL_DEPTH24_STENCIL8_OES, "DEPTH24_STENCIL8: formato interno");
    /* anexado a los dos puntos por separado */
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo (renderbuffer DEPTH24_STENCIL8 en profundidad y esténcil)");
    cena_est(); comprobar_est();
    cena_z(); comprobar_z("packed rb");
    /* y con GL_DEPTH_STENCIL_ATTACHMENT, de una vez */
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    ver(glGetError() == GL_NO_ERROR && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo con GL_DEPTH_STENCIL_ATTACHMENT");
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    ver(v == (GLint)rb, "GL_DEPTH_STENCIL_ATTACHMENT anexa también el esténcil");
    cena_est(); comprobar_est();
    /* un renderbuffer solo de profundidad no vale como esténcil */
    GLuint rd; glGenRenderbuffers(1, &rd); glBindRenderbuffer(GL_RENDERBUFFER, rd);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rd);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "un renderbuffer DEPTH_COMPONENT24 como esténcil: INCOMPLETE_ATTACHMENT");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb); glDeleteRenderbuffers(1, &rb); glDeleteRenderbuffers(1, &rd); glDeleteTextures(1, &tc);
    ver(glGetError() == GL_NO_ERROR, "packed (renderbuffer): sin errores pendientes");
}

/* ------------------------------------------------------------ GL_OES_depth_texture */
static void t_depth_texture(void)
{
    GLint v = 0;
    glClearColor(0, 0, 0, 1);
    /* 1. el FBO con la textura de profundidad (UNSIGNED_INT) y una textura de color */
    GLuint fb, tc, td;
    glGenFramebuffers(1, &fb); glBindFramebuffer(GL_FRAMEBUFFER, fb);
    tc = tex_cor();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tc, 0);
    td = tex_prof(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0, W, H);
    ver(glGetError() == GL_NO_ERROR, "glTexImage2D(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT) sin datos");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, td, 0);
    ver(glGetError() == GL_NO_ERROR && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo (color RGBA + textura de profundidad UNSIGNED_INT)");
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &v);
    ver(v == GL_TEXTURE, "el anexo de profundidad es una textura");
    /* el fondo (profundidad 1.0), un rectángulo a profundidad 0.5 (z de recorte 0.0) y otro a 0.25 (z de recorte -0.5) */
    glViewport(0, 0, W, H);
    glClearDepthf(1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    quad_cor(-1, -1, 0.5f, 1, 0.0f, 1, 0, 0, 1);
    quad_cor(0, -1, 0.25f, 1, -0.5f, 0, 1, 0, 1);
    glDisable(GL_DEPTH_TEST);
    ler_quadro();
    verpx(4, 30, 255, 0, 0, 1, "textura de profundidad: el color también se dibuja");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* 2. muestrearla: (d, d, d, 1); sin comparación de hardware (ES 2.0) */
    glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, td);
    quad_prof();
    ler_quadro();
    verpx(4, 30, 128, 128, 128, 2, "profundidad 0.5 muestreada en el sombreador");
    verpx(36, 30, 64, 64, 64, 2, "profundidad 0.25 muestreada en el sombreador");
    verpx(60, 30, 255, 255, 255, 0, "profundidad 1.0 (el fondo) muestreada en el sombreador");
    /* 3. comparación manual en GLSL: «sombra» donde la profundidad guardada es menor que la del fragmento */
    GLuint ps = programa(VS_QUAD,
        "precision mediump float; uniform sampler2D u_t; varying vec2 v_tc;\n"
        "void main() { float d = texture2D(u_t, v_tc).r; gl_FragColor = d < 0.4 ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(0.0, 1.0, 1.0, 1.0); }\n");
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ps); glUniform1i(glGetUniformLocation(ps, "u_t"), 0);
    quad_em(ps, -1, -1, 1, 1, 0);
    ler_quadro();
    verpx(4, 30, 0, 255, 255, 0, "comparación manual: 0.5 no es menor que 0.4");
    verpx(36, 30, 255, 0, 255, 0, "comparación manual: 0.25 es menor que 0.4");
    glDeleteProgram(ps);
    /* 4. un FBO solo con profundidad (UNSIGNED_SHORT), como el de la escena shadow de glmark2 */
    GLuint fd, t16;
    glGenFramebuffers(1, &fd); glBindFramebuffer(GL_FRAMEBUFFER, fd);
    t16 = tex_prof(GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0, W, H);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, t16, 0);
    ver(glGetError() == GL_NO_ERROR && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO solo con textura de profundidad UNSIGNED_SHORT: completo");
    glViewport(0, 0, W, H);
    glClear(GL_DEPTH_BUFFER_BIT);
    ver(glGetError() == GL_NO_ERROR, "glClear de un FBO sin color");
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    quad_cor(-1, -1, 1, 0, 0.0f, 1, 1, 1, 1);          /* mitad de abajo a 0.5 */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, t16);
    quad_prof();
    ler_quadro();
    verpx(30, 10, 128, 128, 128, 2, "FBO solo de profundidad: 0.5 donde se dibujó");
    verpx(30, 50, 255, 255, 255, 0, "FBO solo de profundidad: 1.0 donde no");
    glDeleteFramebuffers(1, &fd); glDeleteTextures(1, &t16);
    /* 5. con datos: una rampa de 16 bits de 4x4 y un valor de 32 bits */
    unsigned short rampa[16];
    for (int i = 0; i < 16; i++) rampa[i] = (unsigned short)(i * 4369);
    GLuint tr = tex_prof(GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, rampa, 4, 4);
    ver(glGetError() == GL_NO_ERROR, "glTexImage2D(GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT) con datos");
    glClear(GL_COLOR_BUFFER_BIT);
    quad_prof();
    ler_quadro();
    verpx(2, 2, 0, 0, 0, 1, "rampa de 16 bits: el texel 0 (abajo a la izquierda)");
    verpx(W - 2, 2, 51, 51, 51, 2, "rampa de 16 bits: el texel 3");
    verpx(W - 2, H - 2, 255, 255, 255, 1, "rampa de 16 bits: el texel 15");
    unsigned medio = 0x80000000u;
    unsigned dados32[1] = { medio };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, dados32);
    glClear(GL_COLOR_BUFFER_BIT);
    quad_prof();
    ler_quadro();
    verpx(30, 30, 128, 128, 128, 2, "texel de 32 bits 0x80000000 muestrea 0.5");
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, (const unsigned[]){ 0xFFFFFFFFu });
    glClear(GL_COLOR_BUFFER_BIT);
    quad_prof();
    ler_quadro();
    verpx(30, 30, 255, 255, 255, 0, "glTexSubImage2D de profundidad: 0xFFFFFFFF muestrea 1.0");
    /* 6. filtrado lineal permitido */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ver(glGetError() == GL_NO_ERROR, "filtro lineal en una textura de profundidad");
    glDeleteTextures(1, &tr);
    /* 7. errores */
    GLuint te; glGenTextures(1, &te); glBindTexture(GL_TEXTURE_2D, te);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
    erro_es(GL_INVALID_OPERATION, "DEPTH_COMPONENT con UNSIGNED_BYTE: GL_INVALID_OPERATION");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    erro_es(GL_INVALID_OPERATION, "formato interno distinto del formato: GL_INVALID_OPERATION");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    glGenerateMipmap(GL_TEXTURE_2D);
    erro_es(GL_INVALID_OPERATION, "glGenerateMipmap de una textura de profundidad: GL_INVALID_OPERATION");
    GLuint tcubo; glGenTextures(1, &tcubo); glBindTexture(GL_TEXTURE_CUBE_MAP, tcubo);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    erro_es(GL_INVALID_OPERATION, "profundidad en un cubo: GL_INVALID_OPERATION");
    glDeleteTextures(1, &tcubo);
    /* una textura de profundidad en el punto de color, y una de color en el de profundidad: incompletos */
    glGenFramebuffers(1, &fd); glBindFramebuffer(GL_FRAMEBUFFER, fd);
    glBindTexture(GL_TEXTURE_2D, te);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, te, 0);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "textura de profundidad como color: INCOMPLETE_ATTACHMENT");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tc, 0);
    ver(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "textura de color como profundidad: INCOMPLETE_ATTACHMENT");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fd);
    /* una textura de profundidad sin completar (sin datos) da negro opaco al muestrearla */
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glDeleteFramebuffers(1, &fb); glDeleteTextures(1, &td); glDeleteTextures(1, &tc); glDeleteTextures(1, &te);
    ver(glGetError() == GL_NO_ERROR, "textura de profundidad: sin errores pendientes");
}

/* ------------------------------------------------------------ GL_OES_packed_depth_stencil en texturas */
static void t_packed_textura(void)
{
    GLuint fb, tc, tp;
    GLint v = 0;
    glGenFramebuffers(1, &fb); glBindFramebuffer(GL_FRAMEBUFFER, fb);
    tc = tex_cor();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tc, 0);
    tp = tex_prof(GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, 0, W, H);
    ver(glGetError() == GL_NO_ERROR, "glTexImage2D(GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES)");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tp, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tp, 0);
    ver(glGetError() == GL_NO_ERROR && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO completo (color + textura DEPTH_STENCIL en profundidad y esténcil)");
    cena_est(); comprobar_est();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, tp);
    quad_prof();       /* el esténcil no se muestrea; la profundidad es 1.0 donde no se escribió (el fondo) y 0.5 donde se dibujó con z = 0 */
    ler_quadro();
    verpx(10, 30, 102, 102, 102, 2, "textura DEPTH_STENCIL muestreada: la profundidad del verde (z de recorte -0.2 = 0.4)");
    verpx(50, 30, 255, 255, 255, 0, "textura DEPTH_STENCIL muestreada: el fondo es 1.0");
    /* con datos: profundidad 0x800000 (0.5) y esténcil 7 */
    unsigned dado = (0x800000u << 8) | 7u;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL_OES, 1, 1, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES, &dado);
    glClear(GL_COLOR_BUFFER_BIT);
    quad_prof();
    ler_quadro();
    verpx(30, 30, 128, 128, 128, 2, "DEPTH_STENCIL con datos: profundidad 0.5");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL_OES, 4, 4, 0, GL_DEPTH_STENCIL_OES, GL_UNSIGNED_SHORT, 0);
    erro_es(GL_INVALID_OPERATION, "DEPTH_STENCIL con un tipo que no es UNSIGNED_INT_24_8: GL_INVALID_OPERATION");
    glDeleteFramebuffers(1, &fb); glDeleteTextures(1, &tp); glDeleteTextures(1, &tc);
    ver(glGetError() == GL_NO_ERROR, "packed (textura): sin errores pendientes");
}

/* ------------------------------------------------------------ GL_OES_mapbuffer */
static const float TRI_A[8] = { -1, -1, 1, -1, 1, 1, -1, 1 };
static void cena_mapa(void)
{
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    GLuint b;
    glGenBuffers(1, &b); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 2 * sizeof TRI_A, 0, GL_DYNAMIC_DRAW);
    float *p = (float *)glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    if (p) {
        for (int i = 0; i < 8; i++) p[i] = TRI_A[i] * 0.5f;                    /* el cuadrado de en medio... */
        for (int i = 0; i < 8; i++) p[8 + i] = TRI_A[i] * 0.25f;               /* ...y uno más pequeño */
        glUnmapBufferOES(GL_ARRAY_BUFFER);
    }
    glUseProgram(p_cor);
    glUniform4f(glGetUniformLocation(p_cor, "u_cor"), 1, 0.5f, 0, 1);
    glUniform1f(glGetUniformLocation(p_cor, "u_z"), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glUniform4f(glGetUniformLocation(p_cor, "u_cor"), 0, 0.5f, 1, 1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void *)(8 * sizeof(float)));
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}
static void t_mapbuffer(void)
{
    GLuint b; GLint v = -1; void *p = 0, *q = 0;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ver(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES) == 0, "glMapBufferOES sin búfer ligado: NULL");
    erro_es(GL_INVALID_OPERATION, "glMapBufferOES sin búfer ligado: GL_INVALID_OPERATION");
    glGenBuffers(1, &b); glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, 0, GL_DYNAMIC_DRAW);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED_OES, &v); ver(v == 0, "GL_BUFFER_MAPPED_OES = 0 antes de mapear");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_ACCESS_OES, &v); ver(v == GL_WRITE_ONLY_OES, "GL_BUFFER_ACCESS_OES = GL_WRITE_ONLY_OES");
    ver(glMapBufferOES(GL_ARRAY_BUFFER, 0x88B8 /* READ_ONLY */) == 0, "glMapBufferOES de solo lectura: NULL");
    erro_es(GL_INVALID_ENUM, "glMapBufferOES con un acceso que no es de escritura: GL_INVALID_ENUM");
    p = glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    ver(p != 0, "glMapBufferOES devuelve un puntero");
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED_OES, &v); ver(v == 1, "GL_BUFFER_MAPPED_OES = 1 mapeado");
    glGetBufferPointervOES(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER_OES, &q); ver(q == p && p != 0, "glGetBufferPointervOES da el mismo puntero");
    ver(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES) == 0, "mapear dos veces: NULL");
    erro_es(GL_INVALID_OPERATION, "mapear dos veces: GL_INVALID_OPERATION");
    glBufferSubData(GL_ARRAY_BUFFER, 0, 4, TRI_A);
    erro_es(GL_INVALID_OPERATION, "glBufferSubData con el búfer mapeado: GL_INVALID_OPERATION");
    if (p) { for (int i = 0; i < 16; i++) ((unsigned char *)p)[i] = (unsigned char)(i + 1); }
    ver(glUnmapBufferOES(GL_ARRAY_BUFFER) == GL_TRUE, "glUnmapBufferOES devuelve GL_TRUE");
    glGetBufferPointervOES(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER_OES, &q); ver(q == 0, "desmapeado: el puntero es NULL");
    ver(glUnmapBufferOES(GL_ARRAY_BUFFER) == GL_FALSE, "desmapear de nuevo: GL_FALSE");
    erro_es(GL_INVALID_OPERATION, "desmapear de nuevo: GL_INVALID_OPERATION");
    /* lo escrito queda en el búfer: se vuelve a mapear y se lee por el puntero (lo permite este almacén; la extensión solo define escribir) */
    p = glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    ver(p && ((unsigned char *)p)[0] == 1 && ((unsigned char *)p)[15] == 16, "lo escrito por el puntero se conserva");
    glBufferData(GL_ARRAY_BUFFER, 32, 0, GL_STATIC_DRAW);
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED_OES, &v); ver(v == 0, "glBufferData desmapea");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    p = glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    ver(p != 0 && glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER) == GL_TRUE, "también para GL_ELEMENT_ARRAY_BUFFER");
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glMapBufferOES(GL_TEXTURE_2D, GL_WRITE_ONLY_OES);
    erro_es(GL_INVALID_ENUM, "glMapBufferOES con un blanco que no es de búfer: GL_INVALID_ENUM");
    glDeleteBuffers(1, &b);
    /* dibujar con lo escrito por el puntero */
    cena_mapa();
    ler_quadro();
    verpx(W / 2 - 4, H / 2, 0, 128, 255, 1, "dibujo con búfer mapeado: el cuadrado pequeño (azul)");
    verpx(W / 2 - 14, H / 2, 255, 128, 0, 1, "dibujo con búfer mapeado: el cuadrado grande (naranja)");
    verpx(4, 4, 0, 0, 0, 0, "dibujo con búfer mapeado: el fondo");
    ver(glGetError() == GL_NO_ERROR, "mapbuffer: sin errores pendientes");
}

/* ------------------------------------------------------------ la GPU contra la CPU */
static unsigned char ima[W * H * 4], imb[W * H * 4];
/* la escena por la CPU y, si hay GPU, por la GPU: píxeles que difieren (más de 8 en algún canal), y cuántos dibujos hizo cada una */
static void comparar(void (*cena)(void), const char *nome, int fbo_a_ligar, int para_soma)
{
    unsigned d0 = 0, l0 = 0, c0 = 0, d1 = 0, l1 = 0, c1 = 0;
    (void)fbo_a_ligar;
    sgl2_gpu_modo(0);
    cena();
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, ima);
    if (para_soma) for (int i = 0; i < W * H * 4; i++) { soma ^= ima[i]; soma *= 16777619u; }
    if (solo_cpu) { IMPRIMIR("gpu-vs-cpu %s: 0 pixeles distintos, gpu 0, cpu 0\n", nome); return; }
    sgl2_gpu_modo(1);
    sgl2_gpu_contadores(&d0, &l0, &c0);
    cena();
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, imb);
    sgl2_gpu_contadores(&d1, &l1, &c1);
    int dist = 0;
    for (int i = 0; i < W * H; i++) {
        int m = 0;
        for (int k = 0; k < 3; k++) { int x = ima[i * 4 + k] - imb[i * 4 + k]; if (x < 0) x = -x; if (x > m) m = x; }
        if (m > 8) dist++;
    }
    IMPRIMIR("gpu-vs-cpu %s: %d pixeles distintos, gpu %u, cpu %u\n", nome, dist, d1 - d0, c1 - c0);
    char txt[120] = "la GPU da lo mismo que la CPU: ";
    int n = 0; while (txt[n]) n++;
    for (int i = 0; nome[i] && n < 110; i++) txt[n++] = nome[i];
    txt[n] = 0;
    ver(dist <= W * H / 20, txt);
}

static GLuint fb_c, tc_c, rb_c;
static void liga_fbo_z24(void) { glBindFramebuffer(GL_FRAMEBUFFER, fb_c); }
static void cena_gpu_z24(void) { liga_fbo_z24(); cena_z(); }
static void cena_gpu_est(void) { liga_fbo_z24(); cena_est(); }
static void cena_gpu_textura(void)
{
    /* profundidad en una textura (por la CPU) y, después, esa textura muestreada en pantalla (también por la CPU) */
    GLuint f, c, d;
    glGenFramebuffers(1, &f); glBindFramebuffer(GL_FRAMEBUFFER, f);
    c = tex_cor(); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c, 0);
    d = tex_prof(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0, W, H); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, d, 0);
    cena_z();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, d);
    quad_prof();
    glDeleteFramebuffers(1, &f); glDeleteTextures(1, &c); glDeleteTextures(1, &d);
}
static void t_gpu_vs_cpu(void)
{
    glGenFramebuffers(1, &fb_c); glBindFramebuffer(GL_FRAMEBUFFER, fb_c);
    tc_c = tex_cor();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tc_c, 0);
    glGenRenderbuffers(1, &rb_c); glBindRenderbuffer(GL_RENDERBUFFER, rb_c);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb_c);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb_c);
    comparar(cena_gpu_z24, "depth24_stencil8_z", 0, 1);
    comparar(cena_gpu_est, "depth24_stencil8_estencil", 0, 1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24_OES, W, H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb_c);
    comparar(cena_gpu_z24, "depth24", 0, 1);
    comparar(cena_gpu_textura, "depth_texture", 0, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    comparar(cena_mapa, "mapbuffer", 0, 1);
    glDeleteFramebuffers(1, &fb_c); glDeleteRenderbuffers(1, &rb_c); glDeleteTextures(1, &tc_c);
}

static int rodar(void)
{
    if (!egl()) { IMPRIMIR("conformidad: sin contexto de OpenGL ES 2.0 con Z de 24 bits\n"); return 1; }
    p_cor = programa(VS_QUAD, "precision mediump float; uniform vec4 u_cor; void main() { gl_FragColor = u_cor; }\n");
    p_prof = programa(VS_QUAD, "precision mediump float; uniform sampler2D u_t; varying vec2 v_tc;\n"
                      "void main() { vec4 t = texture2D(u_t, v_tc); gl_FragColor = vec4(t.rgb, 1.0); }\n");
    t_extensoes();
    t_depth24();
    t_packed();
    t_depth_texture();
    t_packed_textura();
    t_mapbuffer();
    t_gpu_vs_cpu();
    IMPRIMIR("conformidad: %d bien, %d mal\n", bien, mal);
    IMPRIMIR("suma %08x\n", soma);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx); eglDestroySurface(dpy, sup); eglTerminate(dpy);
    return mal ? 1 : 0;
}
static int rodar_gpu(int cpu)
{
    solo_cpu = cpu;
    if (cpu) sgl2_gpu_modo(0);
    int r = rodar();
    unsigned d = 0, l = 0, c = 0;
    sgl2_gpu_contadores(&d, &l, &c);
    IMPRIMIR("gpu: estado %d, %u dibujos y %u limpiezas por la GPU, %u dibujos que la GPU devolvio a la CPU\n", sgl2_gpu_estado(), d, l, c);
    return r;
}

#if defined(SGL_ANFITRIAO)
int main(int argc, char **argv) { return rodar_gpu(argc > 1 && !strcmp(argv[1], "--cpu")); }
#else
int principal(int argc, char **argv) { return rodar_gpu(argc > 1 && !strcmp(argv[1], "--cpu")); }
#endif
