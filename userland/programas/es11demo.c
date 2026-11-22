// SPDX-License-Identifier: GPL-2.0-or-later
/* es11demo - demostración de OpenGL ES 1.1 con EGL (D425).
 *
 * Toda la parte gráfica usa SOLO las cabeceras estándar (GLES/gl.h, GLES/glext.h
 * y EGL/egl.h): es lo mismo que se escribiría para cualquier OpenGL ES 1.1. Lo
 * único de SaguiOS es abrir la ventana (la «ventana nativa» de EGL, al final).
 * Cuatro escenas:
 *   1 lightmap    multitextura: ladrillo × mapa de luz (GL_COMBINE, escala 2)
 *   2 particulas  fuente de partículas con sprites de puntos y tamaño por punto
 *   3 sombra      sombra plana con esténcil (cada píxel se oscurece una vez)
 *   4 reflejo     esfera con REFLECTION_MAP sobre un cubo de cielo
 *
 *   es11demo            ventana de 320x240; 1-4 o espacio cambian de escena, Esc cierra
 *   es11demo -c N       la escena N (cuadro fijo) en un pbuffer de EGL: escribe su suma
 *                       (el mismo programa en el anfitrión da la misma: tests/h424_sgl_es11.py)
 *   es11demo -n N       la ventana durante N cuadros por escena y los fps de cada una
 *   es11demo -g N       la escena N por software y por la GPU (D426): sumas y píxeles distintos
 * En el anfitrión (-DSGL_ANFITRIAO): «-c N» y «-p N fichero.ppm» (la imagen, para docs/capturas). */
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>

#define L 320
#define A 240

/* ------------------------------------------------------------ utilidades (sin libm) */

static float seno(float x)
{
    const float PI = 3.1415927f, DPI = 6.2831855f;
    while (x > PI) x -= DPI;
    while (x < -PI) x += DPI;
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880)))));
}
static float coseno(float x) { return seno(x + 1.5707964f); }

static unsigned semente;
static float aleatorio(void) { semente = semente * 1103515245u + 12345u; return (float)((semente >> 8) & 0xFFFF) / 65535.0f; }

/* ------------------------------------------------------------ texturas generadas */

static GLuint tex_ladrillo, tex_luz, tex_mancha, tex_ceu, tex_suelo;

