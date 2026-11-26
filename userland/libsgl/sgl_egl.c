// SPDX-License-Identifier: GPL-2.0-or-later
/* sgl_egl.c - EGL 1.4 mínimo sobre SaguiGL y las ventanas de SaguiOS (D425).
 *
 * Una sola pantalla (EGL_DEFAULT_DISPLAY). Cuatro configuraciones: RGB de 8
 * bits sin alfa (la ventana es 0x00RRGGBB), Z de 16 o 24 y esténcil de 0 u 8,
 * para ventanas y pbuffers, con OpenGL ES 1.1. Una superficie de ventana es
 * una jan_t * de la libsagui; eglSwapBuffers es sgl_trocar() (y la GPU, si el
 * contexto la tiene, vuelca ahí su cuadro). El contexto de SaguiGL se crea al
 * primer eglMakeCurrent, que es cuando se sabe dónde dibuja. */
#include "sgl.h"
#include "../inc/EGL/egl.h"
#ifdef SGL_ES2
#include "sgl2.h"   /* D475: contextos de OpenGL ES 2.0 */
#define EGL_BITS_ES (EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT)
#else
#define EGL_BITS_ES EGL_OPENGL_ES_BIT
#endif

#if !defined(SGL_ANFITRIAO)
#include "gui.h"
#define SGL_EGL_JANELAS 1
#endif

#ifdef SGL_ANFITRIAO
#include <stdlib.h>
#define EGL_ALOCAR(n)  calloc(1, (n))
#define EGL_LIBERAR(p) free(p)
#else
#define EGL_ALOCAR(n)  aloca_zerado(n)
#define EGL_LIBERAR(p) libera(p)
#endif

#define EGL_MAX_PB 2048

typedef struct { EGLint id, z, est; } cfg_t;
static const cfg_t cfgs[4] = { { 1, 16, 0 }, { 2, 16, 8 }, { 3, 24, 0 }, { 4, 24, 8 } };
#define N_CFG 4

typedef struct {
    int       magia;           /* 'SURF' */
    const cfg_t *cfg;
    void     *janela;          /* jan_t * o NULL (pbuffer) */
    uint32_t *px;              /* pbuffer: los píxeles */
    int       larg, alt;
    int       destruir;        /* se destruyó pero sigue siendo la actual */
} surf_t;

typedef struct {
    int        magia;          /* 'CTXT' */
    const cfg_t *cfg;
    sgl_ctx_t *c;
    surf_t    *alvo;
    int        destruir;
    int        versao;         /* D475: 1 = OpenGL ES 1.1, 2 = OpenGL ES 2.0 (c2) */
#ifdef SGL_ES2
    sgl2_ctx_t *c2;
#endif
} ctx_t;

#define MAGIA_SURF 0x53555246
#define MAGIA_CTX  0x43545854

static int     iniciada;
static EGLint  erro_egl = EGL_SUCCESS;
static ctx_t  *atual;
static surf_t *atual_d, *atual_l;
static EGLenum api = EGL_OPENGL_ES_API;
static int     intervalo = 1;
static char    pantalla;   /* la única EGLDisplay es su dirección */

#define DPY ((EGLDisplay)&pantalla)

static EGLBoolean falha(EGLint e) { erro_egl = e; return EGL_FALSE; }
static int dpy_ok(EGLDisplay d)
{
    if (d != DPY) { erro_egl = EGL_BAD_DISPLAY; return 0; }
    if (!iniciada) { erro_egl = EGL_NOT_INITIALIZED; return 0; }
    return 1;
}
static const cfg_t *cfg_de(EGLConfig c)
{
    for (int i = 0; i < N_CFG; i++) if ((const void *)&cfgs[i] == c) return &cfgs[i];
    return 0;
}

EGLint eglGetError(void) { EGLint e = erro_egl; erro_egl = EGL_SUCCESS; return e; }

EGLDisplay eglGetDisplay(EGLNativeDisplayType d)
{
    if (d != EGL_DEFAULT_DISPLAY) { erro_egl = EGL_BAD_PARAMETER; return EGL_NO_DISPLAY; }
    return DPY;
}

