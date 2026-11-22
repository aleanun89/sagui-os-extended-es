// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl.h - SaguiGL (mejora 054, v5): subconjunto de OpenGL ES 1.1 por software.
 *
 * Biblioteca de usuario `libsgl`. v6: texturas, luces, niebla y mezcla;
 * D404: filtrado bilineal y mipmaps, perspectiva, luces por vértice.
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

/* D425: fuera del cc de dentro (que compila solo el motor de siempre en
 * /ejemplos/sagui_gl), SaguiGL es OpenGL ES 1.1 completo: los nombres, tipos
 * y funciones son los de las cabeceras estándar (userland/inc/GLES). Las
 * definiciones de abajo solo valen cuando no están (SGL_POR_CC). */
#ifndef SGL_POR_CC
#include "../inc/GLES/gl.h"
#include "../inc/GLES/glext.h"
#define SGL_ES 1
#endif

#ifndef __gles1_gl_h_
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

#endif /* __gles1_gl_h_ */

/* Nombres propios de SaguiGL v6 (no son de GL ES): glLightfv(GL_LIGHT0, ...). */
#define GL_LIGHT_POSITION 0x0B03
#define GL_LIGHT_DIFFUSE  0x0B22
#define GL_LIGHT_AMBIENT  0x0B00

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
sgl_ctx_t *sgl_contexto_atual(void);   /* D426: el contexto actual (el de EGL también) */
void       sgl_redimensionar(sgl_ctx_t *c);   /* tras cambiar el tamaño de la ventana */
void       sgl_trocar(void);                   /* muestra el cuadro (ventana) */
/* D425 (para EGL): otro destino para el contexto (janela o px en memoria), y
 * bits de esténcil de la configuración (0 = no tiene; 8 por defecto). */
int        sgl_mudar_destino(sgl_ctx_t *c, void *janela, uint32_t *px, int larg, int alt);
void       sgl_estencil(sgl_ctx_t *c, int bits);
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
 * Texturas RGBA de 8 bits (D404: también RGB565, RGBA4444, RGBA5551,
 * luminancia y alfa, que se guardan como RGBA), hasta SGL_MAX_LADO de lado,
 * con repetición (GL_REPEAT) o borde (GL_CLAMP_TO_EDGE). Filtro: GL_NEAREST
 * (el de v6), GL_LINEAR y los cuatro *_MIPMAP_* (D404). */
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

/* ---- v8.5.4.10r2 (D404): texturas filtradas, mipmaps, luces por vértice ----
 * Lo de antes no cambia de píxel: la ruta nueva («realista») solo se usa
 * cuando el programa pide algo de esto: un filtro distinto de GL_NEAREST,
 * glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST), normales (glNormal3f o
 * GL_NORMAL_ARRAY), glMaterial, glEnable(GL_LIGHTn) o glFogf(GL_FOG_MODE, ...).
 * En esa ruta las texturas y los colores se interpolan con corrección de
 * perspectiva, el filtro es bilineal o trilineal con mipmaps, y la luz es la
 * de OpenGL ES 1.1 por vértice (ambiente + difusa + especular, hasta 8 luces,
 * atenuación y materiales) con el especular sumado después de la textura. */
#ifndef __gles1_gl_h_
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST  0x2701
#define GL_NEAREST_MIPMAP_LINEAR  0x2702
#define GL_LINEAR_MIPMAP_LINEAR   0x2703
#define GL_GENERATE_MIPMAP        0x8191

#define GL_ALPHA           0x1906
#define GL_LUMINANCE       0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034
#define GL_UNSIGNED_SHORT_5_6_5   0x8363

#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GL_DONT_CARE 0x1100
#define GL_FASTEST   0x1101
#define GL_NICEST    0x1102