static void criar_texturas(void)
{
    static unsigned char lad[64 * 64 * 3], luz[32 * 32], man[32 * 32 * 2], suelo[32 * 32 * 3];
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int fila = y / 16, xx = (x + (fila & 1) * 16) % 32;
            int junta = (y % 16) < 2 || xx < 2;
            int v = ((x * 7 + y * 13) % 23) * 2;
            unsigned char *p = lad + (y * 64 + x) * 3;
            if (junta) { p[0] = 190; p[1] = 185; p[2] = 175; }
            else { p[0] = (unsigned char)(150 + v); p[1] = (unsigned char)(60 + v / 2); p[2] = (unsigned char)(40 + v / 3); }
        }
    /* el mapa de luz: dos manchas de luz sobre penumbra (0.5 = sin cambio con escala 2) */
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            float d1 = (float)((x - 9) * (x - 9) + (y - 10) * (y - 10)), d2 = (float)((x - 24) * (x - 24) + (y - 20) * (y - 20));
            float l = 40.0f + 200.0f / (1.0f + d1 * 0.02f) + 150.0f / (1.0f + d2 * 0.03f);
            luz[y * 32 + x] = (unsigned char)(l > 255 ? 255 : l);
        }
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            float dx = (float)x - 15.5f, dy = (float)y - 15.5f, d = (dx * dx + dy * dy) / 256.0f;
            float a = 1.0f - d;
            if (a < 0) a = 0;
            man[(y * 32 + x) * 2] = 255;
            man[(y * 32 + x) * 2 + 1] = (unsigned char)(a * a * 255.0f);
        }
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int c = ((x / 4) ^ (y / 4)) & 1;
            unsigned char *p = suelo + (y * 32 + x) * 3;
            p[0] = (unsigned char)(c ? 200 : 120); p[1] = (unsigned char)(c ? 200 : 130); p[2] = (unsigned char)(c ? 190 : 110);
        }
    GLuint t[5];
    glGenTextures(5, t);
    tex_ladrillo = t[0]; tex_luz = t[1]; tex_mancha = t[2]; tex_ceu = t[3]; tex_suelo = t[4];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, tex_ladrillo);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, lad);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, tex_luz);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 32, 32, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, luz);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, tex_mancha);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 32, 32, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, man);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, tex_suelo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, suelo);
    /* el cielo en un cubo: degradado de azul arriba a ocre abajo, con una rejilla */
    glBindTexture(GL_TEXTURE_CUBE_MAP_OES, tex_ceu);
    static unsigned char cara[32 * 32 * 3];
    for (int f = 0; f < 6; f++) {
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++) {
                /* altura de la dirección de este texel (la v de las caras laterales va hacia abajo) */
                float h = f == 2 ? 1.0f : f == 3 ? -1.0f : 1.0f - 2.0f * ((float)y + 0.5f) / 32.0f;
                int rej = (x % 8) == 0 || (y % 8) == 0;
                unsigned char *p = cara + (y * 32 + x) * 3;
                if (h > 0) { p[0] = (unsigned char)(90 + 60 * (1 - h)); p[1] = (unsigned char)(140 + 60 * (1 - h)); p[2] = 235; }
                else { p[0] = (unsigned char)(150 - 60 * h); p[1] = (unsigned char)(120 - 30 * h); p[2] = 70; }
                if (rej) { p[0] = (unsigned char)(p[0] / 2 + 100); p[1] = (unsigned char)(p[1] / 2 + 100); p[2] = (unsigned char)(p[2] / 2 + 100); }
            }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + f, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, cara);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void perspectiva(float fov_y, float n, float f)
{
    float t = n * seno(fov_y * 0.5f) / coseno(fov_y * 0.5f), r = t * (float)L / (float)A;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustumf(-r, r, -t, t, n, f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void desligar_tudo(void)
{
    for (int u = 3; u >= 0; u--) {
        glActiveTexture(GL_TEXTURE0 + u);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_CUBE_MAP_OES);
        glDisable(GL_TEXTURE_GEN_STR_OES);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glClientActiveTexture(GL_TEXTURE0 + u);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glDisable(GL_BLEND); glDisable(GL_LIGHTING); glDisable(GL_STENCIL_TEST); glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); glDisable(GL_COLOR_MATERIAL); glDisable(GL_FOG);
    glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_POINT_SIZE_ARRAY_OES);
    glDepthMask(GL_TRUE);
}

/* ------------------------------------------------------------ 1: mapa de luz */

static void cena_lightmap(float t)
{
    static const GLfloat v[] = { -3, -1, -3,  3, -1, -3,  3, -1, 3,  -3, -1, 3,      /* suelo */
                                 -3, -1, -3, 3, -1, -3,  3, 2, -3,  -3, 2, -3,       /* fondo */
                                 -3, -1, 3,  -3, -1, -3,  -3, 2, -3,  -3, 2, 3 };    /* izquierda */
    static const GLfloat uv0[] = { 0, 0, 4, 0, 4, 4, 0, 4,  0, 0, 4, 0, 4, 2, 0, 2,  0, 0, 4, 0, 4, 2, 0, 2 };
    static const GLfloat uv1[] = { 0, 0, 1, 0, 1, 1, 0, 1,  0, 0, 1, 0, 1, 1, 0, 1,  0, 0, 1, 0, 1, 1, 0, 1 };
    glClearColor(0.05f, 0.05f, 0.08f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    perspectiva(1.0f, 0.5f, 30.0f);
    glTranslatef(0, -0.2f, -5.5f);
    glRotatef(20.0f, 1, 0, 0);
    glRotatef(25.0f + 15.0f * seno(t * 0.7f), 0, 1, 0);
    glEnable(GL_DEPTH_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_ladrillo);
    glEnable(GL_TEXTURE_2D);
    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uv0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex_luz);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
    glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
    glClientActiveTexture(GL_TEXTURE1);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uv1);
    glColor4f(1, 1, 1, 1);
    for (int i = 0; i < 3; i++) glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
    desligar_tudo();
}