EGLBoolean eglInitialize(EGLDisplay d, EGLint *maj, EGLint *min)
{
    if (d != DPY) return falha(EGL_BAD_DISPLAY);
    iniciada = 1;
    if (maj) *maj = 1;
    if (min) *min = 4;
    erro_egl = EGL_SUCCESS;
    return EGL_TRUE;
}

EGLBoolean eglTerminate(EGLDisplay d)
{
    if (d != DPY) return falha(EGL_BAD_DISPLAY);
    iniciada = 0;
    return EGL_TRUE;
}

const char *eglQueryString(EGLDisplay d, EGLint nome)
{
    if (!dpy_ok(d)) return 0;
    switch (nome) {
    case EGL_VENDOR: return "SaguiOS";
    case EGL_VERSION: return "1.4 SaguiGL";
    case EGL_EXTENSIONS: return "";
    case EGL_CLIENT_APIS: return "OpenGL_ES";
    default: erro_egl = EGL_BAD_PARAMETER; return 0;
    }
}

static EGLint cfg_attr(const cfg_t *c, EGLint a, int *ok)
{
    *ok = 1;
    switch (a) {
    case EGL_BUFFER_SIZE: return 24;
    case EGL_RED_SIZE: case EGL_GREEN_SIZE: case EGL_BLUE_SIZE: return 8;
    case EGL_ALPHA_SIZE: case EGL_LUMINANCE_SIZE: case EGL_ALPHA_MASK_SIZE: return 0;
    case EGL_DEPTH_SIZE: return c->z;
    case EGL_STENCIL_SIZE: return c->est;
    case EGL_CONFIG_CAVEAT: return EGL_NONE;
    case EGL_CONFIG_ID: return c->id;
    case EGL_LEVEL: return 0;
    case EGL_MAX_PBUFFER_WIDTH: case EGL_MAX_PBUFFER_HEIGHT: return EGL_MAX_PB;
    case EGL_MAX_PBUFFER_PIXELS: return EGL_MAX_PB * EGL_MAX_PB;
    case EGL_NATIVE_RENDERABLE: return EGL_FALSE;
    case EGL_NATIVE_VISUAL_ID: case EGL_NATIVE_VISUAL_TYPE: return 0;
    case EGL_SAMPLES: case EGL_SAMPLE_BUFFERS: return 0;
    case EGL_SURFACE_TYPE: return EGL_WINDOW_BIT | EGL_PBUFFER_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
    case EGL_TRANSPARENT_TYPE: return EGL_NONE;
    case EGL_TRANSPARENT_RED_VALUE: case EGL_TRANSPARENT_GREEN_VALUE: case EGL_TRANSPARENT_BLUE_VALUE: return 0;
    case EGL_BIND_TO_TEXTURE_RGB: case EGL_BIND_TO_TEXTURE_RGBA: return EGL_FALSE;
    case EGL_MIN_SWAP_INTERVAL: return 0;
    case EGL_MAX_SWAP_INTERVAL: return 1;
    case EGL_COLOR_BUFFER_TYPE: return EGL_RGB_BUFFER;
    case EGL_RENDERABLE_TYPE: case EGL_CONFORMANT: return EGL_BITS_ES;
    default: *ok = 0; return 0;
    }
}

EGLBoolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, EGLint a, EGLint *v)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    const cfg_t *k = cfg_de(c);
    if (!k) return falha(EGL_BAD_CONFIG);
    int ok;
    EGLint r = cfg_attr(k, a, &ok);
    if (!ok) return falha(EGL_BAD_ATTRIBUTE);
    if (v) *v = r;
    return EGL_TRUE;
}

EGLBoolean eglGetConfigs(EGLDisplay d, EGLConfig *c, EGLint tam, EGLint *n)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    if (!n) return falha(EGL_BAD_PARAMETER);
    int k = 0;
    for (int i = 0; i < N_CFG; i++) { if (c) { if (k >= tam) break; c[k] = (EGLConfig)&cfgs[i]; } k++; }
    *n = k;
    return EGL_TRUE;
}

