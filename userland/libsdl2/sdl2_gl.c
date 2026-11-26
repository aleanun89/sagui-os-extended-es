// SPDX-License-Identifier: GPL-2.0-or-later
/* sdl2_gl.c - SDL_GL_* de la SDL 2 del SaguiOS sobre EGL y SaguiGL (D425).
 *
 * OpenGL ES 1.1 en la ventana de la SDL: el contexto es uno de EGL cuya «ventana nativa» es la
 * ventana del sistema que hay detrás de la SDL_Window (con su tamaño real, ya escalada).
 * SDL_GL_SwapWindow es eglSwapBuffers. Sin OpenGL de escritorio ni ES 2: se dice con SDL_GetError. */
#include "SDL.h"
#include "sagui.h"
#include "gui.h"
#include "../inc/EGL/egl.h"
#include "../inc/GLES/gl.h"

static int at[SDL_GL_CONTEXT_NO_ERROR + 1];
static int at_pronto;
static EGLDisplay dpy = EGL_NO_DISPLAY;
static jan_t nativa;            /* la ventana del sistema, como la quiere EGL (jan_t) */
static SDL_Window *janela_atual;
static EGLSurface sup = EGL_NO_SURFACE;
static EGLContext ctx_atual = EGL_NO_CONTEXT;
static int intervalo = 1;

/* D451: escala de dibujo de OpenGL ES (SaguiOS): con la pista SDL_HINT_SAGUI_GL_ESCALA (o la variable de entorno del
 * mismo nombre) a 25..99, el contexto dibuja a ese tanto por ciento de la ventana y SaguiGL lo amplía al presentar (por
 * la GPU a pantalla completa, D432). SDL_GL_GetDrawableSize da el tamaño reducido. Se mira en cada SDL_GL_SwapWindow. */
int sgl_escala_medida(int lado, int pct);
struct sgl_ctx;
struct sgl_ctx *sgl_contexto_atual(void);
int sgl_escala(struct sgl_ctx *c, int pct);
static int escala = 100;

static int escala_pedida(void)
{
    const char *e = SDL_GetHint(SDL_HINT_SAGUI_GL_ESCALA);
    if (!e) e = SDL_getenv(SDL_HINT_SAGUI_GL_ESCALA);
    if (!e || !*e) return 100;
    int v = SDL_atoi(e);
    return v < 25 ? 25 : v > 100 ? 100 : v;
}

static void escala_aplicar(void)
{
    int p = escala_pedida();
    if (p == escala || ctx_atual == EGL_NO_CONTEXT) return;
    if (sgl_escala(sgl_contexto_atual(), p)) {
        escala = p;
        SDL_Log("OpenGL ES: escala de dibujo %d %% (D451)", p);
    }
}

static void at_iniciar(void)
{
    if (at_pronto) return;
    for (int i = 0; i <= SDL_GL_CONTEXT_NO_ERROR; i++) at[i] = 0;
    at[SDL_GL_RED_SIZE] = at[SDL_GL_GREEN_SIZE] = at[SDL_GL_BLUE_SIZE] = 8;
    at[SDL_GL_DEPTH_SIZE] = 16; at[SDL_GL_DOUBLEBUFFER] = 1;
    at[SDL_GL_CONTEXT_MAJOR_VERSION] = 1; at[SDL_GL_CONTEXT_MINOR_VERSION] = 1;
    at[SDL_GL_CONTEXT_PROFILE_MASK] = SDL_GL_CONTEXT_PROFILE_ES;
    at[SDL_GL_ACCELERATED_VISUAL] = 1;
    at_pronto = 1;
}

int  SDL_GL_LoadLibrary(const char *ruta) { (void)ruta; return 0; }
void SDL_GL_UnloadLibrary(void) {}
void *SDL_GL_GetProcAddress(const char *nome) { return (void *)eglGetProcAddress(nome); }
void SDL_GL_ResetAttributes(void) { at_pronto = 0; at_iniciar(); }

SDL_bool SDL_GL_ExtensionSupported(const char *ext)
{
    if (!ext || !*ext || ctx_atual == EGL_NO_CONTEXT) return SDL_FALSE;
    const char *l = (const char *)glGetString(GL_EXTENSIONS);
    int n = 0;
    while (ext[n]) n++;
    for (const char *p = l; p && *p; ) {
        const char *q = p;
        while (*q && *q != ' ') q++;
        if (q - p == n) { int i = 0; while (i < n && p[i] == ext[i]) i++; if (i == n) return SDL_TRUE; }
        p = *q ? q + 1 : q;
    }
    return SDL_FALSE;
}

int SDL_GL_SetAttribute(SDL_GLattr a, int v)
{
    at_iniciar();
    if ((int)a < 0 || a > SDL_GL_CONTEXT_NO_ERROR) return SDL_SetError("atributo de GL desconocido");
    at[a] = v;
    return 0;
}

int SDL_GL_GetAttribute(SDL_GLattr a, int *v)
{
    at_iniciar();
    if ((int)a < 0 || a > SDL_GL_CONTEXT_NO_ERROR || !v) return SDL_SetError("atributo de GL desconocido");
    *v = at[a];
    return 0;
}

