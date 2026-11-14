// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl.c - motor de SaguiGL por software, de referencia (mejora 054, v5; D104).
 *
 * Tubería: vértice -> matrices (modelo/vista y proyección) -> coordenadas de
 * recorte -> recorte contra los 6 planos del frustum (Sutherland-Hodgman, con
 * color interpolado) -> división por w -> viewport -> triángulos en punto fijo
 * 28.4 (subpíxel de 1/16) -> rasterización por funciones de arista con la
 * regla top-left -> prueba de Z (16 o 32 bits) -> píxel.
 *
 * Decisiones de claridad y determinismo:
 *   - la rasterización solo usa enteros (los bordes y los gradientes de Z y
 *     de color); la coma flotante queda en la parte de vértices;
 *   - sin libc ni libm: seno y coseno propios, la raíz es la del compilador;
 *   - los colores se interpolan en espacio de pantalla (sin corrección de
 *     perspectiva: sin texturas no hace falta para que el cubo se vea bien);
 *   - sombreado plano = color del último vértice del primitivo (como GL);
 *   - ventana de visión de hasta 2048x2048 (límite del rango de los gradientes).
 * Compila igual en el anfitrión (-DSGL_ANFITRIAO -ffp-contract=off) para
 * calcular los valores esperados de tests/h77_sgl.py.
 *
 * D127: la aritmética entera del rasterizador es de 32 bits, sin int64_t, para
 * que el motor lo pueda compilar también el cc de dentro del SaguiOS (que no
 * tiene enteros de 64 bits); los valores exactos van en double, que representa
 * enteros hasta 2^53. */
#include "sgl.h"

#if defined(__SSE2__) && !defined(SGL_POR_CC)
/* La ruta SIMD (SSE2, cuatro píxeles a la vez) solo se compila donde hay
 * intrínsecos: el GCC del anfitrión y el GCC del SaguiOS. El cc de dentro
 * del SaguiOS (SGL_POR_CC) no los trae: ahí va la ruta de referencia, que
 * da exactamente el mismo píxel (prueba h101). */
/* D191: el GCC incluye <mm_malloc.h> (que pide <stdlib.h>) desde las cabeceras
 * SSE aunque sea -ffreestanding. Aquí no se usa _mm_malloc: se marca la
 * cabecera como ya incluida y no hace falta ninguna libc del anfitrión. */
#define _MM_MALLOC_H_INCLUDED
#include <emmintrin.h>
#define SGL_TEM_SIMD 1
#endif

#ifdef SGL_ANFITRIAO
#include <stdlib.h>
#define SGL_ALOCAR(n)  calloc(1, (n))
#define SGL_LIBERAR(p) free(p)
#elif defined(SGL_POR_CC)
/* Compilado por el cc de dentro del SaguiOS (el ejemplo /ejemplos/sagui_gl):
 * el cc ya trae calloc y free, y no hay libsagui (la ventana es del propio
 * ejemplo, que dibuja en el búfer de píxeles de ella). */
#include <stdlib.h>
#define SGL_ALOCAR(n)  calloc(1, (n))
#define SGL_LIBERAR(p) free(p)
#else
#include "gui.h"
#define SGL_ALOCAR(n)  aloca_zerado(n)
#define SGL_LIBERAR(p) libera(p)
#endif

#define PILHA_MV   32
#define PILHA_PROJ 4
#define MAX_BEGIN  3072
#define LIM_VIEW   2048

typedef struct { float x, y, z, w, r, g, b; float u, v, a, luz; float ex, ey, ez; } vert_t;

#define MAX_TEX 4

typedef struct {
    int           usada;
    GLuint        id;
    int           larg, alt;
    unsigned char *px;      /* RGBA, 4 bytes por píxel */
    int           wrap_s, wrap_t;
} textura_t;

struct sgl_ctx {
    uint32_t *px;
    int       larg, alt;
    int       zbits;
    void     *zbuf;
    void     *janela;
    int       dono_px;        /* 0: el píxel pertenece a la ventana o al llamante */
    float     mv[PILHA_MV][16], proj[PILHA_PROJ][16];
    int       mv_n, proj_n, modo_mat;
    int       vx, vy, vw, vh;
    uint32_t  cor_limpa;
    float     z_limpo;
    int       teste_z, cull, cull_face, frente, plano, func_z;
    GLenum    erro;
    int       v_ativo, c_ativo, t_ativo;
    int       v_tam, v_passo, c_tam, c_tipo, c_passo;
    const void *v_ptr, *c_ptr;
    int       t_tam, t_tipo, t_passo;
    const void *t_ptr;
    float     cor[4];         /* glColor, 0..255 (alfa en cor[3]) */
    float     tc[2];          /* glTexCoord */
    /* glBegin/glEnd */
    int       begin_modo, em_begin, begin_n;
    float     begin_v[MAX_BEGIN][3], begin_c[MAX_BEGIN][4], begin_t[MAX_BEGIN][2];
    /* v6: texturas, mezcla, niebla y luces */
    int       extras;          /* 1 si hay textura, mezcla, niebla o luces */
    textura_t tex[MAX_TEX];
    int       tex_sel;         /* índice de la textura ligada, -1 si ninguna */
    int       tex_on;          /* GL_TEXTURE_2D activo */
    int       blend, blend_s, blend_d;
    int       fog;
    float     fog_ini, fog_fim;
    float     fog_cor[3];
    int       luz_on;
    float     luz_pos[4], luz_dif[3], luz_amb[3];
};

/* Motor de sombreado: 1 = SIMD (SSE2), 0 = referencia. Las dos rutas dan
 * exactamente el mismo píxel; la de referencia sirve para comparar. */
static int sgl_simd = 1;

static sgl_ctx_t *C;   /* contexto actual */

/* ------------------------------------------------------------ utilidades */

static void erro(GLenum e) { if (C && C->erro == GL_NO_ERROR) C->erro = e; }

float sgl_sen(float x)
{
    const float DOIS_PI = 6.2831855f, PI = 3.1415927f, MEIO_PI = 1.5707964f;
    int n = (int)(x * 0.15915494f + (x >= 0 ? 0.5f : -0.5f));
    x -= (float)n * DOIS_PI;
    if (x > PI) x -= DOIS_PI;
    if (x < -PI) x += DOIS_PI;
    if (x > MEIO_PI) x = PI - x;
    if (x < -MEIO_PI) x = -PI - x;
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880)))));
}

float sgl_cos(float x) { return sgl_sen(x + 1.5707964f); }

/* La raíz en double y no con __builtin_sqrtf: el cc no tiene esa forma, y el
 * sqrtsd del GCC y el fsqrt del cc dan el mismo float (el doble redondeo es
 * seguro con 53 bits de mantisa para un resultado de 24). */
static inline float raiz(float x) { return (float)__builtin_sqrt((double)x); }

/* ---- aritmética entera en double (D127) ----
 *
 * El motor se compilaba con enteros de 64 bits (int64_t) para el z en Q0 y los
 * gradientes en Q16. El cc de dentro del SaguiOS no tiene enteros de 64 bits
 * (no hay long long), así que la aritmética entera pasa a double: los valores
 * son enteros exactos mientras quepan en los 53 bits de la mantisa, que es el
 * caso de todo lo que usa el rasterizador (z de 31 bits, coordenadas de 15,
 * gradientes), y las divisiones se truncan igual que antes.
 *
 * piso() es el suelo (floor) de un double grande: con el redondeo al más
 * cercano (el modo por defecto de la FPU), sumar la constante 2^52+2^51 y
 * restarla deja solo la parte entera, mientras |v| < 2^51. */
#define PISO_MAGICO 6755399441055744.0

static inline double piso(double v) { return (v + PISO_MAGICO) - PISO_MAGICO; }

/* Trunca hacia cero, como la división entera con signo. */
static inline double trun(double v) { return v < 0 ? -piso(-v) : piso(v); }

/* Divide por 2^b y trunca hacia menos infinito (el >> de dos complemento). */
static inline double deszc(double v, int b) { return piso(v / (double)(1 << b)); }

uint32_t sgl_soma(const uint32_t *px, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) {
        uint32_t v = px[i] & 0xFFFFFF;
        for (int k = 0; k < 3; k++) { h ^= (v >> (8 * k)) & 0xFF; h *= 16777619u; }
    }
    return h;
}

/* ------------------------------------------------------------ contexto */