/* ¿Cumple la configuración lo pedido? Tamaños: «al menos»; lo demás: igual o con bits. */
static int cfg_cumpre(const cfg_t *c, const EGLint *at)
{
    for (; at && at[0] != EGL_NONE; at += 2) {
        EGLint a = at[0], v = at[1];
        if (v == EGL_DONT_CARE) continue;
        int ok;
        EGLint r = cfg_attr(c, a, &ok);
        if (!ok) return -1;
        switch (a) {
        case EGL_BUFFER_SIZE: case EGL_RED_SIZE: case EGL_GREEN_SIZE: case EGL_BLUE_SIZE: case EGL_ALPHA_SIZE:
        case EGL_DEPTH_SIZE: case EGL_STENCIL_SIZE: case EGL_LUMINANCE_SIZE: case EGL_ALPHA_MASK_SIZE:
        case EGL_SAMPLES: case EGL_SAMPLE_BUFFERS:
            if (r < v) return 0;
            break;
        case EGL_SURFACE_TYPE: case EGL_RENDERABLE_TYPE: case EGL_CONFORMANT:
            if ((r & v) != v) return 0;
            break;
        case EGL_MAX_PBUFFER_WIDTH: case EGL_MAX_PBUFFER_HEIGHT: case EGL_MAX_PBUFFER_PIXELS:
        case EGL_NATIVE_VISUAL_ID:
            break;   /* se ignoran al elegir */
        case EGL_MIN_SWAP_INTERVAL: case EGL_MAX_SWAP_INTERVAL:
            if (r != v) return 0;
            break;
        default:
            if (r != v) return 0;
        }
    }
    return 1;
}

EGLBoolean eglChooseConfig(EGLDisplay d, const EGLint *at, EGLConfig *c, EGLint tam, EGLint *n)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    if (!n) return falha(EGL_BAD_PARAMETER);
    int k = 0;
    /* ya están en el orden de EGL: menos Z y menos esténcil primero */
    for (int i = 0; i < N_CFG; i++) {
        int r = cfg_cumpre(&cfgs[i], at);
        if (r < 0) return falha(EGL_BAD_ATTRIBUTE);
        if (!r) continue;
        if (c) { if (k >= tam) break; c[k] = (EGLConfig)&cfgs[i]; }
        k++;
    }
    *n = k;
    return EGL_TRUE;
}

EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *at)
{
    if (!dpy_ok(d)) return EGL_NO_SURFACE;
    const cfg_t *k = cfg_de(c);
    if (!k) { erro_egl = EGL_BAD_CONFIG; return EGL_NO_SURFACE; }
    for (; at && at[0] != EGL_NONE; at += 2)
        if (at[0] != EGL_RENDER_BUFFER) { erro_egl = EGL_BAD_ATTRIBUTE; return EGL_NO_SURFACE; }
#ifdef SGL_EGL_JANELAS
    jan_t *j = (jan_t *)w;
    if (!j || !j->px || j->larg <= 0 || j->alt <= 0) { erro_egl = EGL_BAD_NATIVE_WINDOW; return EGL_NO_SURFACE; }
    surf_t *s = (surf_t *)EGL_ALOCAR(sizeof *s);
    if (!s) { erro_egl = EGL_BAD_ALLOC; return EGL_NO_SURFACE; }
    s->magia = MAGIA_SURF; s->cfg = k; s->janela = j; s->px = j->px; s->larg = j->larg; s->alt = j->alt;
    return (EGLSurface)s;
#else
    (void)w;
    erro_egl = EGL_BAD_NATIVE_WINDOW;
    return EGL_NO_SURFACE;
#endif
}