static int nativa_atualizar(SDL_Window *w)
{
    int id, l, a;
    uint32_t *px;
    if (sagui_sdl2_janela_nativa(w, &id, &px, &l, &a) < 0) return -1;
    nativa.id = id; nativa.px = px; nativa.larg = l; nativa.alt = a;
    return 0;
}

SDL_GLContext SDL_GL_CreateContext(SDL_Window *w)
{
    at_iniciar();
    if (!w) { SDL_SetError("SDL_GL_CreateContext sin ventana"); return NULL; }
    if ((at[SDL_GL_CONTEXT_PROFILE_MASK] && at[SDL_GL_CONTEXT_PROFILE_MASK] != SDL_GL_CONTEXT_PROFILE_ES) ||
        at[SDL_GL_CONTEXT_MAJOR_VERSION] > 1) {
        SDL_SetError("la SDL del SaguiOS solo tiene OpenGL ES 1.1 (SaguiGL)");
        return NULL;
    }
    if (nativa_atualizar(w) < 0) { SDL_SetError("ventana sin píxeles"); return NULL; }
    if (dpy == EGL_NO_DISPLAY) {
        dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!eglInitialize(dpy, 0, 0)) { SDL_SetError("eglInitialize: %x", eglGetError()); return NULL; }
    }
    EGLint pede[] = { EGL_DEPTH_SIZE, at[SDL_GL_DEPTH_SIZE] > 16 ? 24 : 16, EGL_STENCIL_SIZE, at[SDL_GL_STENCIL_SIZE] > 0 ? 8 : 0,
                      EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT, EGL_NONE };
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, pede, &cfg, 1, &n) || n < 1) { SDL_SetError("sin configuración de EGL"); return NULL; }
    if (sup == EGL_NO_SURFACE) {
        sup = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)&nativa, NULL);
        if (sup == EGL_NO_SURFACE) { SDL_SetError("eglCreateWindowSurface: %x", eglGetError()); return NULL; }
    }
    static const EGLint cx[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
    EGLContext c = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, cx);
    if (c == EGL_NO_CONTEXT) { SDL_SetError("eglCreateContext: %x", eglGetError()); return NULL; }
    if (!eglMakeCurrent(dpy, sup, sup, c)) { eglDestroyContext(dpy, c); SDL_SetError("eglMakeCurrent: %x", eglGetError()); return NULL; }
    janela_atual = w; ctx_atual = c;
    escala = 100;
    escala_aplicar();
    return (SDL_GLContext)c;
}

int SDL_GL_MakeCurrent(SDL_Window *w, SDL_GLContext c)
{
    if (!c) { eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT); janela_atual = NULL; ctx_atual = EGL_NO_CONTEXT; return 0; }
    if (!w || nativa_atualizar(w) < 0) return SDL_SetError("SDL_GL_MakeCurrent sin ventana");
    if (!eglMakeCurrent(dpy, sup, sup, (EGLContext)c)) return SDL_SetError("eglMakeCurrent: %x", eglGetError());
    janela_atual = w; ctx_atual = (EGLContext)c;
    return 0;
}

SDL_Window *SDL_GL_GetCurrentWindow(void) { return janela_atual; }
SDL_GLContext SDL_GL_GetCurrentContext(void) { return (SDL_GLContext)ctx_atual; }

void SDL_GL_GetDrawableSize(SDL_Window *w, int *l, int *a)
{
    int id, ll = 0, aa = 0;
    uint32_t *px;
    if (w) sagui_sdl2_janela_nativa(w, &id, &px, &ll, &aa);
    if (w && w == janela_atual && escala < 100) { ll = sgl_escala_medida(ll, escala); aa = sgl_escala_medida(aa, escala); }
    if (l) *l = ll;
    if (a) *a = aa;
}

int SDL_GL_SetSwapInterval(int i)
{
    if (i < -1 || i > 1) return SDL_SetError("intervalo no válido");
    intervalo = i;
    if (ctx_atual != EGL_NO_CONTEXT) eglSwapInterval(dpy, i < 0 ? 1 : i);
    return 0;
}
int SDL_GL_GetSwapInterval(void) { return intervalo; }

void SDL_GL_SwapWindow(SDL_Window *w)
{
    if (!w || sup == EGL_NO_SURFACE) return;
    nativa_atualizar(w);   /* la ventana pudo cambiar (pantalla completa): EGL lo ve en el cambio */
    eglSwapBuffers(dpy, sup);
    escala_aplicar();
    sagui_sdl_fps_limitar();   /* D476 */
}

void SDL_GL_DeleteContext(SDL_GLContext c)
{
    if (!c) return;
    if ((EGLContext)c == ctx_atual) {
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        ctx_atual = EGL_NO_CONTEXT; janela_atual = NULL;
        if (sup != EGL_NO_SURFACE) { eglDestroySurface(dpy, sup); sup = EGL_NO_SURFACE; }
    }
    eglDestroyContext(dpy, (EGLContext)c);
}