/* ------------------------------------------------------------ 2: partículas */

#define N_PART 300
static void cena_particulas(float t)
{
    static GLfloat pos[N_PART * 3], tam[N_PART];
    static GLubyte cor[N_PART * 4];
    semente = 12345;
    for (int i = 0; i < N_PART; i++) {
        float vida = 2.0f, fase = aleatorio() * vida, idade = t + fase;
        while (idade > vida) idade -= vida;
        float ang = aleatorio() * 6.2831855f, vel = 0.6f + aleatorio() * 0.5f, sub = 3.0f + aleatorio();
        pos[i * 3] = coseno(ang) * vel * idade;
        pos[i * 3 + 1] = -1.0f + sub * idade - 2.5f * idade * idade;
        pos[i * 3 + 2] = seno(ang) * vel * idade;
        float k = idade / vida;
        tam[i] = 14.0f + 26.0f * k;
        cor[i * 4] = 255; cor[i * 4 + 1] = (GLubyte)(220 - 150 * k); cor[i * 4 + 2] = (GLubyte)(80 * (1 - k)); cor[i * 4 + 3] = (GLubyte)(255 * (1 - k));
    }
    glClearColor(0.02f, 0.02f, 0.06f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    perspectiva(1.0f, 0.5f, 30.0f);
    glTranslatef(0, -0.3f, -5.0f);
    glRotatef(15.0f, 1, 0, 0);
    glRotatef(t * 20.0f, 0, 1, 0);
    /* el suelo */
    static const GLfloat chao[] = { -2, -1, -2, 2, -1, -2, 2, -1, 2, -2, -1, 2 };
    static const GLfloat uvc[] = { 0, 0, 3, 0, 3, 3, 0, 3 };
    glEnable(GL_DEPTH_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, chao);
    glBindTexture(GL_TEXTURE_2D, tex_suelo);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uvc);
    glColor4f(0.6f, 0.6f, 0.7f, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    /* las partículas: sprites con mezcla aditiva, sin escribir Z */
    glBindTexture(GL_TEXTURE_2D, tex_mancha);
    glEnable(GL_POINT_SPRITE_OES);
    glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    static const GLfloat att[3] = { 0.0f, 0.0f, 0.04f };
    glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, att);
    glPointParameterf(GL_POINT_SIZE_MAX, 48.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glVertexPointer(3, GL_FLOAT, 0, pos);
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, cor);
    glEnableClientState(GL_POINT_SIZE_ARRAY_OES);
    glPointSizePointerOES(GL_FLOAT, 0, tam);
    glDrawArrays(GL_POINTS, 0, N_PART);
    glDisable(GL_POINT_SPRITE_OES);
    static const GLfloat att0[3] = { 1, 0, 0 };
    glPointParameterfv(GL_POINT_DISTANCE_ATTENUATION, att0);
    desligar_tudo();
}

/* ------------------------------------------------------------ 3: sombra con esténcil */

static const GLfloat cubo_v[] = {
    -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1,   1,-1,-1, -1,-1,-1, -1, 1,-1,  1, 1,-1,
    -1,-1,-1, -1,-1, 1, -1, 1, 1, -1, 1,-1,   1,-1, 1,  1,-1,-1,  1, 1,-1,  1, 1, 1,
    -1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1,-1,  -1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1, 1 };
static const GLfloat cubo_n[] = {
    0,0,1, 0,0,1, 0,0,1, 0,0,1,  0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1,  -1,0,0, -1,0,0, -1,0,0, -1,0,0,
    1,0,0, 1,0,0, 1,0,0, 1,0,0,  0,1,0, 0,1,0, 0,1,0, 0,1,0,  0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0 };

static void desenhar_cubo(void)
{
    glVertexPointer(3, GL_FLOAT, 0, cubo_v);
    for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
}