EGLSurface eglCreatePbufferSurface(EGLDisplay d, EGLConfig c, const EGLint *at)
{
    if (!dpy_ok(d)) return EGL_NO_SURFACE;
    const cfg_t *k = cfg_de(c);
    if (!k) { erro_egl = EGL_BAD_CONFIG; return EGL_NO_SURFACE; }
    int l = 0, a = 0, maior = 0;
    for (; at && at[0] != EGL_NONE; at += 2) {
        if (at[0] == EGL_WIDTH) l = at[1];
        else if (at[0] == EGL_HEIGHT) a = at[1];
        else if (at[0] == EGL_LARGEST_PBUFFER) maior = at[1];
        else if (at[0] == EGL_TEXTURE_FORMAT || at[0] == EGL_TEXTURE_TARGET) { if (at[1] != EGL_NO_TEXTURE) { erro_egl = EGL_BAD_MATCH; return EGL_NO_SURFACE; } }
        else if (at[0] != EGL_MIPMAP_TEXTURE) { erro_egl = EGL_BAD_ATTRIBUTE; return EGL_NO_SURFACE; }
    }
    if (l < 0 || a < 0) { erro_egl = EGL_BAD_PARAMETER; return EGL_NO_SURFACE; }
    if (l > EGL_MAX_PB || a > EGL_MAX_PB) {
        if (!maior) { erro_egl = EGL_BAD_ALLOC; return EGL_NO_SURFACE; }
        if (l > EGL_MAX_PB) l = EGL_MAX_PB;
        if (a > EGL_MAX_PB) a = EGL_MAX_PB;
    }
    if (l == 0) l = 1;
    if (a == 0) a = 1;
    surf_t *s = (surf_t *)EGL_ALOCAR(sizeof *s);
    if (!s) { erro_egl = EGL_BAD_ALLOC; return EGL_NO_SURFACE; }
    s->px = (uint32_t *)EGL_ALOCAR((size_t)l * a * 4);
    if (!s->px) { EGL_LIBERAR(s); erro_egl = EGL_BAD_ALLOC; return EGL_NO_SURFACE; }
    s->magia = MAGIA_SURF; s->cfg = k; s->janela = 0; s->larg = l; s->alt = a;
    return (EGLSurface)s;
}

EGLSurface eglCreatePixmapSurface(EGLDisplay d, EGLConfig c, EGLNativePixmapType p, const EGLint *at)
{
    (void)c; (void)p; (void)at;
    if (!dpy_ok(d)) return EGL_NO_SURFACE;
    erro_egl = EGL_BAD_MATCH;   /* ninguna configuración tiene EGL_PIXMAP_BIT */
    return EGL_NO_SURFACE;
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay d, EGLenum t, EGLClientBuffer b, EGLConfig c, const EGLint *at)
{
    (void)t; (void)b; (void)c; (void)at;
    if (!dpy_ok(d)) return EGL_NO_SURFACE;
    erro_egl = EGL_BAD_PARAMETER;   /* solo existe para OpenVG */
    return EGL_NO_SURFACE;
}

static surf_t *surf_de(EGLSurface s) { surf_t *x = (surf_t *)s; return x && x->magia == MAGIA_SURF ? x : 0; }
static ctx_t *ctx_de(EGLContext c) { ctx_t *x = (ctx_t *)c; return x && x->magia == MAGIA_CTX ? x : 0; }

static void surf_soltar(surf_t *s)
{
    if (!s->janela && s->px) EGL_LIBERAR(s->px);
    s->magia = 0;
    EGL_LIBERAR(s);
}

EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    surf_t *x = surf_de(s);
    if (!x) return falha(EGL_BAD_SURFACE);
    if (x == atual_d || x == atual_l) { x->destruir = 1; return EGL_TRUE; }   /* cuando deje de ser la actual */
    surf_soltar(x);
    return EGL_TRUE;
}

/* La ventana pudo cambiar de tamaño: el contexto la sigue. */
static void surf_atualizar(surf_t *s, ctx_t *c)
{
#ifdef SGL_EGL_JANELAS
    if (s->janela) {
        jan_t *j = (jan_t *)s->janela;
        if (j->px != s->px || j->larg != s->larg || j->alt != s->alt) {
            s->px = j->px; s->larg = j->larg; s->alt = j->alt;
            if (c && c->c && c->alvo == s) sgl_mudar_destino(c->c, j, j->px, j->larg, j->alt);
        }
    }
#else
    (void)s; (void)c;
#endif
}

EGLBoolean eglQuerySurface(EGLDisplay d, EGLSurface s, EGLint a, EGLint *v)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    surf_t *x = surf_de(s);
    if (!x) return falha(EGL_BAD_SURFACE);
    surf_atualizar(x, atual);
    switch (a) {
    case EGL_WIDTH: *v = x->larg; break;
    case EGL_HEIGHT: *v = x->alt; break;
    case EGL_CONFIG_ID: *v = x->cfg->id; break;
    case EGL_RENDER_BUFFER: *v = x->janela ? EGL_BACK_BUFFER : EGL_BACK_BUFFER; break;
    case EGL_SWAP_BEHAVIOR: *v = EGL_BUFFER_PRESERVED; break;
    case EGL_MULTISAMPLE_RESOLVE: *v = EGL_MULTISAMPLE_RESOLVE_DEFAULT; break;
    case EGL_LARGEST_PBUFFER: case EGL_MIPMAP_TEXTURE: case EGL_MIPMAP_LEVEL: *v = 0; break;
    case EGL_TEXTURE_FORMAT: case EGL_TEXTURE_TARGET: *v = EGL_NO_TEXTURE; break;
    case EGL_HORIZONTAL_RESOLUTION: case EGL_VERTICAL_RESOLUTION: case EGL_PIXEL_ASPECT_RATIO: *v = EGL_UNKNOWN; break;
    case EGL_VG_ALPHA_FORMAT: *v = EGL_VG_ALPHA_FORMAT_NONPRE; break;
    case EGL_VG_COLORSPACE: *v = EGL_VG_COLORSPACE_sRGB; break;
    default: return falha(EGL_BAD_ATTRIBUTE);
    }
    return EGL_TRUE;
}