static void ctx_estado_inicial(sgl_ctx_t *c)
{
    static const float id[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    for (int i = 0; i < 16; i++) { c->mv[0][i] = id[i]; c->proj[0][i] = id[i]; }
    c->mv_n = c->proj_n = 1;
    c->modo_mat = GL_MODELVIEW;
    c->vx = c->vy = 0; c->vw = c->larg; c->vh = c->alt;
    if (c->vw > LIM_VIEW) c->vw = LIM_VIEW;
    if (c->vh > LIM_VIEW) c->vh = LIM_VIEW;
    c->cor_limpa = 0; c->z_limpo = 1.0f;
    c->teste_z = 0; c->cull = 0; c->cull_face = GL_BACK; c->frente = GL_CCW; c->plano = 0; c->func_z = GL_LESS;
    c->cor[0] = c->cor[1] = c->cor[2] = 255.0f; c->cor[3] = 255.0f;
    /* v6 */
    c->extras = 0; c->tex_sel = -1; c->tex_on = 0;
    c->blend = 0; c->blend_s = GL_SRC_ALPHA; c->blend_d = GL_ONE_MINUS_SRC_ALPHA;
    c->fog = 0; c->fog_ini = 0; c->fog_fim = 1; c->fog_cor[0] = c->fog_cor[1] = c->fog_cor[2] = 0.0f;
    c->luz_on = 0;
    c->luz_pos[0] = 0.0f; c->luz_pos[1] = 0.0f; c->luz_pos[2] = 1.0f; c->luz_pos[3] = 0.0f;
    c->luz_dif[0] = c->luz_dif[1] = c->luz_dif[2] = 1.0f;
    c->luz_amb[0] = c->luz_amb[1] = c->luz_amb[2] = 0.0f;
}

static sgl_ctx_t *criar(uint32_t *px, int larg, int alt, int zbits, void *janela, int dono)
{
    if (zbits != 16 && zbits != 32) zbits = 32;
    sgl_ctx_t *c = SGL_ALOCAR(sizeof *c);
    if (!c) return 0;
    c->px = px; c->larg = larg; c->alt = alt; c->zbits = zbits; c->janela = janela; c->dono_px = dono;
    c->zbuf = SGL_ALOCAR((size_t)larg * alt * (zbits / 8));
    if (!c->zbuf) { SGL_LIBERAR(c); return 0; }
    ctx_estado_inicial(c);
    C = c;
    return c;
}

sgl_ctx_t *sgl_criar_contexto_mem(uint32_t *px, int larg, int alt, int zbits)
{
    return criar(px, larg, alt, zbits, 0, 0);
}

#if !defined(SGL_ANFITRIAO) && !defined(SGL_POR_CC)
sgl_ctx_t *sgl_criar_contexto(void *janela, int zbits)
{
    jan_t *j = janela;
    return criar(j->px, j->larg, j->alt, zbits, j, 0);
}

void sgl_redimensionar(sgl_ctx_t *c)
{
    jan_t *j = c->janela;
    if (!j) return;
    SGL_LIBERAR(c->zbuf);
    c->px = j->px; c->larg = j->larg; c->alt = j->alt;
    c->zbuf = SGL_ALOCAR((size_t)c->larg * c->alt * (c->zbits / 8));
    c->vx = c->vy = 0;
    c->vw = c->larg > LIM_VIEW ? LIM_VIEW : c->larg;
    c->vh = c->alt > LIM_VIEW ? LIM_VIEW : c->alt;
}

void sgl_trocar(void)
{
    if (C && C->janela) jan_mostrar(C->janela);
}
#else
void sgl_redimensionar(sgl_ctx_t *c) { (void)c; }
void sgl_trocar(void) {}
#endif

void sgl_destruir_contexto(sgl_ctx_t *c)
{
    if (!c) return;
    if (C == c) C = 0;
    SGL_LIBERAR(c->zbuf);
    SGL_LIBERAR(c);
}

void sgl_usar_contexto(sgl_ctx_t *c) { C = c; }

/* ------------------------------------------------------------ matrizes */

static float *atual(void)
{
    return C->modo_mat == GL_PROJECTION ? C->proj[C->proj_n - 1] : C->mv[C->mv_n - 1];
}

/* r = a * b (por columnas) */
static void mat_mul(float *r, const float *a, const float *b)
{
    float t[16];
    for (int col = 0; col < 4; col++)
        for (int lin = 0; lin < 4; lin++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k * 4 + lin] * b[col * 4 + k];
            t[col * 4 + lin] = s;
        }
    for (int i = 0; i < 16; i++) r[i] = t[i];
}

void glMatrixMode(GLenum m)
{
    if (!C) return;
    if (m != GL_MODELVIEW && m != GL_PROJECTION) { erro(GL_INVALID_ENUM); return; }
    C->modo_mat = (int)m;
}

void glLoadIdentity(void)
{
    if (!C) return;
    float *m = atual();
    for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

void glLoadMatrixf(const GLfloat *s)
{
    if (!C) return;
    float *m = atual();
    for (int i = 0; i < 16; i++) m[i] = s[i];
}

void glMultMatrixf(const GLfloat *s)
{
    if (!C) return;
    float *m = atual();
    mat_mul(m, m, s);
}

void glPushMatrix(void)
{
    if (!C) return;
    if (C->modo_mat == GL_PROJECTION) {
        if (C->proj_n >= PILHA_PROJ) { erro(GL_STACK_OVERFLOW); return; }
        for (int i = 0; i < 16; i++) C->proj[C->proj_n][i] = C->proj[C->proj_n - 1][i];
        C->proj_n++;
    } else {
        if (C->mv_n >= PILHA_MV) { erro(GL_STACK_OVERFLOW); return; }
        for (int i = 0; i < 16; i++) C->mv[C->mv_n][i] = C->mv[C->mv_n - 1][i];
        C->mv_n++;
    }
}

void glPopMatrix(void)
{
    if (!C) return;
    int *n = C->modo_mat == GL_PROJECTION ? &C->proj_n : &C->mv_n;
    if (*n <= 1) { erro(GL_STACK_UNDERFLOW); return; }
    (*n)--;
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    float t[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1 };
    glMultMatrixf(t);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    float t[16] = { x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1 };
    glMultMatrixf(t);
}

void glRotatef(GLfloat graus, GLfloat x, GLfloat y, GLfloat z)
{
    float n = raiz(x * x + y * y + z * z);
    if (n == 0.0f) return;
    x /= n; y /= n; z /= n;
    float a = graus * 0.017453292f, c = sgl_cos(a), s = sgl_sen(a), k = 1.0f - c;
    float t[16] = {
        x * x * k + c,     y * x * k + z * s, z * x * k - y * s, 0,
        x * y * k - z * s, y * y * k + c,     z * y * k + x * s, 0,
        x * z * k + y * s, y * z * k - x * s, z * z * k + c,     0,
        0, 0, 0, 1 };
    glMultMatrixf(t);
}

void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    if (n <= 0 || f <= 0 || l == r || b == t || n == f) { erro(GL_INVALID_VALUE); return; }
    float m[16] = {
        2 * n / (r - l), 0, 0, 0,
        0, 2 * n / (t - b), 0, 0,
        (r + l) / (r - l), (t + b) / (t - b), -(f + n) / (f - n), -1,
        0, 0, -2 * f * n / (f - n), 0 };
    glMultMatrixf(m);
}

void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    if (l == r || b == t || n == f) { erro(GL_INVALID_VALUE); return; }
    float m[16] = {
        2 / (r - l), 0, 0, 0,
        0, 2 / (t - b), 0, 0,
        0, 0, -2 / (f - n), 0,
        -(r + l) / (r - l), -(t + b) / (t - b), -(f + n) / (f - n), 1 };
    glMultMatrixf(m);
}

/* ------------------------------------------------------------ estado */

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    if (!C) return;
    if (w < 0 || h < 0) { erro(GL_INVALID_VALUE); return; }
    if (w > LIM_VIEW) w = LIM_VIEW;
    if (h > LIM_VIEW) h = LIM_VIEW;
    C->vx = x; C->vy = y; C->vw = w; C->vh = h;
}

static int a_byte(float v)   /* 0..1 -> 0..255 */
{
    if (!(v > 0)) return 0;
    if (v >= 1) return 255;
    return (int)(v * 255.0f + 0.5f);
}

void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    (void)a;
    if (!C) return;
    C->cor_limpa = ((uint32_t)a_byte(r) << 16) | ((uint32_t)a_byte(g) << 8) | (uint32_t)a_byte(b);
}

void glClear(GLbitfield m)
{
    if (!C) return;
    int n = C->larg * C->alt;
    if (m & GL_COLOR_BUFFER_BIT)
        for (int i = 0; i < n; i++) C->px[i] = C->cor_limpa;
    if (m & GL_DEPTH_BUFFER_BIT) {
        if (C->zbits == 32) { uint32_t *z = C->zbuf; for (int i = 0; i < n; i++) z[i] = 0x7FFFFFFFu; }
        else { uint16_t *z = C->zbuf; for (int i = 0; i < n; i++) z[i] = 0xFFFF; }
    }
}