static void cena_sombra(float t)
{
    static const GLfloat chao[] = { -4, 0, -4, 4, 0, -4, 4, 0, 4, -4, 0, 4 };
    static const GLfloat uvc[] = { 0, 0, 4, 0, 4, 4, 0, 4 };
    const GLfloat luz[4] = { 2.0f, 5.0f, 1.5f, 1.0f };   /* luz puntual */
    glClearColor(0.25f, 0.3f, 0.4f, 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    perspectiva(1.0f, 0.5f, 40.0f);
    glTranslatef(0, -1.2f, -7.0f);
    glRotatef(25.0f, 1, 0, 0);
    glRotatef(-30.0f + 10.0f * seno(t * 0.5f), 0, 1, 0);
    glEnable(GL_DEPTH_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    /* el suelo marca el esténcil a 1 */
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glVertexPointer(3, GL_FLOAT, 0, chao);
    glBindTexture(GL_TEXTURE_2D, tex_suelo);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uvc);
    glColor4f(1, 1, 1, 1);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
    /* la sombra: el cubo aplastado sobre el suelo desde la luz; solo donde el esténcil es 1, y lo sube a 2 */
    GLfloat m[16] = { luz[1], 0, 0, 0,  -luz[0], 0, -luz[2], -1,  0, 0, luz[1], 0,  0, 0, 0, luz[1] };
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.5f);
    glPushMatrix();
    glMultMatrixf(m);
    glTranslatef(0, 1.6f, 0);
    glRotatef(t * 40.0f, 0.3f, 1, 0.2f);
    glScalef(0.7f, 0.7f, 0.7f);
    desenhar_cubo();
    glPopMatrix();
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);
    /* el cubo, con luz */
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, luz);
    static const GLfloat dif[4] = { 0.9f, 0.85f, 0.8f, 1 }, amb[4] = { 0.25f, 0.25f, 0.3f, 1 };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glEnable(GL_COLOR_MATERIAL);
    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, cubo_n);
    glColor4f(0.9f, 0.4f, 0.2f, 1);
    glPushMatrix();
    glTranslatef(0, 1.6f, 0);
    glRotatef(t * 40.0f, 0.3f, 1, 0.2f);
    glScalef(0.7f, 0.7f, 0.7f);
    desenhar_cubo();
    glPopMatrix();
    glDisable(GL_LIGHT0);
    desligar_tudo();
}

/* ------------------------------------------------------------ 4: reflejo con un cubo */

#define ES_LAT 12
#define ES_LON 20
static void cena_reflexo(float t)
{
    static GLfloat v[(ES_LAT + 1) * (ES_LON + 1) * 3];
    static GLushort idx[ES_LAT * ES_LON * 6];
    static int feito;
    if (!feito) {
        for (int i = 0; i <= ES_LAT; i++)
            for (int j = 0; j <= ES_LON; j++) {
                float th = 3.1415927f * (float)i / ES_LAT, ph = 6.2831855f * (float)j / ES_LON;
                GLfloat *p = v + (i * (ES_LON + 1) + j) * 3;
                p[0] = seno(th) * coseno(ph); p[1] = coseno(th); p[2] = seno(th) * seno(ph);
            }
        int k = 0;
        for (int i = 0; i < ES_LAT; i++)
            for (int j = 0; j < ES_LON; j++) {
                GLushort a = (GLushort)(i * (ES_LON + 1) + j), b = (GLushort)(a + ES_LON + 1);
                /* en sentido antihorario visto desde fuera */
                idx[k++] = a; idx[k++] = (GLushort)(a + 1); idx[k++] = b;
                idx[k++] = (GLushort)(a + 1); idx[k++] = (GLushort)(b + 1); idx[k++] = b;
            }
        feito = 1;
    }
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    perspectiva(1.1f, 0.5f, 50.0f);
    glRotatef(10.0f, 1, 0, 0);
    glRotatef(t * 15.0f, 0, 1, 0);
    /* el cielo: un cubo grande cuyas coordenadas de textura son sus vértices */
    glBindTexture(GL_TEXTURE_CUBE_MAP_OES, tex_ceu);
    glEnable(GL_TEXTURE_CUBE_MAP_OES);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(3, GL_FLOAT, 0, cubo_v);
    glPushMatrix();
    glScalef(20, 20, 20);
    desenhar_cubo();
    glPopMatrix();
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    /* la esfera: el cielo reflejado (la normal es la posición); la matriz de textura vuelve
     * del ojo al mundo (la inversa del giro de la cámara) */
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glRotatef(-t * 15.0f, 0, 1, 0);
    glRotatef(-10.0f, 1, 0, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glTranslatef(0, 0, -3.0f);
    glEnable(GL_TEXTURE_GEN_STR_OES);
    glTexGeniOES(GL_TEXTURE_GEN_STR_OES, GL_TEXTURE_GEN_MODE_OES, GL_REFLECTION_MAP_OES);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnableClientState(GL_NORMAL_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    glNormalPointer(GL_FLOAT, 0, v);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(0.95f, 0.95f, 1.0f, 1);
    glDrawElements(GL_TRIANGLES, ES_LAT * ES_LON * 6, GL_UNSIGNED_SHORT, idx);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    desligar_tudo();
}

typedef void (*cena_f)(float);
static const cena_f cenas[4] = { cena_lightmap, cena_particulas, cena_sombra, cena_reflexo };
static const char *nomes[4] = { "lightmap", "particulas", "sombra", "reflejo" };

/* ------------------------------------------------------------ EGL */

static EGLDisplay dpy;
static EGLSurface sup;
static EGLContext ctx;

static int egl_iniciar(EGLNativeWindowType w)
{
    EGLint maj, min, n;
    EGLConfig cfg;
    const EGLint at[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8,
                          EGL_SURFACE_TYPE, w ? EGL_WINDOW_BIT : EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT, EGL_NONE };
    const EGLint pb[] = { EGL_WIDTH, L, EGL_HEIGHT, A, EGL_NONE };
    const EGLint cx[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, &maj, &min)) return 0;
    if (!eglChooseConfig(dpy, at, &cfg, 1, &n) || n < 1) return 0;
    sup = w ? eglCreateWindowSurface(dpy, cfg, w, 0) : eglCreatePbufferSurface(dpy, cfg, pb);
    if (sup == EGL_NO_SURFACE) return 0;
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cx);
    if (ctx == EGL_NO_CONTEXT) return 0;
    if (!eglMakeCurrent(dpy, sup, sup, ctx)) return 0;
    eglSwapInterval(dpy, 1);
    criar_texturas();
    return 1;
}