EGLBoolean eglSurfaceAttrib(EGLDisplay d, EGLSurface s, EGLint a, EGLint v)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    if (!surf_de(s)) return falha(EGL_BAD_SURFACE);
    if (a == EGL_SWAP_BEHAVIOR && (v == EGL_BUFFER_PRESERVED || v == EGL_BUFFER_DESTROYED)) return EGL_TRUE;
    if (a == EGL_MIPMAP_LEVEL || a == EGL_MULTISAMPLE_RESOLVE) return EGL_TRUE;
    return falha(EGL_BAD_ATTRIBUTE);
}

EGLBoolean eglBindTexImage(EGLDisplay d, EGLSurface s, EGLint b) { (void)s; (void)b; if (!dpy_ok(d)) return EGL_FALSE; return falha(EGL_BAD_MATCH); }
EGLBoolean eglReleaseTexImage(EGLDisplay d, EGLSurface s, EGLint b) { (void)s; (void)b; if (!dpy_ok(d)) return EGL_FALSE; return falha(EGL_BAD_MATCH); }

EGLBoolean eglBindAPI(EGLenum a)
{
    if (a != EGL_OPENGL_ES_API) return falha(EGL_BAD_PARAMETER);
    api = a;
    return EGL_TRUE;
}
EGLenum eglQueryAPI(void) { return api; }

EGLBoolean eglSwapInterval(EGLDisplay d, EGLint i)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    if (!atual) return falha(EGL_BAD_CONTEXT);
    intervalo = i < 0 ? 0 : i > 1 ? 1 : i;   /* el cuadro se muestra al momento: se guarda y ya */
    return EGL_TRUE;
}

EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext comp, const EGLint *at)
{
    (void)comp;   /* sin objetos compartidos entre contextos (SaguiGL tiene uno por destino) */
    if (!dpy_ok(d)) return EGL_NO_CONTEXT;
    const cfg_t *k = cfg_de(c);
    if (!k) { erro_egl = EGL_BAD_CONFIG; return EGL_NO_CONTEXT; }
    if (api != EGL_OPENGL_ES_API) { erro_egl = EGL_BAD_MATCH; return EGL_NO_CONTEXT; }
    int versao = 1;
    for (; at && at[0] != EGL_NONE; at += 2) {
#ifdef SGL_ES2
        if (at[0] == EGL_CONTEXT_CLIENT_VERSION) { if (at[1] != 1 && at[1] != 2) { erro_egl = EGL_BAD_MATCH; return EGL_NO_CONTEXT; } versao = at[1]; }   /* D475: ES 1.1 o 2.0 */
#else
        if (at[0] == EGL_CONTEXT_CLIENT_VERSION) { if (at[1] != 1) { erro_egl = EGL_BAD_MATCH; return EGL_NO_CONTEXT; } }   /* solo ES 1 */
#endif
        else { erro_egl = EGL_BAD_ATTRIBUTE; return EGL_NO_CONTEXT; }
    }
    ctx_t *x = (ctx_t *)EGL_ALOCAR(sizeof *x);
    if (!x) { erro_egl = EGL_BAD_ALLOC; return EGL_NO_CONTEXT; }
    x->magia = MAGIA_CTX; x->cfg = k; x->versao = versao;
    return (EGLContext)x;
}

static void ctx_soltar(ctx_t *x)
{
#ifdef SGL_ES2
    if (x->c2) sgl2_destruir(x->c2);
#endif
    if (x->c) sgl_destruir_contexto(x->c);
    x->magia = 0;
    EGL_LIBERAR(x);
}