/* ¿Hace falta la ruta v6 (texturas, mezcla, niebla, luces)? */
static void refaz_extras(void)
{
    C->extras = (C->tex_on || C->blend || C->fog || C->luz_on) ? 1 : 0;
}

void glEnable(GLenum cap)
{
    if (!C) return;
    if (cap == GL_DEPTH_TEST) C->teste_z = 1;
    else if (cap == GL_CULL_FACE) C->cull = 1;
    else if (cap == GL_TEXTURE_2D) { C->tex_on = 1; refaz_extras(); }
    else if (cap == GL_BLEND) { C->blend = 1; refaz_extras(); }
    else if (cap == GL_FOG) { C->fog = 1; refaz_extras(); }
    else if (cap == GL_LIGHTING) { C->luz_on = 1; refaz_extras(); }
    else erro(GL_INVALID_ENUM);
}

void glDisable(GLenum cap)
{
    if (!C) return;
    if (cap == GL_DEPTH_TEST) C->teste_z = 0;
    else if (cap == GL_CULL_FACE) C->cull = 0;
    else if (cap == GL_TEXTURE_2D) { C->tex_on = 0; refaz_extras(); }
    else if (cap == GL_BLEND) { C->blend = 0; refaz_extras(); }
    else if (cap == GL_FOG) { C->fog = 0; refaz_extras(); }
    else if (cap == GL_LIGHTING) { C->luz_on = 0; refaz_extras(); }
    else erro(GL_INVALID_ENUM);
}

void glCullFace(GLenum f) { if (C) { if (f == GL_FRONT || f == GL_BACK) C->cull_face = (int)f; else erro(GL_INVALID_ENUM); } }
void glFrontFace(GLenum m) { if (C) { if (m == GL_CW || m == GL_CCW) C->frente = (int)m; else erro(GL_INVALID_ENUM); } }
void glShadeModel(GLenum m) { if (C) { if (m == GL_FLAT) C->plano = 1; else if (m == GL_SMOOTH) C->plano = 0; else erro(GL_INVALID_ENUM); } }
void glDepthFunc(GLenum f) { if (C) { if (f == GL_LESS || f == GL_LEQUAL || f == GL_ALWAYS) C->func_z = (int)f; else erro(GL_INVALID_ENUM); } }
GLenum glGetError(void) { GLenum e = C ? C->erro : GL_INVALID_OPERATION; if (C) C->erro = GL_NO_ERROR; return e; }

/* ------------------------------------------------------------ rasterizador */