static void egl_fim(void)
{
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, sup);
    eglTerminate(dpy);
}

/* Suma FNV-1a del cuadro leído con glReadPixels (la misma en el anfitrión y en el SaguiOS). */
static unsigned soma_quadro(void)
{
    static unsigned char p[L * A * 4];
    glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, p);
    unsigned h = 2166136261u;
    for (int i = 0; i < L * A; i++) for (int k = 0; k < 3; k++) { h ^= p[i * 4 + k]; h *= 16777619u; }
    return h;
}

#define T_FIXO 1.25f

/* ------------------------------------------------------------ plataforma */

#if defined(SGL_ANFITRIAO)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (!egl_iniciar(0)) { printf("es11demo: sin EGL\n"); return 1; }
    if (argc > 2 && !strcmp(argv[1], "-c")) {
        int n = atoi(argv[2]);
        if (n < 1 || n > 4) return 1;
        cenas[n - 1](T_FIXO);
        printf("%s %08x\n", nomes[n - 1], soma_quadro());
    } else if (argc > 3 && !strcmp(argv[1], "-p")) {
        int n = atoi(argv[2]);
        if (n < 1 || n > 4) return 1;
        cenas[n - 1](T_FIXO);
        static unsigned char p[L * A * 4];
        glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, p);
        FILE *f = fopen(argv[3], "wb");
        if (!f) return 1;
        fprintf(f, "P6\n%d %d\n255\n", L, A);
        for (int y = A - 1; y >= 0; y--) for (int x = 0; x < L; x++) fwrite(p + (y * L + x) * 4, 1, 3, f);
        fclose(f);
    } else {
        for (int n = 0; n < 4; n++) { cenas[n](T_FIXO); printf("%s %08x\n", nomes[n], soma_quadro()); }
    }
    egl_fim();
    return 0;
}
#else
#include "sagui.h"
#include "gui.h"
/* D426: para «-g», de la SaguiGL (sin su cabecera: la demo solo usa las estándar, y el TCC también la compila). */
struct sgl_ctx;
struct sgl_ctx *sgl_contexto_atual(void);
int sgl_gpu_ligar(struct sgl_ctx *c);
int sgl_gpu_ativa(void);

