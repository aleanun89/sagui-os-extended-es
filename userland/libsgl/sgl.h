// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl.h - SaguiGL (mejora 054, v5): subconjunto de OpenGL ES 1.1 por software.
 *
 * Biblioteca de usuario `libsgl`. Sin texturas, luces, niebla ni mezcla (v6).
 * El motor (sgl.c) es determinista y no usa libc ni libm: compila igual en
 * el anfitrión (pruebas de píxeles, tests/h77_sgl.py) y en el SaguiOS.
 * Usa coma flotante con SSE2 (el programa debe comprobar el CPUID antes). */
#ifndef SAGUI_SGL_H
#define SAGUI_SGL_H

#ifdef SGL_ANFITRIAO
#include <stdint.h>
#elif defined(SGL_POR_CC)
/* Compilado por el cc de dentro del SaguiOS (el ejemplo /ejemplos/sagui_gl):
 * el cc ya trae los tipos enteros con nombre (D127) y el <stdint.h> lo
 * ignora. */
#include <stdint.h>
#else
#include "sagui.h"
#endif

typedef float    GLfloat;
typedef int      GLint;
typedef unsigned GLenum;
typedef int      GLsizei;
typedef unsigned GLbitfield;
typedef unsigned GLuint;

#define GL_TRUE  1
#define GL_FALSE 0

#define GL_MODELVIEW  0x1700
#define GL_PROJECTION 0x1701

#define GL_POINTS         0
#define GL_LINES          1
#define GL_LINE_LOOP      2
#define GL_LINE_STRIP     3
#define GL_TRIANGLES      4
#define GL_TRIANGLE_STRIP 5
#define GL_TRIANGLE_FAN   6

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100

#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE  0x0B44
#define GL_FRONT      0x0404
#define GL_BACK       0x0405
#define GL_CW         0x0900
#define GL_CCW        0x0901
#define GL_FLAT       0x1D00
#define GL_SMOOTH     0x1D01
#define GL_LESS       0x0201
#define GL_LEQUAL     0x0203
#define GL_ALWAYS     0x0207

#define GL_VERTEX_ARRAY 0x8074
#define GL_COLOR_ARRAY  0x8076

/* ---- v6 (mejora 054 parte A): texturas, mezcla, niebla y luces ---- */
#define GL_TEXTURE_2D 0x0DE1
#define GL_BLEND      0x0BE2
#define GL_FOG        0x0B60
#define GL_LIGHTING   0x0B50
#define GL_LIGHT0     0x4000

#define GL_SRC_ALPHA           0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE   1
#define GL_ZERO  0

#define GL_RGB  0x1907
#define GL_RGBA 0x1908

#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S     0x2802
#define GL_TEXTURE_WRAP_T     0x2803
#define GL_NEAREST      0x2600
#define GL_LINEAR       0x2601
#define GL_REPEAT       0x2901
#define GL_CLAMP_TO_EDGE 0x812F

#define GL_FOG_START 0x0B63
#define GL_FOG_END   0x0B64
#define GL_FOG_COLOR 0x0B66

#define GL_LIGHT_POSITION 0x0B03
#define GL_LIGHT_DIFFUSE  0x0B22
#define GL_LIGHT_AMBIENT  0x0B00

#define GL_TEXTURE_COORD_ARRAY 0x8078

#define GL_UNSIGNED_BYTE  0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_FLOAT          0x1406

#define GL_NO_ERROR          0
#define GL_INVALID_ENUM      0x0500
#define GL_INVALID_VALUE     0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW    0x0503
#define GL_STACK_UNDERFLOW   0x0504
#define GL_OUT_OF_MEMORY     0x0505

struct jan_s;

typedef struct sgl_ctx sgl_ctx_t;

/* ---- contexto ---- */
/* Contexto ligado a una ventana (jan_t, de gui.h): dibuja en su área de píxeles.
 * zbits = 16 o 32. Queda como contexto actual. */
sgl_ctx_t *sgl_criar_contexto(void *janela, int zbits);
/* Contexto en memoria propia (pruebas, desplazamiento fuera de pantalla):
 * px = larg*alt píxeles 0x00RRGGBB. */
sgl_ctx_t *sgl_criar_contexto_mem(uint32_t *px, int larg, int alt, int zbits);
void       sgl_destruir_contexto(sgl_ctx_t *c);
void       sgl_usar_contexto(sgl_ctx_t *c);
void       sgl_redimensionar(sgl_ctx_t *c);   /* tras cambiar el tamaño de la ventana */
void       sgl_trocar(void);                   /* muestra el cuadro (ventana) */
uint32_t   sgl_soma(const uint32_t *px, int n); /* suma FNV-1a, para las pruebas */