EGLBoolean eglDestroyContext(EGLDisplay d, EGLContext c)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    ctx_t *x = ctx_de(c);
    if (!x) return falha(EGL_BAD_CONTEXT);
    if (x == atual) { x->destruir = 1; return EGL_TRUE; }
    ctx_soltar(x);
    return EGL_TRUE;
}

static void soltar_atual(void)
{
    ctx_t *c = atual;
    surf_t *d = atual_d, *l = atual_l;
    atual = 0; atual_d = atual_l = 0;
    sgl_usar_contexto(0);
#ifdef SGL_ES2
    sgl2_usar(0);
#endif
    if (c && c->destruir) ctx_soltar(c);
    if (d && d->destruir) surf_soltar(d);
    if (l && l != d && l->destruir) surf_soltar(l);
}

#ifdef SGL_ES2
static void sincronizar_gpu2(void) { if (atual && atual->c2) sgl2_gpu_sincronizar(atual->c2); }   /* D480 */
#else
static void sincronizar_gpu2(void) {}
#endif
EGLBoolean eglMakeCurrent(EGLDisplay d, EGLSurface sd, EGLSurface sl, EGLContext c)
{
    sincronizar_gpu2();
    if (d == EGL_NO_DISPLAY && c == EGL_NO_CONTEXT && sd == EGL_NO_SURFACE && sl == EGL_NO_SURFACE) { soltar_atual(); return EGL_TRUE; }
    if (!dpy_ok(d)) return EGL_FALSE;
    if (c == EGL_NO_CONTEXT) {
        if (sd != EGL_NO_SURFACE || sl != EGL_NO_SURFACE) return falha(EGL_BAD_MATCH);
        if (atual) glFinish();
        soltar_atual();
        return EGL_TRUE;
    }
    ctx_t *x = ctx_de(c);
    surf_t *s = surf_de(sd), *r = surf_de(sl);
    if (!x) return falha(EGL_BAD_CONTEXT);
    if (!s || !r) return falha(EGL_BAD_SURFACE);
    if (s->cfg != x->cfg) return falha(EGL_BAD_MATCH);
    if (atual && atual != x) glFinish();
    if (atual != x || atual_d != s) soltar_atual();
    surf_atualizar(s, 0);
    if (!x->c) {
        int zb = x->cfg->z == 16 ? 16 : 32;
#ifdef SGL_EGL_JANELAS
        x->c = s->janela ? sgl_criar_contexto(s->janela, zb) : sgl_criar_contexto_mem(s->px, s->larg, s->alt, zb);
#else
        x->c = sgl_criar_contexto_mem(s->px, s->larg, s->alt, zb);
#endif
        if (!x->c) return falha(EGL_BAD_ALLOC);
        sgl_estencil(x->c, x->cfg->est);
        sgl_tras_proprio(x->c);   /* D451: la ventana solo ve cuadros enteros (eglSwapBuffers) */
        x->alvo = s;
#ifdef SGL_ES2
        if (x->versao == 2) {
            /* D475: el contexto de ES 1.1 solo pone el destino; ES 2.0 va por la CPU (sin GPU por ahora) */
            sgl_gpu_desligar(x->c);
            x->c2 = sgl2_criar(x->c, x->cfg->z, x->cfg->est);
            if (!x->c2) return falha(EGL_BAD_ALLOC);
        }
#endif
    } else if (x->alvo != s) {
        if (!sgl_mudar_destino(x->c, s->janela, s->px, s->larg, s->alt)) return falha(EGL_BAD_ALLOC);
#ifdef SGL_ES2
        if (x->versao == 2) sgl_gpu_desligar(x->c);
#endif
        x->alvo = s;
    }
    sgl_usar_contexto(x->c);
#ifdef SGL_ES2
    sgl2_usar(x->versao == 2 ? x->c2 : 0);
#endif
    atual = x; atual_d = s; atual_l = r;
    return EGL_TRUE;
}