#define GL_LIGHT1 0x4001
#define GL_LIGHT2 0x4002
#define GL_LIGHT3 0x4003
#define GL_LIGHT4 0x4004
#define GL_LIGHT5 0x4005
#define GL_LIGHT6 0x4006
#define GL_LIGHT7 0x4007
#define GL_AMBIENT               0x1200
#define GL_DIFFUSE               0x1201
#define GL_SPECULAR              0x1202
#define GL_POSITION              0x1203
#define GL_CONSTANT_ATTENUATION  0x1207
#define GL_LINEAR_ATTENUATION    0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#define GL_EMISSION              0x1600
#define GL_SHININESS             0x1601
#define GL_AMBIENT_AND_DIFFUSE   0x1602
#define GL_FRONT_AND_BACK        0x0408
#define GL_COLOR_MATERIAL        0x0B57
#define GL_NORMALIZE             0x0BA1
#define GL_RESCALE_NORMAL        0x803A
#define GL_LIGHT_MODEL_AMBIENT   0x0B53
#define GL_NORMAL_ARRAY          0x8075

#define GL_FOG_MODE    0x0B65
#define GL_FOG_DENSITY 0x0B62
#define GL_EXP         0x0800
#define GL_EXP2        0x0801

#endif /* __gles1_gl_h_ */

#define SGL_MAX_LADO   2048   /* lado máximo de una textura */
#define SGL_MAX_LUZES  8

void glHint(GLenum alvo, GLenum modo);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glNormalPointer(GLenum tipo, GLsizei passo, const void *p);     /* GL_FLOAT */
void glMaterialf(GLenum face, GLenum nome, GLfloat valor);
void glMaterialfv(GLenum face, GLenum nome, const GLfloat *valor);
void glLightf(GLenum luz, GLenum nome, GLfloat valor);
void glLightModelfv(GLenum nome, const GLfloat *valor);
void glGenerateMipmap(GLenum alvo);   /* genera los niveles desde el 0 (caja de 2x2) */

/* Cuentas desde el último sgl_estatisticas(..., 1): para medir. */
typedef struct {
    uint32_t triangulos;       /* triángulos que llegaron al rasterizador o a la GPU */
    uint32_t tex_subidas;      /* texturas subidas a la GPU */
    uint32_t tex_kb_subidos;   /* KB subidos a la GPU */
    uint32_t tex_reusos;       /* dibujos con la textura ya en la GPU */
    uint32_t tex_kb;           /* KB de texturas de este contexto (con mipmaps) */
    uint32_t gpu_vagas;        /* huecos de textura que da el kernel (0 si no hay GPU) */
} sgl_estat_t;
void sgl_estatisticas(sgl_estat_t *e, int zerar);

/* ---- v8.3: 3D por la GPU (D285) ----
 * Si el kernel tiene un driver 3D para la tarjeta (Intel Gen3...) y el motor
 * está libre, un contexto de ventana manda sus triángulos a la GPU; si no, o
 * si la GPU falla, sigue por software sin que el programa haga nada. Con la
 * GPU el cuadro llega a los píxeles de la ventana en sgl_trocar() o en
 * glFinish(): quien dibuje encima con la CPU (texto, HUD) llama antes a
 * glFinish(). Los contextos en memoria (sgl_criar_contexto_mem) son siempre
 * por software salvo que se pida con sgl_gpu_ligar(). La GPU no da los mismos
 * píxeles bit a bit que el motor por software (bordes, redondeo). */
/* D451: el contexto de ventana dibuja en un búfer propio y sgl_trocar copia el cuadro entero a la ventana (EGL lo
 * pide para sus superficies de ventana). Con la GPU a pantalla completa, sgl_trocar presenta directo (D432). */
void sgl_tras_proprio(sgl_ctx_t *c);
/* D451: escala de dibujo (25 a 100 %; 100 = el tamaño de la ventana) de un contexto con búfer propio; el cuadro se
 * amplía a la ventana al presentar. sgl_escala_medida da el lado que resulta. */
int  sgl_escala(sgl_ctx_t *c, int pct);
int  sgl_escala_medida(int lado, int pct);
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