/* Un píxel candidato: z de 31 bits. */
static inline void ponto_z(int x, int y, double z, int r, int g, int b)
{
    if (z < 0) z = 0;
    if (z > 0x7FFFFFFE) z = 0x7FFFFFFE;
    int i = y * C->larg + x;
    if (C->teste_z) {
        if (C->zbits == 32) {
            uint32_t *zb = C->zbuf, zi = (uint32_t)z;
            if (C->func_z == GL_LESS ? !(zi < zb[i]) : C->func_z == GL_LEQUAL ? !(zi <= zb[i]) : 0) return;
            zb[i] = zi;
        } else {
            uint16_t *zb = C->zbuf;
            uint32_t zi = (uint32_t)deszc(z, 15);
            if (zi > 0xFFFE) zi = 0xFFFE;
            if (C->func_z == GL_LESS ? !(zi < zb[i]) : C->func_z == GL_LEQUAL ? !(zi <= zb[i]) : 0) return;
            zb[i] = (uint16_t)zi;
        }
    }
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    C->px[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

typedef struct { int X, Y; double a[8]; } sv_t;
/* a[0] = z (Q0, 31 bits), a[1..3] = r, g, b (Q8), a[4..5] = u, v (Q16),
 * a[6] = alfa (Q8), a[7] = luz (Q8). Los cuatro últimos solo se usan en la
 * ruta v6 (texturas, mezcla, niebla y luces); la ruta de referencia sigue
 * con cuatro atributos y da los mismos píxeles que antes. */

static inline int canto_superior_esquerdo(int dx, int dy) { return dy < 0 || (dy == 0 && dx > 0); }

/* Caixa de píxeles válida: viewport interseccionado con el búfer. */
static void caixa(int *x0, int *y0, int *x1, int *y1)
{
    int ya = C->alt - C->vy - C->vh, yb = C->alt - C->vy;   /* filas, y de GL hacia arriba */
    *x0 = C->vx < 0 ? 0 : C->vx;
    *x1 = C->vx + C->vw > C->larg ? C->larg : C->vx + C->vw;
    *y0 = ya < 0 ? 0 : ya;
    *y1 = yb > C->alt ? C->alt : yb;
}

static void triangulo_fixo(sv_t v[3])
{
    double area = (double)(v[1].X - v[0].X) * (double)(v[2].Y - v[0].Y)
                - (double)(v[2].X - v[0].X) * (double)(v[1].Y - v[0].Y);
    if (area == 0) return;
    int frente = C->frente == GL_CCW ? area < 0 : area > 0;   /* el eje Y de pantalla va hacia abajo */
    if (C->cull) {
        if (C->cull_face == GL_BACK && !frente) return;
        if (C->cull_face == GL_FRONT && frente) return;
    }
    if (area < 0) { sv_t t = v[1]; v[1] = v[2]; v[2] = t; area = -area; }

    int cx0, cy0, cx1, cy1;
    caixa(&cx0, &cy0, &cx1, &cy1);
    int minX = v[0].X, maxX = v[0].X, minY = v[0].Y, maxY = v[0].Y;
    for (int i = 1; i < 3; i++) {
        if (v[i].X < minX) minX = v[i].X;
        if (v[i].X > maxX) maxX = v[i].X;
        if (v[i].Y < minY) minY = v[i].Y;
        if (v[i].Y > maxY) maxY = v[i].Y;
    }
    int bx0 = minX >> 4, bx1 = maxX >> 4, by0 = minY >> 4, by1 = maxY >> 4;
    if (bx0 < cx0) bx0 = cx0;
    if (by0 < cy0) by0 = cy0;
    if (bx1 > cx1 - 1) bx1 = cx1 - 1;
    if (by1 > cy1 - 1) by1 = cy1 - 1;
    if (bx0 > bx1 || by0 > by1) return;

    /* Aristas: e[i] es la opuesta al vértice i; E >= 0 dentro (con el sesgo -1 si no es top-left). */
    double e0[3], ex[3], ey[3];
    double Px = (double)bx0 * 16.0 + 8.0, Py = (double)by0 * 16.0 + 8.0;
    for (int i = 0; i < 3; i++) {
        const sv_t *a = &v[(i + 1) % 3], *b = &v[(i + 2) % 3];
        int dx = b->X - a->X, dy = b->Y - a->Y;
        e0[i] = (double)dx * (Py - (double)a->Y) - (double)dy * (Px - (double)a->X)
              - (canto_superior_esquerdo(dx, dy) ? 0.0 : 1.0);
        ex[i] = -(double)dy * 16.0;
        ey[i] = (double)dx * 16.0;
    }

    /* Gradientes por subpíxel, en Q16. */
    double gx[4], gy[4], a0[4];
    for (int k = 0; k < 4; k++) {
        double d1 = v[1].a[k] - v[0].a[k], d2 = v[2].a[k] - v[0].a[k];
        double nx = d1 * (double)(v[2].Y - v[0].Y) - d2 * (double)(v[1].Y - v[0].Y);
        double ny = d2 * (double)(v[1].X - v[0].X) - d1 * (double)(v[2].X - v[0].X);
        gx[k] = trun(nx * 65536.0 / area);
        gy[k] = trun(ny * 65536.0 / area);
        a0[k] = v[0].a[k] * 65536.0 + gx[k] * (Px - (double)v[0].X) + gy[k] * (Py - (double)v[0].Y);
    }

    for (int y = by0; y <= by1; y++) {
        double E0 = e0[0], E1 = e0[1], E2 = e0[2];
        double A0 = a0[0], A1 = a0[1], A2 = a0[2], A3 = a0[3];
        for (int x = bx0; x <= bx1; x++) {
            if (E0 >= 0 && E1 >= 0 && E2 >= 0)
                ponto_z(x, y, deszc(A0, 16), (int)deszc(A1, 24), (int)deszc(A2, 24), (int)deszc(A3, 24));
            E0 += ex[0]; E1 += ex[1]; E2 += ex[2];
            A0 += gx[0] * 16.0; A1 += gx[1] * 16.0; A2 += gx[2] * 16.0; A3 += gx[3] * 16.0;
        }
        e0[0] += ey[0]; e0[1] += ey[1]; e0[2] += ey[2];
        a0[0] += gy[0] * 16.0; a0[1] += gy[1] * 16.0; a0[2] += gy[2] * 16.0; a0[3] += gy[3] * 16.0;
    }
}

/* ------------------------------------------------- v6: ruta de texturas,
 * mezcla, niebla y luces.
 *
 * La ruta de referencia (v5) sigue igual cuando no hay ninguna de estas
 * cosas activadas, así que los píxeles de antes no cambian. Cuando sí las
 * hay, el recorrido es por bloques de 8x8 y el sombreado se hace de cuatro
 * píxeles en cuatro (SSE2) o de uno en uno (referencia); las dos rutas dan
 * exactamente el mismo píxel (lo comprueba la prueba h101). */

/* Un píxel ya interpolado y preparado. */
typedef struct {
    int    x, y;
    double z;               /* Q0 (31 bits) */
    int    r, g, b;         /* 0..255 */
    int    u, v;            /* Q8 (0..256 por unidad de textura) */
    int    a;               /* alfa 0..255 */
    int    luz;             /* 0..255 */
    int    tr, tg, tb, ta;  /* texel 0..255 (255 si no hay textura) */
    float  zn;              /* z normalizado (para la niebla) */
} pix_t;

/* Texel del vecino más próximo. */
static void amostra_textura(pix_t *p)
{
    const textura_t *t = 0;
    if (C->tex_on && C->tex_sel >= 0 && C->tex[C->tex_sel].usada) t = &C->tex[C->tex_sel];
    if (!t || t->larg <= 0 || t->alt <= 0) { p->tr = p->tg = p->tb = p->ta = 255; return; }
    int u = p->u, v = p->v;
    if (u > 65536) u = 65536; else if (u < -65536) u = -65536;
    if (v > 65536) v = 65536; else if (v < -65536) v = -65536;
    int x = (u * t->larg) / 256;
    int y = (v * t->alt) / 256;
    if (t->wrap_s == GL_REPEAT) { x = x % t->larg; if (x < 0) x += t->larg; }
    else { if (x < 0) x = 0; else if (x >= t->larg) x = t->larg - 1; }
    if (t->wrap_t == GL_REPEAT) { y = y % t->alt; if (y < 0) y += t->alt; }
    else { if (y < 0) y = 0; else if (y >= t->alt) y = t->alt - 1; }
    const unsigned char *q = t->px + ((size_t)y * (size_t)t->larg + (size_t)x) * 4;
    p->tr = q[0]; p->tg = q[1]; p->tb = q[2]; p->ta = q[3];
}

/* Ruta de referencia: un píxel a la vez. */
static void sombrear_ref(const pix_t *p, int n, float *cor)
{
    for (int i = 0; i < n; i++) {
        const pix_t *q = &p[i];
        float vr = (float)q->r / 255.0f;
        float vg = (float)q->g / 255.0f;
        float vb = (float)q->b / 255.0f;
        float m  = (float)q->luz / 255.0f;
        float mr = m * C->luz_dif[0] + C->luz_amb[0];
        float mg = m * C->luz_dif[1] + C->luz_amb[1];
        float mb = m * C->luz_dif[2] + C->luz_amb[2];
        float fr = ((float)q->tr * vr) * mr;
        float fg = ((float)q->tg * vg) * mg;
        float fb = ((float)q->tb * vb) * mb;
        /* El alfa va de 0 a 1 (como los factores de la mezcla): el del
         * texel (0..255) por el del vértice (0..255), dividido por 255*255. */
        float fa = ((float)q->ta * (float)q->a) / 65025.0f;
        if (C->fog) {
            float f = (C->fog_fim - q->zn) / (C->fog_fim - C->fog_ini);
            if (f < 0.0f) f = 0.0f;
            else if (f > 1.0f) f = 1.0f;
            fr = fr * f + C->fog_cor[0] * (1.0f - f);
            fg = fg * f + C->fog_cor[1] * (1.0f - f);
            fb = fb * f + C->fog_cor[2] * (1.0f - f);
        }
        /* Disposición en plano (como la deja la ruta SIMD): los cuatro
         * rojos, los cuatro verdes, los cuatro azules y los cuatro alfas. */
        cor[i] = fr; cor[4 + i] = fg; cor[8 + i] = fb; cor[12 + i] = fa;
    }
}

#ifdef SGL_TEM_SIMD
/* Ruta SIMD: las mismas operaciones, cuatro píxeles a la vez. Las divisiones
 * y los productos son los mismos que en la referencia (SSE2 redondea como
 * el escalar), así que el píxel es el mismo bit a bit. */
static void sombrear_simd(const pix_t *p, int n, float *cor)
{
    float tr[4], tg[4], tb[4], ta[4], vr[4], vg[4], vb[4], al[4], ml[4], zn[4];
    for (int i = 0; i < 4; i++) {
        int j = i < n ? i : 0;
        tr[i] = (float)p[j].tr; tg[i] = (float)p[j].tg;
        tb[i] = (float)p[j].tb; ta[i] = (float)p[j].ta;
        vr[i] = (float)p[j].r / 255.0f;
        vg[i] = (float)p[j].g / 255.0f;
        vb[i] = (float)p[j].b / 255.0f;
        al[i] = (float)p[j].a;
        ml[i] = (float)p[j].luz / 255.0f;
        zn[i] = p[j].zn;
    }
    __m128 TR = _mm_loadu_ps(tr), TG = _mm_loadu_ps(tg), TB = _mm_loadu_ps(tb), TA = _mm_loadu_ps(ta);
    __m128 VR = _mm_loadu_ps(vr), VG = _mm_loadu_ps(vg), VB = _mm_loadu_ps(vb);
    __m128 AL = _mm_loadu_ps(al), ML = _mm_loadu_ps(ml);
    __m128 MR = _mm_add_ps(_mm_mul_ps(ML, _mm_set1_ps(C->luz_dif[0])), _mm_set1_ps(C->luz_amb[0]));
    __m128 MG = _mm_add_ps(_mm_mul_ps(ML, _mm_set1_ps(C->luz_dif[1])), _mm_set1_ps(C->luz_amb[1]));
    __m128 MB = _mm_add_ps(_mm_mul_ps(ML, _mm_set1_ps(C->luz_dif[2])), _mm_set1_ps(C->luz_amb[2]));
    __m128 FR = _mm_mul_ps(_mm_mul_ps(TR, VR), MR);
    __m128 FG = _mm_mul_ps(_mm_mul_ps(TG, VG), MG);
    __m128 FB = _mm_mul_ps(_mm_mul_ps(TB, VB), MB);
    /* El alfa va de 0 a 1: el del texel (0..255) por el del vértice (0..255),
     * dividido por 255*255 (la misma división que en la referencia, para que
     * los dos caminos den el mismo píxel bit a bit). */
    __m128 FA = _mm_div_ps(_mm_mul_ps(TA, AL), _mm_set1_ps(65025.0f));
    if (C->fog) {
        __m128 F = _mm_div_ps(_mm_sub_ps(_mm_set1_ps(C->fog_fim), _mm_loadu_ps(zn)),
                              _mm_sub_ps(_mm_set1_ps(C->fog_fim), _mm_set1_ps(C->fog_ini)));
        F = _mm_min_ps(_mm_max_ps(F, _mm_setzero_ps()), _mm_set1_ps(1.0f));
        __m128 UMF = _mm_sub_ps(_mm_set1_ps(1.0f), F);
        FR = _mm_add_ps(_mm_mul_ps(FR, F), _mm_mul_ps(_mm_set1_ps(C->fog_cor[0]), UMF));
        FG = _mm_add_ps(_mm_mul_ps(FG, F), _mm_mul_ps(_mm_set1_ps(C->fog_cor[1]), UMF));
        FB = _mm_add_ps(_mm_mul_ps(FB, F), _mm_mul_ps(_mm_set1_ps(C->fog_cor[2]), UMF));
    }
    _mm_storeu_ps(cor, FR);
    _mm_storeu_ps(cor + 4, FG);
    _mm_storeu_ps(cor + 8, FB);
    _mm_storeu_ps(cor + 12, FA);
}
#endif

/* Escribe el píxel: prueba de Z, mezcla y color, en el mismo orden que en la
 * ruta de referencia. */
static void escreve_v6(int x, int y, double z, float fr, float fg, float fb, float fa)
{
    if (z < 0) z = 0;
    if (z > 0x7FFFFFFE) z = 0x7FFFFFFE;
    int i = y * C->larg + x;
    if (C->teste_z) {
        if (C->zbits == 32) {
            uint32_t *zb = C->zbuf, zi = (uint32_t)z;
            if (C->func_z == GL_LESS ? !(zi < zb[i]) : C->func_z == GL_LEQUAL ? !(zi <= zb[i]) : 0) return;
            zb[i] = zi;
        } else {
            uint16_t *zb = C->zbuf;
            uint32_t zi = (uint32_t)deszc(z, 15);
            if (zi > 0xFFFE) zi = 0xFFFE;
            if (C->func_z == GL_LESS ? !(zi < zb[i]) : C->func_z == GL_LEQUAL ? !(zi <= zb[i]) : 0) return;
            zb[i] = (uint16_t)zi;
        }
    }
    if (C->blend) {
        uint32_t d = C->px[i];
        float dr = (float)((d >> 16) & 0xFF), dg = (float)((d >> 8) & 0xFF), db = (float)(d & 0xFF);
        float fs = 1.0f, fd = 1.0f;
        if (C->blend_s == GL_SRC_ALPHA) fs = fa;
        else if (C->blend_s == GL_ONE_MINUS_SRC_ALPHA) fs = 1.0f - fa;
        else if (C->blend_s == GL_ZERO) fs = 0.0f;
        if (C->blend_d == GL_SRC_ALPHA) fd = fa;
        else if (C->blend_d == GL_ONE_MINUS_SRC_ALPHA) fd = 1.0f - fa;
        else if (C->blend_d == GL_ZERO) fd = 0.0f;
        fr = fr * fs + dr * fd;
        fg = fg * fs + dg * fd;
        fb = fb * fs + db * fd;
    }
    int r = (int)(fr + 0.5f); if (r < 0) r = 0; else if (r > 255) r = 255;
    int g = (int)(fg + 0.5f); if (g < 0) g = 0; else if (g > 255) g = 255;
    int b = (int)(fb + 0.5f); if (b < 0) b = 0; else if (b > 255) b = 255;
    C->px[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void pinta_lote(const pix_t *p, int n)
{
    float cor[16];
#ifdef SGL_TEM_SIMD
    if (sgl_simd) sombrear_simd(p, n, cor);
    else sombrear_ref(p, n, cor);
#else
    sombrear_ref(p, n, cor);
#endif
    for (int i = 0; i < n; i++)
        escreve_v6(p[i].x, p[i].y, p[i].z, cor[i], cor[4 + i], cor[8 + i], cor[12 + i]);
}

/* Rasterizador v6: mismas aristas y gradientes que el de referencia, pero el
 * recorrido es por bloques de 8x8 y el sombreado va de cuatro en cuatro. */
static void triangulo_v6(sv_t v[3])
{
    double area = (double)(v[1].X - v[0].X) * (double)(v[2].Y - v[0].Y)
                - (double)(v[2].X - v[0].X) * (double)(v[1].Y - v[0].Y);
    if (area == 0) return;
    int frente = C->frente == GL_CCW ? area < 0 : area > 0;
    if (C->cull) {
        if (C->cull_face == GL_BACK && !frente) return;
        if (C->cull_face == GL_FRONT && frente) return;
    }
    if (area < 0) { sv_t t = v[1]; v[1] = v[2]; v[2] = t; area = -area; }

    int cx0, cy0, cx1, cy1;
    caixa(&cx0, &cy0, &cx1, &cy1);
    int minX = v[0].X, maxX = v[0].X, minY = v[0].Y, maxY = v[0].Y;
    for (int i = 1; i < 3; i++) {
        if (v[i].X < minX) minX = v[i].X;
        if (v[i].X > maxX) maxX = v[i].X;
        if (v[i].Y < minY) minY = v[i].Y;
        if (v[i].Y > maxY) maxY = v[i].Y;
    }
    int bx0 = minX >> 4, bx1 = maxX >> 4, by0 = minY >> 4, by1 = maxY >> 4;
    if (bx0 < cx0) bx0 = cx0;
    if (by0 < cy0) by0 = cy0;
    if (bx1 > cx1 - 1) bx1 = cx1 - 1;
    if (by1 > cy1 - 1) by1 = cy1 - 1;
    if (bx0 > bx1 || by0 > by1) return;

    double e0[3], ex[3], ey[3];
    double Px = (double)bx0 * 16.0 + 8.0, Py = (double)by0 * 16.0 + 8.0;
    for (int i = 0; i < 3; i++) {
        const sv_t *a = &v[(i + 1) % 3], *b = &v[(i + 2) % 3];
        int dx = b->X - a->X, dy = b->Y - a->Y;
        e0[i] = (double)dx * (Py - (double)a->Y) - (double)dy * (Px - (double)a->X)
              - (canto_superior_esquerdo(dx, dy) ? 0.0 : 1.0);
        ex[i] = -(double)dy * 16.0;
        ey[i] = (double)dx * 16.0;
    }

    double gx[8], gy[8], a0[8];
    for (int k = 0; k < 8; k++) {
        double d1 = v[1].a[k] - v[0].a[k], d2 = v[2].a[k] - v[0].a[k];
        double nx = d1 * (double)(v[2].Y - v[0].Y) - d2 * (double)(v[1].Y - v[0].Y);
        double ny = d2 * (double)(v[1].X - v[0].X) - d1 * (double)(v[2].X - v[0].X);
        gx[k] = trun(nx * 65536.0 / area);
        gy[k] = trun(ny * 65536.0 / area);
        a0[k] = v[0].a[k] * 65536.0 + gx[k] * (Px - (double)v[0].X) + gy[k] * (Py - (double)v[0].Y);
    }

    pix_t lote[4];
    int nl = 0;
    for (int by = by0; by <= by1; by += 8) {
        for (int bx = bx0; bx <= bx1; bx += 8) {
            int xa = bx, xb = bx + 7, ya = by, yb = by + 7;
            if (xb > bx1) xb = bx1;
            if (yb > by1) yb = by1;
            double E[3], A[8];
            for (int i = 0; i < 3; i++)
                E[i] = e0[i] + ex[i] * (double)(bx - bx0) + ey[i] * (double)(by - by0);
            for (int k = 0; k < 8; k++)
                A[k] = a0[k] + gx[k] * 16.0 * (double)(bx - bx0) + gy[k] * 16.0 * (double)(by - by0);
            for (int y = ya; y <= yb; y++) {
                double E0 = E[0], E1 = E[1], E2 = E[2];
                double A0 = A[0], A1 = A[1], A2 = A[2], A3 = A[3];
                double A4 = A[4], A5 = A[5], A6 = A[6], A7 = A[7];
                for (int x = xa; x <= xb; x++) {
                    if (E0 >= 0 && E1 >= 0 && E2 >= 0) {
                        pix_t *q = &lote[nl];
                        q->x = x; q->y = y;
                        q->z = deszc(A0, 16);
                        q->r = (int)deszc(A1, 24);
                        q->g = (int)deszc(A2, 24);
                        q->b = (int)deszc(A3, 24);
                        /* u y v vuelven en Q8 (256 por unidad de textura): los
                         * atributos se guardaron en Q8 (u*256), así que se
                         * recuperan con 16 bits de desplazamiento, no con 24
                         * (con 24 salía u en unidades y la textura se muestreaba
                         * siempre en el texel 0). */
                        q->u = (int)deszc(A4, 16); q->v = (int)deszc(A5, 16);
                        q->a = (int)deszc(A6, 24);
                        q->luz = (int)deszc(A7, 24);
                        q->zn = (float)(q->z * (1.0 / 2147483520.0));
                        amostra_textura(q);
                        nl++;
                        if (nl == 4) { pinta_lote(lote, 4); nl = 0; }
                    }
                    E0 += ex[0]; E1 += ex[1]; E2 += ex[2];
                    A0 += gx[0] * 16.0; A1 += gx[1] * 16.0; A2 += gx[2] * 16.0; A3 += gx[3] * 16.0;
                    A4 += gx[4] * 16.0; A5 += gx[5] * 16.0; A6 += gx[6] * 16.0; A7 += gx[7] * 16.0;
                }
                E[0] += ey[0]; E[1] += ey[1]; E[2] += ey[2];
                for (int k = 0; k < 8; k++) A[k] += gy[k] * 16.0;
            }
        }
    }
    if (nl) pinta_lote(lote, nl);
}

/* ------------------------------------------------------------ recorte y proyección */

static inline int arred16(float v)
{
    if (v > 1.0e6f) v = 1.0e6f;
    if (v < -1.0e6f) v = -1.0e6f;
    return (int)(v * 16.0f + (v >= 0 ? 0.5f : -0.5f));
}

static void a_pantalha(const vert_t *v, sv_t *s)
{
    float iw = 1.0f / v->w;
    float nx = v->x * iw, ny = v->y * iw, nz = v->z * iw;
    float sx = (float)C->vx + (nx + 1.0f) * 0.5f * (float)C->vw;
    float sy = (float)(C->alt - C->vy) - (ny + 1.0f) * 0.5f * (float)C->vh;
    float zw = (nz + 1.0f) * 0.5f;
    if (zw < 0) zw = 0;
    if (zw > 1) zw = 1;
    s->X = arred16(sx);
    s->Y = arred16(sy);
    s->a[0] = (double)(int)(zw * 2147483520.0f);
    s->a[1] = (double)(int)(v->r * 256.0f + 0.5f);
    s->a[2] = (double)(int)(v->g * 256.0f + 0.5f);
    s->a[3] = (double)(int)(v->b * 256.0f + 0.5f);
    /* u y v en Q8 (0..256 por unidad): con Q16 los gradientes en Q16 se
     * salían del rango de los enteros de 32 bits. */
    s->a[4] = (double)(int)(v->u * 256.0f + 0.5f);
    s->a[5] = (double)(int)(v->v * 256.0f + 0.5f);
    s->a[6] = (double)(int)(v->a * 256.0f + 0.5f);
    s->a[7] = (double)(int)(v->luz * 256.0f + 0.5f);
}

static inline float dist_plano(const vert_t *v, int p)
{
    switch (p) {
    case 0: return v->z + v->w;   /* near */
    case 1: return v->w - v->z;   /* far */
    case 2: return v->x + v->w;
    case 3: return v->w - v->x;
    case 4: return v->y + v->w;
    default: return v->w - v->y;
    }
}

static void interpolar(vert_t *r, const vert_t *a, const vert_t *b, float t)
{
    r->x = a->x + (b->x - a->x) * t;
    r->y = a->y + (b->y - a->y) * t;
    r->z = a->z + (b->z - a->z) * t;
    r->w = a->w + (b->w - a->w) * t;
    r->r = a->r + (b->r - a->r) * t;
    r->g = a->g + (b->g - a->g) * t;
    r->b = a->b + (b->b - a->b) * t;
    r->u = a->u + (b->u - a->u) * t;
    r->v = a->v + (b->v - a->v) * t;
    r->a = a->a + (b->a - a->a) * t;
    r->luz = a->luz + (b->luz - a->luz) * t;
    r->ex = a->ex + (b->ex - a->ex) * t;
    r->ey = a->ey + (b->ey - a->ey) * t;
    r->ez = a->ez + (b->ez - a->ez) * t;
}

static void emitir_triangulo(vert_t t[3])
{
    vert_t pol[2][12];
    int n = 3, cur = 0;
    for (int i = 0; i < 3; i++) pol[0][i] = t[i];
    for (int p = 0; p < 6 && n >= 3; p++) {
        vert_t *in = pol[cur], *out = pol[cur ^ 1];
        int m = 0;
        for (int i = 0; i < n; i++) {
            const vert_t *a = &in[i], *b = &in[(i + 1) % n];
            float da = dist_plano(a, p), db = dist_plano(b, p);
            if (da >= 0) out[m++] = *a;
            if ((da >= 0) != (db >= 0)) interpolar(&out[m++], a, b, da / (da - db));
        }
        n = m; cur ^= 1;
    }
    if (n < 3) return;
    sv_t s[12];
    for (int i = 0; i < n; i++) a_pantalha(&pol[cur][i], &s[i]);
    for (int i = 1; i + 1 < n; i++) {
        sv_t tri[3] = { s[0], s[i], s[i + 1] };
        if (C->extras) triangulo_v6(tri);
        else triangulo_fixo(tri);
    }
}

static void emitir_linha(const vert_t *va, const vert_t *vb)
{
    vert_t a = *va, b = *vb;
    float t0 = 0, t1 = 1;
    for (int p = 0; p < 6; p++) {
        float da = dist_plano(&a, p), db = dist_plano(&b, p);
        if (da < 0 && db < 0) return;
        if (da < 0) { vert_t r; interpolar(&r, &a, &b, da / (da - db)); a = r; }
        else if (db < 0) { vert_t r; interpolar(&r, &a, &b, da / (da - db)); b = r; }
    }
    (void)t0; (void)t1;
    sv_t s0, s1;
    a_pantalha(&a, &s0);
    a_pantalha(&b, &s1);
    int x0 = s0.X >> 4, y0 = s0.Y >> 4, x1 = s1.X >> 4, y1 = s1.Y >> 4;
    int cx0, cy0, cx1, cy1;
    caixa(&cx0, &cy0, &cx1, &cy1);
    int dx = x1 - x0, dy = y1 - y0;
    int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    for (int i = 0; i <= n; i++) {
        int x, y;
        double z, c[3];
        if (n == 0) { x = x0; y = y0; z = s0.a[0]; for (int k = 0; k < 3; k++) c[k] = s0.a[k + 1]; }
        else {
            x = x0 + (int)trun((double)(dx * i * 2 + (dx < 0 ? -n : n)) / (2.0 * (double)n));
            y = y0 + (int)trun((double)(dy * i * 2 + (dy < 0 ? -n : n)) / (2.0 * (double)n));
            z = s0.a[0] + trun((s1.a[0] - s0.a[0]) * (double)i / (double)n);
            for (int k = 0; k < 3; k++)
                c[k] = s0.a[k + 1] + trun((s1.a[k + 1] - s0.a[k + 1]) * (double)i / (double)n);
        }
        if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1) continue;
        ponto_z(x, y, z, (int)deszc(c[0], 8), (int)deszc(c[1], 8), (int)deszc(c[2], 8));
    }
}

/* ------------------------------------------------------------ vértices y primitivos */

typedef struct {
    const float *v; int v_tam, v_passo;          /* GL_FLOAT */
    const void  *c; int c_tam, c_tipo, c_passo;
    int          tem_cor;
    float        cor[4];
    const void  *tc; int tc_tam, tc_tipo, tc_passo;
    int          tem_tc;
    const float *tcv;          /* coordenadas por vértice (glBegin) */
    float        tc_uv[2];
} fonte_t;

static void buscar(const fonte_t *f, int i, vert_t *o)
{
    const float *p = (const float *)((const char *)f->v + (size_t)i * f->v_passo);
    float m[4] = { p[0], p[1], f->v_tam > 2 ? p[2] : 0.0f, f->v_tam > 3 ? p[3] : 1.0f };
    const float *M = C->mv[C->mv_n - 1], *P = C->proj[C->proj_n - 1];
    float e[4];
    for (int l = 0; l < 4; l++) e[l] = M[l] * m[0] + M[4 + l] * m[1] + M[8 + l] * m[2] + M[12 + l] * m[3];
    o->x = P[0] * e[0] + P[4] * e[1] + P[8] * e[2] + P[12] * e[3];
    o->y = P[1] * e[0] + P[5] * e[1] + P[9] * e[2] + P[13] * e[3];
    o->z = P[2] * e[0] + P[6] * e[1] + P[10] * e[2] + P[14] * e[3];
    o->w = P[3] * e[0] + P[7] * e[1] + P[11] * e[2] + P[15] * e[3];
    o->ex = e[0]; o->ey = e[1]; o->ez = e[2];
    o->u = f->tc_uv[0]; o->v = f->tc_uv[1];
    if (f->tcv) {
        o->u = f->tcv[i * 2]; o->v = f->tcv[i * 2 + 1];
    } else if (f->tem_tc && f->tc) {
        const char *q = (const char *)f->tc + (size_t)i * f->tc_passo;
        if (f->tc_tipo == GL_UNSIGNED_BYTE) {
            o->u = (float)((const uint8_t *)q)[0] * (1.0f / 255.0f);
            o->v = (float)((const uint8_t *)q)[1] * (1.0f / 255.0f);
        } else {
            o->u = ((const float *)q)[0];
            o->v = ((const float *)q)[1];
        }
    }
    o->a = 255.0f;
    if (f->tem_cor) {
        const char *q = (const char *)f->c + (size_t)i * f->c_passo;
        for (int k = 0; k < 3; k++) {
            float c = f->c_tipo == GL_UNSIGNED_BYTE ? (float)((const uint8_t *)q)[k] : ((const float *)q)[k] * 255.0f;
            if (c < 0) c = 0;
            if (c > 255) c = 255;
            (&o->r)[k] = c;
        }
        if (f->c_tam > 3) {
            float a = f->c_tipo == GL_UNSIGNED_BYTE ? (float)((const uint8_t *)q)[3] : ((const float *)q)[3] * 255.0f;
            if (a < 0) a = 0;
            if (a > 255) a = 255;
            o->a = a;
        }
    } else {
        o->r = f->cor[0]; o->g = f->cor[1]; o->b = f->cor[2]; o->a = f->cor[3];
    }
    o->luz = 255.0f;
}

/* Luz difusa por cara: normal de la cara en el espacio de la vista y producto
 * escalar con la dirección de la luz (plana, como el sombreado GL_FLAT). */
static void luz_cara(vert_t *t)
{
    float ux = t[1].ex - t[0].ex, uy = t[1].ey - t[0].ey, uz = t[1].ez - t[0].ez;
    float vx = t[2].ex - t[0].ex, vy = t[2].ey - t[0].ey, vz = t[2].ez - t[0].ez;
    float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    float ln = raiz(nx * nx + ny * ny + nz * nz);
    float lx, ly, lz;
    if (C->luz_pos[3] == 0.0f) {
        lx = C->luz_pos[0]; ly = C->luz_pos[1]; lz = C->luz_pos[2];
    } else {
        lx = (t[0].ex + t[1].ex + t[2].ex) * (1.0f / 3.0f) - C->luz_pos[0];
        ly = (t[0].ey + t[1].ey + t[2].ey) * (1.0f / 3.0f) - C->luz_pos[1];
        lz = (t[0].ez + t[1].ez + t[2].ez) * (1.0f / 3.0f) - C->luz_pos[2];
    }
    float dl = raiz(lx * lx + ly * ly + lz * lz);
    float d = 0.0f;
    if (ln > 0.0f && dl > 0.0f) {
        d = (nx * lx + ny * ly + nz * lz) / (ln * dl);
        if (d < 0.0f) d = 0.0f;
        else if (d > 1.0f) d = 1.0f;
    }
    for (int i = 0; i < 3; i++) t[i].luz = d * 255.0f;
}

static void tri_prim(const vert_t *a, const vert_t *b, const vert_t *c)
{
    vert_t t[3] = { *a, *b, *c };
    if (C->plano)
        for (int i = 0; i < 3; i++) { t[i].r = c->r; t[i].g = c->g; t[i].b = c->b; }
    if (C->luz_on) luz_cara(t);
    emitir_triangulo(t);
}

static void linha_prim(const vert_t *a, const vert_t *b)
{
    vert_t u = *a, w = *b;
    if (C->plano) { u.r = w.r; u.g = w.g; u.b = w.b; }
    emitir_linha(&u, &w);
}

/* idx = NULL: índices consecutivos desde primeiro. */
static void desenhar(GLenum modo, int primeiro, int n, const void *idx, GLenum tipo, const fonte_t *f)
{
    #define VERT(k, out) do { int _i = idx ? (tipo == GL_UNSIGNED_BYTE ? ((const uint8_t *)idx)[k] : ((const uint16_t *)idx)[k]) : primeiro + (k); buscar(f, _i, &out); } while (0)
    vert_t a, b, c;
    switch (modo) {
    case GL_TRIANGLES:
        for (int i = 0; i + 2 < n; i += 3) { VERT(i, a); VERT(i + 1, b); VERT(i + 2, c); tri_prim(&a, &b, &c); }
        break;
    case GL_TRIANGLE_STRIP:
        for (int i = 0; i + 2 < n; i++) {
            VERT(i, a); VERT(i + 1, b); VERT(i + 2, c);
            if (i & 1) tri_prim(&b, &a, &c); else tri_prim(&a, &b, &c);
        }
        break;
    case GL_TRIANGLE_FAN:
        if (n >= 3) VERT(0, a);
        for (int i = 1; i + 1 < n; i++) { VERT(i, b); VERT(i + 1, c); tri_prim(&a, &b, &c); }
        break;
    case GL_LINES:
        for (int i = 0; i + 1 < n; i += 2) { VERT(i, a); VERT(i + 1, b); linha_prim(&a, &b); }
        break;
    case GL_LINE_STRIP:
        for (int i = 0; i + 1 < n; i++) { VERT(i, a); VERT(i + 1, b); linha_prim(&a, &b); }
        break;
    case GL_LINE_LOOP:
        for (int i = 0; i < n && n >= 2; i++) { VERT(i, a); VERT((i + 1) % n, b); linha_prim(&a, &b); }
        break;
    default: erro(GL_INVALID_ENUM);
    }
    #undef VERT
}

void glEnableClientState(GLenum m) { if (!C) return; if (m == GL_VERTEX_ARRAY) C->v_ativo = 1; else if (m == GL_COLOR_ARRAY) C->c_ativo = 1; else if (m == GL_TEXTURE_COORD_ARRAY) C->t_ativo = 1; else erro(GL_INVALID_ENUM); }
void glDisableClientState(GLenum m) { if (!C) return; if (m == GL_VERTEX_ARRAY) C->v_ativo = 0; else if (m == GL_COLOR_ARRAY) C->c_ativo = 0; else if (m == GL_TEXTURE_COORD_ARRAY) C->t_ativo = 0; else erro(GL_INVALID_ENUM); }

void glVertexPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (tam < 2 || tam > 4 || tipo != GL_FLOAT || passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->v_tam = tam; C->v_passo = passo ? passo : tam * 4; C->v_ptr = p;
}

void glColorPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (tam < 3 || tam > 4 || (tipo != GL_FLOAT && tipo != GL_UNSIGNED_BYTE) || passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->c_tam = tam; C->c_tipo = (int)tipo; C->c_passo = passo ? passo : tam * (tipo == GL_FLOAT ? 4 : 1); C->c_ptr = p;
}

static int fonte_do_contexto(fonte_t *f)
{
    if (!C->v_ativo || !C->v_ptr) { erro(GL_INVALID_OPERATION); return 0; }
    f->v = C->v_ptr; f->v_tam = C->v_tam; f->v_passo = C->v_passo;
    f->tem_cor = C->c_ativo && C->c_ptr;
    f->c = C->c_ptr; f->c_tam = C->c_tam; f->c_tipo = C->c_tipo; f->c_passo = C->c_passo;
    for (int k = 0; k < 4; k++) f->cor[k] = C->cor[k];
    f->tcv = 0;
    f->tem_tc = C->t_ativo && C->t_ptr;
    f->tc = C->t_ptr; f->tc_tam = C->t_tam; f->tc_tipo = C->t_tipo; f->tc_passo = C->t_passo;
    f->tc_uv[0] = C->tc[0]; f->tc_uv[1] = C->tc[1];
    return 1;
}

void glDrawArrays(GLenum modo, GLint primeiro, GLsizei n)
{
    fonte_t f;
    if (!C || n < 0 || primeiro < 0) return;
    if (!fonte_do_contexto(&f)) return;
    desenhar(modo, primeiro, n, 0, 0, &f);
}

void glDrawElements(GLenum modo, GLsizei n, GLenum tipo, const void *indices)
{
    fonte_t f;
    if (!C || n < 0) return;
    if (tipo != GL_UNSIGNED_BYTE && tipo != GL_UNSIGNED_SHORT) { erro(GL_INVALID_ENUM); return; }
    if (!fonte_do_contexto(&f)) return;
    desenhar(modo, 0, n, indices, tipo, &f);
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    if (!C) return;
    float v[4] = { r, g, b, a };
    for (int k = 0; k < 4; k++) C->cor[k] = v[k] <= 0 ? 0.0f : v[k] >= 1 ? 255.0f : v[k] * 255.0f;
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) { glColor4f(r, g, b, 1.0f); }

void glBegin(GLenum modo)
{
    if (!C) return;
    if (C->em_begin) { erro(GL_INVALID_OPERATION); return; }
    C->em_begin = 1; C->begin_modo = (int)modo; C->begin_n = 0;
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (!C || !C->em_begin) { erro(GL_INVALID_OPERATION); return; }
    if (C->begin_n >= MAX_BEGIN) return;
    int i = C->begin_n++;
    C->begin_v[i][0] = x; C->begin_v[i][1] = y; C->begin_v[i][2] = z;
    for (int k = 0; k < 4; k++) C->begin_c[i][k] = C->cor[k] * (1.0f / 255.0f);
    C->begin_t[i][0] = C->tc[0]; C->begin_t[i][1] = C->tc[1];
}

void glVertex2f(GLfloat x, GLfloat y) { glVertex3f(x, y, 0.0f); }

void glEnd(void)
{
    if (!C || !C->em_begin) { erro(GL_INVALID_OPERATION); return; }
    C->em_begin = 0;
    fonte_t f;
    f.v = &C->begin_v[0][0]; f.v_tam = 3; f.v_passo = 12;
    f.c = &C->begin_c[0][0]; f.c_tam = 4; f.c_tipo = GL_FLOAT; f.c_passo = 16; f.tem_cor = 1;
    f.cor[0] = f.cor[1] = f.cor[2] = f.cor[3] = 255.0f;
    f.tem_tc = 0; f.tc = 0; f.tc_tam = 2; f.tc_tipo = GL_FLOAT; f.tc_passo = 8;
    f.tcv = &C->begin_t[0][0];
    f.tc_uv[0] = C->tc[0]; f.tc_uv[1] = C->tc[1];
    desenhar((GLenum)C->begin_modo, 0, C->begin_n, 0, 0, &f);
}

/* ------------------------------------------------------------ v6: texturas */

static textura_t *textura_por_id(GLuint id)
{
    for (int i = 0; i < MAX_TEX; i++)
        if (C->tex[i].usada && C->tex[i].id == id) return &C->tex[i];
    return 0;
}

static int textura_livre(void)
{
    for (int i = 0; i < MAX_TEX; i++) if (!C->tex[i].usada) return i;
    return -1;
}

void glGenTextures(GLsizei n, GLuint *ids)
{
    if (!C || !ids || n < 0) return;
    for (int i = 0; i < n; i++) {
        int k = textura_livre();
        if (k < 0) { ids[i] = 0; continue; }
        C->tex[k].usada = 1;
        C->tex[k].id = ids[i] ? ids[i] : (GLuint)(k + 1);
        C->tex[k].larg = C->tex[k].alt = 0;
        C->tex[k].px = 0;
        C->tex[k].wrap_s = C->tex[k].wrap_t = GL_REPEAT;
        ids[i] = C->tex[k].id;
    }
}

void glBindTexture(GLenum alvo, GLuint id)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D) { erro(GL_INVALID_ENUM); return; }
    if (id == 0) { C->tex_sel = -1; return; }
    textura_t *t = textura_por_id(id);
    if (!t) {
        int k = textura_livre();
        if (k < 0) { erro(GL_INVALID_OPERATION); return; }
        t = &C->tex[k];
        t->usada = 1; t->id = id; t->larg = t->alt = 0; t->px = 0;
        t->wrap_s = t->wrap_t = GL_REPEAT;
    }
    C->tex_sel = (int)(t - C->tex);
}

void glTexImage2D(GLenum alvo, GLint nivel, GLint formato_interno, GLsizei larg,
                   GLsizei alt, GLint bord, GLenum formato, GLenum tipo, const void *dados)
{
    (void)nivel; (void)formato_interno; (void)bord;
    if (!C) return;
    if (alvo != GL_TEXTURE_2D) { erro(GL_INVALID_ENUM); return; }
    if (C->tex_sel < 0) { erro(GL_INVALID_OPERATION); return; }
    if (larg <= 0 || alt <= 0 || larg > 1024 || alt > 1024) { erro(GL_INVALID_VALUE); return; }
    if (tipo != GL_UNSIGNED_BYTE) { erro(GL_INVALID_ENUM); return; }
    if (formato != GL_RGB && formato != GL_RGBA) { erro(GL_INVALID_ENUM); return; }
    if (!dados) { erro(GL_INVALID_VALUE); return; }
    textura_t *t = &C->tex[C->tex_sel];
    if (t->px) SGL_LIBERAR(t->px);
    t->px = (unsigned char *)SGL_ALOCAR((size_t)larg * (size_t)alt * 4);
    if (!t->px) { erro(GL_OUT_OF_MEMORY); return; }
    t->larg = larg; t->alt = alt;
    int comp = (formato == GL_RGBA) ? 4 : 3;
    const unsigned char *q = (const unsigned char *)dados;
    for (int i = 0; i < larg * alt; i++) {
        t->px[i * 4 + 0] = q[i * comp + 0];
        t->px[i * 4 + 1] = q[i * comp + 1];
        t->px[i * 4 + 2] = q[i * comp + 2];
        t->px[i * 4 + 3] = (comp == 4) ? q[i * comp + 3] : 255;
    }
    refaz_extras();
}

void glTexParameteri(GLenum alvo, GLenum nome, GLint valor)
{
    if (!C) return;
    if (alvo != GL_TEXTURE_2D) { erro(GL_INVALID_ENUM); return; }
    if (C->tex_sel < 0) { erro(GL_INVALID_OPERATION); return; }
    textura_t *t = &C->tex[C->tex_sel];
    if (nome == GL_TEXTURE_WRAP_S) {
        if (valor != GL_REPEAT && valor != GL_CLAMP_TO_EDGE) { erro(GL_INVALID_ENUM); return; }
        t->wrap_s = valor;
    } else if (nome == GL_TEXTURE_WRAP_T) {
        if (valor != GL_REPEAT && valor != GL_CLAMP_TO_EDGE) { erro(GL_INVALID_ENUM); return; }
        t->wrap_t = valor;
    } else if (nome == GL_TEXTURE_MIN_FILTER || nome == GL_TEXTURE_MAG_FILTER) {
        /* Solo el vecino más próximo: el filtrado bilineal no está. */
        if (valor != GL_NEAREST) { erro(GL_INVALID_ENUM); return; }
    } else erro(GL_INVALID_ENUM);
}

void glDeleteTextures(GLsizei n, const GLuint *ids)
{
    if (!C || !ids || n < 0) return;
    for (int i = 0; i < n; i++) {
        textura_t *t = textura_por_id(ids[i]);
        if (!t) continue;
        if (t->px) SGL_LIBERAR(t->px);
        t->px = 0; t->usada = 0; t->larg = t->alt = 0;
        if (C->tex_sel == (int)(t - C->tex)) C->tex_sel = -1;
    }
}

void glTexCoord2f(GLfloat u, GLfloat v)
{
    if (!C) return;
    C->tc[0] = u; C->tc[1] = v;
}

void glTexCoordPointer(GLint tam, GLenum tipo, GLsizei passo, const void *p)
{
    if (!C) return;
    if (tam != 2 || (tipo != GL_FLOAT && tipo != GL_UNSIGNED_BYTE) || passo < 0) { erro(GL_INVALID_VALUE); return; }
    C->t_tam = tam; C->t_tipo = (int)tipo;
    C->t_passo = passo ? passo : tam * (tipo == GL_FLOAT ? 4 : 1);
    C->t_ptr = p;
}

/* ------------------------------------------------------------ v6: mezcla, niebla y luces */

void glBlendFunc(GLenum src, GLenum dst)
{
    if (!C) return;
    if (src != GL_SRC_ALPHA && src != GL_ONE_MINUS_SRC_ALPHA && src != GL_ONE && src != GL_ZERO) { erro(GL_INVALID_ENUM); return; }
    if (dst != GL_SRC_ALPHA && dst != GL_ONE_MINUS_SRC_ALPHA && dst != GL_ONE && dst != GL_ZERO) { erro(GL_INVALID_ENUM); return; }
    C->blend_s = (int)src; C->blend_d = (int)dst;
}

void glFogf(GLenum nome, GLfloat valor)
{
    if (!C) return;
    if (nome == GL_FOG_START) C->fog_ini = valor;
    else if (nome == GL_FOG_END) C->fog_fim = valor;
    else erro(GL_INVALID_ENUM);
}

void glFogfv(GLenum nome, const GLfloat *valor)
{
    if (!C || !valor) return;
    if (nome != GL_FOG_COLOR) { erro(GL_INVALID_ENUM); return; }
    for (int k = 0; k < 3; k++) {
        float v = valor[k];
        if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
        C->fog_cor[k] = v * 255.0f;
    }
}

void glLightfv(GLenum luz, GLenum nome, const GLfloat *valor)
{
    if (!C || !valor) return;
    if (luz != GL_LIGHT0) { erro(GL_INVALID_ENUM); return; }
    if (nome == GL_LIGHT_POSITION) {
        for (int k = 0; k < 4; k++) C->luz_pos[k] = valor[k];
    } else if (nome == GL_LIGHT_DIFFUSE) {
        for (int k = 0; k < 3; k++) {
            float v = valor[k];
            if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
            C->luz_dif[k] = v;
        }
    } else if (nome == GL_LIGHT_AMBIENT) {
        for (int k = 0; k < 3; k++) {
            float v = valor[k];
            if (v < 0.0f) v = 0.0f; else if (v > 1.0f) v = 1.0f;
            C->luz_amb[k] = v;
        }
    } else erro(GL_INVALID_ENUM);
}

/* ------------------------------------------------------------ v6: motor */

void sgl_motor_simd(int ativo) { sgl_simd = ativo ? 1 : 0; }
int  sgl_motor_simd_ativo(void) { return sgl_simd; }