EGLContext eglGetCurrentContext(void) { return atual ? (EGLContext)atual : EGL_NO_CONTEXT; }
EGLSurface eglGetCurrentSurface(EGLint q)
{
    if (q == EGL_DRAW) return atual_d ? (EGLSurface)atual_d : EGL_NO_SURFACE;
    if (q == EGL_READ) return atual_l ? (EGLSurface)atual_l : EGL_NO_SURFACE;
    erro_egl = EGL_BAD_PARAMETER;
    return EGL_NO_SURFACE;
}
EGLDisplay eglGetCurrentDisplay(void) { return atual ? DPY : EGL_NO_DISPLAY; }

EGLBoolean eglQueryContext(EGLDisplay d, EGLContext c, EGLint a, EGLint *v)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    ctx_t *x = ctx_de(c);
    if (!x) return falha(EGL_BAD_CONTEXT);
    switch (a) {
    case EGL_CONFIG_ID: *v = x->cfg->id; break;
    case EGL_CONTEXT_CLIENT_TYPE: *v = EGL_OPENGL_ES_API; break;
    case EGL_CONTEXT_CLIENT_VERSION: *v = x->versao; break;
    case EGL_RENDER_BUFFER: *v = x->alvo ? EGL_BACK_BUFFER : EGL_NONE; break;
    default: return falha(EGL_BAD_ATTRIBUTE);
    }
    return EGL_TRUE;
}

EGLBoolean eglWaitClient(void) { if (atual) glFinish(); return EGL_TRUE; }
EGLBoolean eglWaitGL(void) { return eglWaitClient(); }
EGLBoolean eglWaitNative(EGLint e) { if (e != EGL_CORE_NATIVE_ENGINE) return falha(EGL_BAD_PARAMETER); return EGL_TRUE; }
EGLBoolean eglReleaseThread(void) { soltar_atual(); erro_egl = EGL_SUCCESS; return EGL_TRUE; }

EGLBoolean eglSwapBuffers(EGLDisplay d, EGLSurface s)
{
    if (!dpy_ok(d)) return EGL_FALSE;
    surf_t *x = surf_de(s);
    if (!x) return falha(EGL_BAD_SURFACE);
    if (!atual || atual_d != x) return falha(EGL_BAD_SURFACE);
    sincronizar_gpu2();
    if (x->janela) sgl_trocar();     /* trae el cuadro de la GPU y muestra la ventana */
    else glFinish();
    surf_atualizar(x, atual);
    return EGL_TRUE;
}

EGLBoolean eglCopyBuffers(EGLDisplay d, EGLSurface s, EGLNativePixmapType p)
{
    (void)p;
    if (!dpy_ok(d)) return EGL_FALSE;
    if (!surf_de(s)) return falha(EGL_BAD_SURFACE);
    return falha(EGL_BAD_NATIVE_PIXMAP);
}

/* ---- eglGetProcAddress: todas las funciones de GL ES y de EGL por su nombre ---- */
typedef void (*proc_t)(void);
#define P(f) { #f, (proc_t)f }
static const struct { const char *n; proc_t f; } procs[] = {
#include "sgl_procs.inc"
#ifdef SGL_ES2
#include "sgl2_procs.inc"
#endif
    P(eglGetError), P(eglGetDisplay), P(eglInitialize), P(eglTerminate), P(eglQueryString), P(eglGetConfigs),
    P(eglChooseConfig), P(eglGetConfigAttrib), P(eglCreateWindowSurface), P(eglCreatePbufferSurface),
    P(eglCreatePixmapSurface), P(eglDestroySurface), P(eglQuerySurface), P(eglBindAPI), P(eglQueryAPI),
    P(eglWaitClient), P(eglReleaseThread), P(eglCreatePbufferFromClientBuffer), P(eglSurfaceAttrib),
    P(eglBindTexImage), P(eglReleaseTexImage), P(eglSwapInterval), P(eglCreateContext), P(eglDestroyContext),
    P(eglMakeCurrent), P(eglGetCurrentContext), P(eglGetCurrentSurface), P(eglGetCurrentDisplay),
    P(eglQueryContext), P(eglWaitGL), P(eglWaitNative), P(eglSwapBuffers), P(eglCopyBuffers), P(eglGetProcAddress),
};

static int nome_igual(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *nome)
{
    if (!nome) return 0;
    for (unsigned i = 0; i < sizeof procs / sizeof procs[0]; i++)
        if (nome_igual(procs[i].n, nome)) return procs[i].f;
    return 0;
}