/* ---- estado ---- */
void   glMatrixMode(GLenum modo);
void   glLoadIdentity(void);
void   glLoadMatrixf(const GLfloat *m);   /* 16 valores, por columnas */
void   glMultMatrixf(const GLfloat *m);
void   glPushMatrix(void);
void   glPopMatrix(void);
void   glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void   glRotatef(GLfloat graus, GLfloat x, GLfloat y, GLfloat z);
void   glScalef(GLfloat x, GLfloat y, GLfloat z);
void   glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
void   glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f);
void   glViewport(GLint x, GLint y, GLsizei larg, GLsizei alt);
void   glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void   glClear(GLbitfield mascara);
void   glEnable(GLenum cap);
void   glDisable(GLenum cap);
void   glCullFace(GLenum face);
void   glFrontFace(GLenum modo);
void   glShadeModel(GLenum modo);
void   glDepthFunc(GLenum f);
GLenum glGetError(void);

/* ---- vértices ---- */
void glEnableClientState(GLenum matriz);
void glDisableClientState(GLenum matriz);
void glVertexPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p);   /* GL_FLOAT, tam 2..4 */
void glColorPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p);    /* GL_FLOAT o GL_UNSIGNED_BYTE, tam 3..4 */
void glDrawArrays(GLenum modo, GLint primeiro, GLsizei n);
void glDrawElements(GLenum modo, GLsizei n, GLenum tipo, const void *indices); /* GL_UNSIGNED_BYTE o SHORT */
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glBegin(GLenum modo);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex2f(GLfloat x, GLfloat y);
void glEnd(void);

/* ---- v6: texturas ----
 * Texturas RGBA de 8 bits, filtrado del vecino más próximo (GL_NEAREST) y
 * repetición (GL_REPEAT) o borde (GL_CLAMP_TO_EDGE). */
void glGenTextures(GLsizei n, GLuint *ids);
void glBindTexture(GLenum alvo, GLuint id);
void glTexImage2D(GLenum alvo, GLint nivel, GLint formato_interno, GLsizei larg,
                   GLsizei alt, GLint bord, GLenum formato, GLenum tipo, const void *dados);
void glTexParameteri(GLenum alvo, GLenum nome, GLint valor);
void glDeleteTextures(GLsizei n, const GLuint *ids);
void glTexCoord2f(GLfloat u, GLfloat v);
void glTexCoordPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p);

/* ---- v6: mezcla, niebla y luces ---- */
void glBlendFunc(GLenum src, GLenum dst);
void glFogf(GLenum nome, GLfloat valor);
void glFogfv(GLenum nome, const GLfloat *valor);
void glLightfv(GLenum luz, GLenum nome, const GLfloat *valor);

/* ---- v6: motor de sombreado ----
 * 1 = ruta SIMD (SSE2, cuatro píxeles a la vez, si el compilador la tiene),
 * 0 = ruta de referencia (un píxel a la vez). Las dos dan exactamente el
 * mismo píxel; la de referencia sirve para comparar (prueba h101). */
void sgl_motor_simd(int ativo);
int  sgl_motor_simd_ativo(void);

/* ---- v8.3: 3D por la GPU (D285) ----
 * Si el kernel tiene un driver 3D para la tarjeta (Intel Gen3...) y el motor
 * está libre, un contexto de ventana manda sus triángulos a la GPU; si no, o
 * si la GPU falla, sigue por software sin que el programa haga nada. Con la
 * GPU el cuadro llega a los píxeles de la ventana en sgl_trocar() o en
 * glFinish(): quien dibuje encima con la CPU (texto, HUD) llama antes a
 * glFinish(). Los contextos en memoria (sgl_criar_contexto_mem) son siempre
 * por software salvo que se pida con sgl_gpu_ligar(). La GPU no da los mismos
 * píxeles bit a bit que el motor por software (bordes, redondeo). */
int  sgl_gpu_ligar(sgl_ctx_t *c);      /* 1 = el contexto dibuja por la GPU; 0 = sigue por software */
void sgl_gpu_desligar(sgl_ctx_t *c);
int  sgl_gpu_ativa(void);              /* el contexto actual, ahora */
void sgl_gpu_automatica(int ativa);    /* 1 (por defecto): los contextos de ventana prueban la GPU */
void glFinish(void);
void glFlush(void);

/* ---- utilidades deterministas (sin libm) ---- */
float sgl_sen(float rad);
float sgl_cos(float rad);

#endif