static int tem_sse2(void)
{
    unsigned a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    return (d >> 26) & 1;
}

int principal(int argc, char **argv)
{
    if (!tem_sse2()) { imprimir("es11demo: SaguiGL pide SSE2\n"); return 1; }
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'c') {
        int n = argv[2][0] - '0';
        if (n < 1 || n > 4) { imprimir("es11demo: escena 1 a 4\n"); return 1; }
        if (!egl_iniciar(0)) { imprimir("es11demo: sin EGL (%x)\n", eglGetError()); return 1; }
        cenas[n - 1](T_FIXO);
        imprimir("%s %08x\n", nomes[n - 1], soma_quadro());
        egl_fim();
        return 0;
    }
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'g') {
        /* D426: la escena N por software y por la GPU (gpu3d v1 o v2, lo que anuncie el driver): sumas y
         * cuántos píxeles se apartan más de 24 en algún canal (los bordes de los triángulos no son iguales). */
        int n = argv[2][0] - '0';
        if (n < 1 || n > 4) { imprimir("es11demo: escena 1 a 4\n"); return 1; }
        if (!egl_iniciar(0)) { imprimir("es11demo: sin EGL (%x)\n", eglGetError()); return 1; }
        static unsigned char sw[L * A * 4], gp[L * A * 4];
        cenas[n - 1](T_FIXO);
        unsigned s_sw = soma_quadro();
        glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, sw);
        if (!sgl_gpu_ligar(sgl_contexto_atual())) { imprimir("es11demo: %s sin GPU\n", nomes[n - 1]); egl_fim(); return 1; }
        cenas[n - 1](T_FIXO);
        unsigned s_gp = soma_quadro();
        glReadPixels(0, 0, L, A, GL_RGBA, GL_UNSIGNED_BYTE, gp);
        int dif = 0, ativa = sgl_gpu_ativa();
        for (int i = 0; i < L * A; i++)
            for (int k = 0; k < 3; k++) {
                int d = (int)sw[i * 4 + k] - (int)gp[i * 4 + k];
                if (d > 24 || d < -24) { dif++; break; }
            }
        imprimir("es11demo -g: %s software %08x gpu %08x (%s) distintos %d de %d\n", nomes[n - 1], s_sw, s_gp, ativa ? "GPU" : "CPU", dif, L * A);
        egl_fim();
        return 0;
    }
    int por_cena = 0;
    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n') { por_cena = 0; for (const char *s = argv[2]; *s >= '0' && *s <= '9'; s++) por_cena = por_cena * 10 + (*s - '0'); }
    jan_t *j = jan_abrir("OpenGL ES 1.1 + EGL", L, A);
    if (!j) { imprimir("es11demo: sin ventana\n"); return 1; }
    if (!egl_iniciar((EGLNativeWindowType)j)) { imprimir("es11demo: sin EGL (%x)\n", eglGetError()); jan_fechar(j); return 1; }
    int cena = 0, quadros = 0, sair = 0;
    unsigned t0 = tempo_ms(), tc = t0;
    while (!sair) {
        evento_t e;
        while (proximo_evento(j->id, &e, false)) {
            if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == 27)) sair = 1;
            else if (e.tipo == EV_TECLA && e.tecla >= '1' && e.tecla <= '4') cena = e.tecla - '1';
            else if (e.tipo == EV_TECLA && e.tecla == ' ') cena = (cena + 1) % 4;
        }
        float t = (float)(tempo_ms() - t0) * 0.001f;
        cenas[cena](t);
        eglSwapBuffers(dpy, sup);
        quadros++;
        if (por_cena && quadros >= por_cena) {
            unsigned ms = tempo_ms() - tc;
            if (!ms) ms = 1;
            imprimir("es11demo: %s %d cuadros en %u ms, %u fps\n", nomes[cena], quadros, ms, (unsigned)quadros * 1000u / ms);
            quadros = 0; tc = tempo_ms();
            if (++cena == 4) sair = 1;
        }
    }
    egl_fim();
    jan_fechar(j);
    return 0;
}
#endif
