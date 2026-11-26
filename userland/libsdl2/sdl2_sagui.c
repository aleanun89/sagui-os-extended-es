/* sdl2_sagui.c - la SDL 2 del SaguiOS para los programas portados (D230).
 *
 * Lo que hay detrás de SDL.h (ver allí la lista de lo que hay y lo que no).
 * Código nuestro, sobre la libsagui; de la SDL solo está la interfaz. Sigue
 * las ideas de la SDL 1.2 común (userland/libsdl12/sdl_sagui.c, D228):
 *
 *   - vídeo:   una ventana. El programa ve una «ventana lógica» del tamaño
 *              que pidió (SDL_GetWindowSize, SDL_GetRendererOutputSize y las
 *              coordenadas del ratón son de ella); detrás hay un lienzo de
 *              32 bits de ese tamaño, donde dibujan el renderer y la
 *              superficie de la ventana. SDL_RenderPresent (o
 *              SDL_UpdateWindowSurface) lo copia a la ventana del sistema:
 *              tal cual en una ventana de su tamaño (por sagui_sdl2_escala),
 *              escalado y con bandas negras a pantalla completa o en una
 *              ventana de otro tamaño (4:3 para 320x200 y 640x400, como la
 *              VGA, D204; las demás, con su proporción).
 *   - teclado: los códigos de rastreo de SDL 2 salen del mapa de teclas
 *              pulsadas del kernel (teclas_seguradas, conjunto 1): así hay
 *              KEYDOWN y KEYUP de todas las teclas, también de las que no
 *              dan carácter (Mayús, Ctrl, Alt, Bloq Mayús). El EV_TECLA da
 *              el texto (SDL_TEXTINPUT, en UTF-8) y asegura el KEYDOWN de
 *              una pulsación tan corta que el mapa no la vio. Los caracteres
 *              se llevan a la posición de la tecla en el teclado de EE. UU.
 *   - ratón:   EV_MOVER, EV_CLIQUE, EV_SOLTOU y EV_RODA, en coordenadas de
 *              la ventana lógica. SDL_ShowCursor(SDL_DISABLE) oculta el
 *              puntero del sistema sobre la ventana (D230); los cursores de
 *              los programas se aceptan y no se dibujan.
 *   - audio:   el programa mezcla en su llamada de vuelta o pone datos en la
 *              cola (SDL_QueueAudio); un HILO DE AUDIO propio (D393, arranca
 *              con SDL_PauseAudio(0)) los pasa al flujo de sonido del sistema
 *              al ritmo de la tarjeta y llama desde él a la llamada de vuelta,
 *              con el cerrojo del dispositivo. SDL_SAGUI_AUDIO_HILO=0 vuelve
 *              al modo de antes: SDL_PollEvent, SDL_Delay y SDL_RenderPresent
 *              del hilo principal lo mueven. Sin tarjeta, mudo.
 *              SDL_PollEvent, SDL_Delay y SDL_RenderPresent hacen avanzar la
 *              OpenAL del SaguiOS (userland/libopenal) si el programa la enlaza.
 *   - reloj:   SDL_GetTicks y SDL_Delay sobre tempo_ms() y dormir().
 */
#include "SDL.h"

#include "sagui.h"
#include "gui.h"
#include "mandos.h"

#include <stdarg.h>

/* El teclado.h del kernel (TECLA_F1..F12) no entra en el userland. */
#ifndef TECLA_F1
#define TECLA_F1   0x10A
#define TECLA_F12  0x115
#endif

#define TAM_FILA 256   /* eventos en cola */

const char *sagui_sdl2_nome = "SDL2";
int sagui_sdl2_cheia_inicial = 0;
int sagui_sdl2_escala = 1;
int sagui_sdl2_medir = 0;   /* D247: línea «cuadro: ...» cada pocos segundos (o SAGUI_SDL2_MEDIR=1) */

/* ------------------------------------------------------------------ medidas (D247) */
/* Ciclos del TSC acumulados por fase; cada ~3 s se escribe la media por cuadro. Sin coste si no
 * se pide (una comparación por llamada). La frecuencia del TSC se calibra con tempo_ms(). */
static unsigned long long ciclos_paleta, ciclos_escala, ciclos_apres;
static unsigned medir_cuadros, medir_ini_ms;
static unsigned long long medir_ini_tsc;
static int medir_estado = -1;   /* -1: sin decidir */

static inline unsigned long long tsc_ler(void)
{
    unsigned lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}
static int medir_ativo(void)
{
    if (medir_estado < 0) {
        const char *e = getenv("SAGUI_SDL2_MEDIR");
        medir_estado = sagui_sdl2_medir || (e && e[0] && e[0] != '0');
    }
    return medir_estado;
}
#define MEDIR_INI() unsigned long long _t0 = medir_ativo() ? tsc_ler() : 0
#define MEDIR_FIM(acum) do { if (medir_estado > 0) (acum) += tsc_ler() - _t0; } while (0)

/* La OpenAL del SaguiOS, si el programa la enlaza (símbolo débil: sin ella, 0). */
extern void sagui_al_bombear(void) __attribute__((weak));

/* ------------------------------------------------------------------ general */

static char erro[160] = "";
static Uint32 sub_sistemas = 0;

const char *SDL_GetError(void) { return erro; }
void SDL_ClearError(void) { erro[0] = 0; }
int SDL_SetError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(erro, sizeof erro, fmt, ap);
    va_end(ap);
    return -1;
}

void SDL_Log(const char *fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    registrar("[%s] %s", sagui_sdl2_nome, b);
}

void *SDL_malloc(size_t n) { return malloc(n); }
void  SDL_free(void *p) { free(p); }
char *SDL_getenv(const char *nome) { return getenv(nome); }
char *SDL_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}
size_t sagui_sdl_strlcpy(char *dst, const char *src, size_t n)
{
    size_t k = strlen(src);
    if (n) {
        size_t c = k < n - 1 ? k : n - 1;
        memcpy(dst, src, c);
        dst[c] = 0;
    }
    return k;
}

void SDL_GetVersion(SDL_version *v) { SDL_VERSION(v); }
const char *SDL_GetPlatform(void) { return "SaguiOS"; }
/* CPU que pueden correr hilos de este programa: la principal y los secundarios arrancados (con --smp). */
int SDL_GetCPUCount(void)
{
    static int n;
    if (n) return n;
    static info_nucleo_t v[64];
    int k = info_sistema(INFO_NUCLEOS, v, 64), c = 0;
    for (int i = 0; i < k; i++) if (i == 0 || v[i].estado != 0) c++;   /* estado 0: no arrancó */
    n = c > 0 ? c : 1;
    return n;
}
int SDL_GetSystemRAM(void) { return 256; }

/* ---- pistas: se guardan unas pocas, por si el programa las vuelve a leer ---- */
#define N_PISTAS 16
static struct { char nome[48]; char valor[48]; } pistas[N_PISTAS];

SDL_bool SDL_SetHintWithPriority(const char *nome, const char *valor, SDL_HintPriority p)
{
    (void)p;
    if (!nome) return SDL_FALSE;
    int livre = -1;
    for (int i = 0; i < N_PISTAS; i++) {
        if (pistas[i].nome[0] && strcmp(pistas[i].nome, nome) == 0) { livre = i; break; }
        if (!pistas[i].nome[0] && livre < 0) livre = i;
    }
    if (livre < 0) return SDL_FALSE;
    sagui_sdl_strlcpy(pistas[livre].nome, nome, sizeof pistas[livre].nome);
    sagui_sdl_strlcpy(pistas[livre].valor, valor ? valor : "", sizeof pistas[livre].valor);
    return SDL_TRUE;
}
SDL_bool SDL_SetHint(const char *nome, const char *valor) { return SDL_SetHintWithPriority(nome, valor, SDL_HINT_NORMAL); }
const char *SDL_GetHint(const char *nome)
{
    for (int i = 0; nome && i < N_PISTAS; i++)
        if (pistas[i].nome[0] && strcmp(pistas[i].nome, nome) == 0) return pistas[i].valor;
    return NULL;
}

/* ------------------------------------------------------------------ rectángulos */

SDL_bool SDL_IntersectRect(const SDL_Rect *a, const SDL_Rect *b, SDL_Rect *r)
{
    if (!a || !b || !r) return SDL_FALSE;
    int x0 = a->x > b->x ? a->x : b->x, y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
    int y1 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;
    r->x = x0; r->y = y0;
    r->w = x1 > x0 ? x1 - x0 : 0;
    r->h = y1 > y0 ? y1 - y0 : 0;
    return (r->w > 0 && r->h > 0) ? SDL_TRUE : SDL_FALSE;
}
SDL_bool SDL_HasIntersection(const SDL_Rect *a, const SDL_Rect *b)
{
    SDL_Rect r;
    return SDL_IntersectRect(a, b, &r);
}

/* ------------------------------------------------------------------ paletas y formatos */

SDL_Palette *SDL_AllocPalette(int n)
{
    if (n <= 0) n = 256;
    SDL_Palette *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->colors = malloc((size_t)n * sizeof(SDL_Color));
    if (!p->colors) { free(p); return NULL; }
    for (int i = 0; i < n; i++) {   /* como la SDL: todo blanco */
        p->colors[i].r = p->colors[i].g = p->colors[i].b = 255;
        p->colors[i].a = 255;
    }
    p->ncolors = n;
    p->refcount = 1;
    p->version = 1;
    return p;
}

void SDL_FreePalette(SDL_Palette *p)
{
    if (!p || --p->refcount > 0) return;
    free(p->colors);
    free(p);
}

int SDL_SetPaletteColors(SDL_Palette *p, const SDL_Color *cores, int primeiro, int n)
{
    if (!p || !cores || primeiro < 0) return SDL_SetError("SDL_SetPaletteColors: paleta no válida");
    if (primeiro + n > p->ncolors) n = p->ncolors - primeiro;
    if (n > 0) memcpy(p->colors + primeiro, cores, (size_t)n * sizeof(SDL_Color));
    p->version++;
    return 0;
}

static void formato_preparar(SDL_PixelFormat *f, Uint32 formato)
{
    memset(f, 0, sizeof *f);
    f->format = formato;
    f->BitsPerPixel = (Uint8)SDL_BITSPERPIXEL(formato);
    f->BytesPerPixel = (Uint8)SDL_BYTESPERPIXEL(formato);
    f->refcount = 1;
    if (SDL_PIXELTYPE(formato) == SDL_PIXELTYPE_PACKED32) {
        int ordem = SDL_PIXELORDER(formato);
        /* Lo que se usa en el SaguiOS: ARGB/XRGB (0xAARRGGBB). Los demás
         * órdenes se describen bien pero se dibujan como ese. */
        if (ordem == SDL_PACKEDORDER_ABGR || ordem == SDL_PACKEDORDER_XBGR) {
            f->Rmask = 0x000000FF; f->Gmask = 0x0000FF00; f->Bmask = 0x00FF0000;
            f->Rshift = 0; f->Gshift = 8; f->Bshift = 16;
        } else {
            f->Rmask = 0x00FF0000; f->Gmask = 0x0000FF00; f->Bmask = 0x000000FF;
            f->Rshift = 16; f->Gshift = 8; f->Bshift = 0;
        }
        if (SDL_ISPIXELFORMAT_ALPHA(formato)) { f->Amask = 0xFF000000; f->Ashift = 24; }
        else f->Aloss = 8;
    }
}

SDL_PixelFormat *SDL_AllocFormat(Uint32 formato)
{
    SDL_PixelFormat *f = malloc(sizeof *f);
    if (f) formato_preparar(f, formato);
    return f;
}
void SDL_FreeFormat(SDL_PixelFormat *f) { free(f); }

const char *SDL_GetPixelFormatName(Uint32 f)
{
    switch (f) {
    case SDL_PIXELFORMAT_INDEX8: return "SDL_PIXELFORMAT_INDEX8";
    case SDL_PIXELFORMAT_RGB888: return "SDL_PIXELFORMAT_RGB888";
    case SDL_PIXELFORMAT_BGR888: return "SDL_PIXELFORMAT_BGR888";
    case SDL_PIXELFORMAT_ARGB8888: return "SDL_PIXELFORMAT_ARGB8888";
    case SDL_PIXELFORMAT_RGBA8888: return "SDL_PIXELFORMAT_RGBA8888";
    case SDL_PIXELFORMAT_ABGR8888: return "SDL_PIXELFORMAT_ABGR8888";
    case SDL_PIXELFORMAT_BGRA8888: return "SDL_PIXELFORMAT_BGRA8888";
    case SDL_PIXELFORMAT_RGB565: return "SDL_PIXELFORMAT_RGB565";
    case SDL_PIXELFORMAT_RGB24: return "SDL_PIXELFORMAT_RGB24";
    default: return "SDL_PIXELFORMAT_UNKNOWN";
    }
}

static Uint32 cor_de(const SDL_Color *c) { return 0xFF000000u | ((Uint32)c->r << 16) | ((Uint32)c->g << 8) | c->b; }

Uint32 SDL_MapRGBA(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!f) return 0;
    if (f->BytesPerPixel == 1 && f->palette) {
        /* La entrada más parecida de la paleta. */
        int melhor = 0;
        long dmin = 0x7FFFFFFF;
        for (int i = 0; i < f->palette->ncolors; i++) {
            const SDL_Color *c = &f->palette->colors[i];
            long d = (long)(c->r - r) * (c->r - r) + (long)(c->g - g) * (c->g - g) + (long)(c->b - b) * (c->b - b);
            if (d < dmin) { dmin = d; melhor = i; if (!d) break; }
        }
        return (Uint32)melhor;
    }
    return ((Uint32)r << f->Rshift) | ((Uint32)g << f->Gshift) | ((Uint32)b << f->Bshift) |
           (f->Amask ? ((Uint32)a << f->Ashift) : 0);
}
Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b) { return SDL_MapRGBA(f, r, g, b, 255); }

void SDL_GetRGBA(Uint32 p, const SDL_PixelFormat *f, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a)
{
    Uint8 cr = 0, cg = 0, cb = 0, ca = 255;
    if (f && f->BytesPerPixel == 1 && f->palette) {
        if ((int)p < f->palette->ncolors) {
            const SDL_Color *c = &f->palette->colors[p];
            cr = c->r; cg = c->g; cb = c->b; ca = c->a;
        }
    } else if (f) {
        cr = (Uint8)(p >> f->Rshift); cg = (Uint8)(p >> f->Gshift); cb = (Uint8)(p >> f->Bshift);
        if (f->Amask) ca = (Uint8)(p >> f->Ashift);
    }
    if (r) *r = cr;
    if (g) *g = cg;
    if (b) *b = cb;
    if (a) *a = ca;
}
void SDL_GetRGB(Uint32 p, const SDL_PixelFormat *f, Uint8 *r, Uint8 *g, Uint8 *b) { SDL_GetRGBA(p, f, r, g, b, NULL); }

/* ------------------------------------------------------------------ superficies */

/* Solo hay dos clases de superficie: de 8 bits con paleta y de 32 bits. */
static Uint32 formato_por_bits(int bits, Uint32 amask)
{
    if (bits <= 8) return SDL_PIXELFORMAT_INDEX8;
    return amask ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGB888;
}

static SDL_Surface *superficie_nova(int l, int a, Uint32 formato, void *pixels, int pitch)
{
    if (l < 0 || a < 0) { SDL_SetError("superficie de %dx%d", l, a); return NULL; }
    int bpp = SDL_BYTESPERPIXEL(formato);
    if (bpp != 1 && bpp != 4) { SDL_SetError("formato %s no disponible", SDL_GetPixelFormatName(formato)); return NULL; }
    SDL_Surface *s = calloc(1, sizeof *s);
    if (!s) { SDL_SetError("sin memoria"); return NULL; }
    s->format = calloc(1, sizeof(SDL_PixelFormat));
    if (!s->format) { free(s); SDL_SetError("sin memoria"); return NULL; }
    formato_preparar(s->format, formato);
    s->w = l;
    s->h = a;
    if (pixels) {
        s->pixels = pixels;
        s->pitch = pitch;
        s->flags |= SDL_PREALLOC;
    } else {
        s->pitch = (l * bpp + 3) & ~3;
        size_t tam = (size_t)s->pitch * (size_t)(a ? a : 1);
        s->pixels = calloc(1, tam);
        if (!s->pixels) { free(s->format); free(s); SDL_SetError("sin memoria para %dx%d", l, a); return NULL; }
    }
    if (bpp == 1) {
        s->format->palette = SDL_AllocPalette(256);
        /* Una escala de grises, como la paleta por defecto de la SDL para 8 bits. */
        if (s->format->palette)
            for (int i = 0; i < 256; i++) {
                SDL_Color *c = &s->format->palette->colors[i];
                c->r = c->g = c->b = (Uint8)i;
            }
    }
    s->clip_rect.w = l;
    s->clip_rect.h = a;
    s->refcount = 1;
    return s;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int l, int a, int bits, Uint32 r, Uint32 g, Uint32 b, Uint32 am)
{
    (void)flags; (void)r; (void)g; (void)b;
    return superficie_nova(l, a, formato_por_bits(bits, am), NULL, 0);
}
SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int l, int a, int bits, Uint32 formato)
{
    (void)flags; (void)bits;
    return superficie_nova(l, a, formato, NULL, 0);
}
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *px, int l, int a, int bits, int pitch, Uint32 r, Uint32 g, Uint32 b, Uint32 am)
{
    (void)r; (void)g; (void)b;
    return superficie_nova(l, a, formato_por_bits(bits, am), px, pitch);
}
SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *px, int l, int a, int bits, int pitch, Uint32 formato)
{
    (void)bits;
    return superficie_nova(l, a, formato, px, pitch);
}

void SDL_FreeSurface(SDL_Surface *s)
{
    if (!s || (s->flags & SDL_DONTFREE)) return;
    if (--s->refcount > 0) return;
    if (s->format) {
        SDL_FreePalette(s->format->palette);
        free(s->format);
    }
    if (!(s->flags & SDL_PREALLOC)) free(s->pixels);
    free(s);
}

int SDL_SetSurfacePalette(SDL_Surface *s, SDL_Palette *p)
{
    if (!s || !s->format || s->format->BytesPerPixel != 1) return SDL_SetError("la superficie no tiene paleta");
    if (p == s->format->palette) return 0;
    if (p) p->refcount++;
    SDL_FreePalette(s->format->palette);
    s->format->palette = p;
    return 0;
}

int  SDL_LockSurface(SDL_Surface *s) { if (s) s->locked++; return 0; }
void SDL_UnlockSurface(SDL_Surface *s) { if (s && s->locked) s->locked--; }

SDL_bool SDL_SetClipRect(SDL_Surface *s, const SDL_Rect *r)
{
    if (!s) return SDL_FALSE;
    SDL_Rect todo = { 0, 0, s->w, s->h };
    if (!r) { s->clip_rect = todo; return SDL_TRUE; }
    return SDL_IntersectRect(r, &todo, &s->clip_rect);
}
void SDL_GetClipRect(SDL_Surface *s, SDL_Rect *r) { if (s && r) *r = s->clip_rect; }

int SDL_SetColorKey(SDL_Surface *s, int ativo, Uint32 chave)
{
    if (!s) return -1;
    s->sagui_tem_chave = ativo != 0;
    s->sagui_chave = chave;
    return 0;
}
int SDL_GetColorKey(SDL_Surface *s, Uint32 *chave)
{
    if (!s || !s->sagui_tem_chave) return SDL_SetError("sin color clave");
    if (chave) *chave = s->sagui_chave;
    return 0;
}
int SDL_SetSurfaceBlendMode(SDL_Surface *s, SDL_BlendMode m) { if (!s) return -1; s->sagui_blend_set = 1; s->sagui_blend = (int)m; return 0; }
/* D374: alfa global y modo de mezcla por superficie; los aplican los blits de 32 a 32 bits. */
int SDL_SetSurfaceAlphaMod(SDL_Surface *s, Uint8 a) { if (!s) return -1; s->sagui_alfa_mod = a; s->sagui_alfa_set = 1; return 0; }
int SDL_GetSurfaceAlphaMod(SDL_Surface *s, Uint8 *a) { if (!s) return -1; if (a) *a = s->sagui_alfa_set ? s->sagui_alfa_mod : 255; return 0; }
static SDL_EventFilter filtro_eventos; static void *filtro_datos;
void SDL_SetEventFilter(SDL_EventFilter f, void *d) { filtro_eventos = f; filtro_datos = d; (void)filtro_eventos; (void)filtro_datos; }
int SDL_SetSurfaceRLE(SDL_Surface *s, int rle) { (void)s; (void)rle; return 0; }

int SDL_FillRect(SDL_Surface *d, const SDL_Rect *rect, Uint32 cor)
{
    if (!d) return -1;
    SDL_Rect r;
    if (rect) { if (!SDL_IntersectRect(rect, &d->clip_rect, &r)) return 0; }
    else r = d->clip_rect;
    for (int y = r.y; y < r.y + r.h; y++) {
        Uint8 *linha = (Uint8 *)d->pixels + (size_t)y * d->pitch;
        if (d->format->BytesPerPixel == 1) memset(linha + r.x, (int)(cor & 0xFF), (size_t)r.w);
        else preencher32((uint32_t *)linha + r.x, cor, (size_t)r.w);
    }
    return 0;
}
int SDL_FillRects(SDL_Surface *d, const SDL_Rect *rs, int n, Uint32 cor)
{
    for (int i = 0; i < n; i++) SDL_FillRect(d, &rs[i], cor);
    return 0;
}

/* Recorta el blit: el origen a su superficie y el destino al recorte del
 * destino, moviendo los dos a la vez. Deja en *rd lo que se dibujó. */
static int recortar_blit(SDL_Surface *o, const SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd, SDL_Rect *so, SDL_Rect *sd)
{
    SDL_Rect r = ro ? *ro : (SDL_Rect){ 0, 0, o->w, o->h };
    int dx = rd ? rd->x : 0, dy = rd ? rd->y : 0;
    /* El origen, dentro de su superficie. */
    if (r.x < 0) { dx -= r.x; r.w += r.x; r.x = 0; }
    if (r.y < 0) { dy -= r.y; r.h += r.y; r.y = 0; }
    if (r.x + r.w > o->w) r.w = o->w - r.x;
    if (r.y + r.h > o->h) r.h = o->h - r.y;
    /* El destino, dentro del recorte. */
    const SDL_Rect *c = &d->clip_rect;
    if (dx < c->x) { int k = c->x - dx; r.x += k; r.w -= k; dx = c->x; }
    if (dy < c->y) { int k = c->y - dy; r.y += k; r.h -= k; dy = c->y; }
    if (dx + r.w > c->x + c->w) r.w = c->x + c->w - dx;
    if (dy + r.h > c->y + c->h) r.h = c->y + c->h - dy;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    *so = r;
    sd->x = dx; sd->y = dy; sd->w = r.w; sd->h = r.h;
    if (rd) *rd = *sd;
    return r.w > 0 && r.h > 0;
}

static void paleta32(const SDL_Surface *s, Uint32 *t)
{
    const SDL_Palette *p = s->format->palette;
    for (int i = 0; i < 256; i++)
        t[i] = (p && i < p->ncolors) ? cor_de(&p->colors[i]) : 0xFF000000u;
}

/* D374: mezcla un píxel de 32 bits con alfa (SDL_BLENDMODE_BLEND) sobre otro, cada uno con su formato. */
static inline Uint32 mezclar_px(const SDL_PixelFormat *fs, Uint32 s, const SDL_PixelFormat *fd, Uint32 d, unsigned amod)
{
    unsigned sa = (unsigned)((s & fs->Amask) >> fs->Ashift);
    if (amod != 255) sa = (sa * amod + 127) / 255;
    if (sa == 0) return d;
    unsigned sr = (s & fs->Rmask) >> fs->Rshift, sg = (s & fs->Gmask) >> fs->Gshift, sb = (s & fs->Bmask) >> fs->Bshift;
    if (sa == 255) return (sr << fd->Rshift) | (sg << fd->Gshift) | (sb << fd->Bshift) | (fd->Amask ? ((Uint32)255 << fd->Ashift) : 0);
    unsigned dr = (d & fd->Rmask) >> fd->Rshift, dg = (d & fd->Gmask) >> fd->Gshift, db = (d & fd->Bmask) >> fd->Bshift;
    unsigned da = fd->Amask ? (unsigned)((d & fd->Amask) >> fd->Ashift) : 255;
    unsigned ia = 255 - sa;
    dr = (sr * sa + dr * ia + 127) / 255; dg = (sg * sa + dg * ia + 127) / 255; db = (sb * sa + db * ia + 127) / 255;
    unsigned oa = sa + (da * ia + 127) / 255;
    return (dr << fd->Rshift) | (dg << fd->Gshift) | (db << fd->Bshift) | (fd->Amask ? ((Uint32)oa << fd->Ashift) : 0);
}

int SDL_UpperBlit(SDL_Surface *o, const SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd)
{
    if (!o || !d) return SDL_SetError("SDL_BlitSurface: superficie nula");
    SDL_Rect so, sd;
    if (!recortar_blit(o, ro, d, rd, &so, &sd)) return 0;
    int bo = o->format->BytesPerPixel, bd = d->format->BytesPerPixel;
    int chave = o->sagui_tem_chave;
    Uint32 k = o->sagui_chave;
    /* 32 -> 32: con canal alfa en el origen se mezcla, salvo que el programa pida SDL_BLENDMODE_NONE */
    unsigned amod = o->sagui_alfa_set ? o->sagui_alfa_mod : 255;
    int mezcla = bo == 4 && bd == 4 && (o->format->Amask || amod != 255) &&
                 !(o->sagui_blend_set && o->sagui_blend == SDL_BLENDMODE_NONE);
    if (bo == 1 && bd == 1) {
        for (int y = 0; y < so.h; y++) {
            const Uint8 *a = (const Uint8 *)o->pixels + (size_t)(so.y + y) * o->pitch + so.x;
            Uint8 *b = (Uint8 *)d->pixels + (size_t)(sd.y + y) * d->pitch + sd.x;
            if (!chave) { memmove(b, a, (size_t)so.w); continue; }
            for (int x = 0; x < so.w; x++)
                if (a[x] != (Uint8)k) b[x] = a[x];
        }
    } else if (bo == 1 && bd == 4) {
        MEDIR_INI();
        Uint32 t[256];
        paleta32(o, t);
        for (int y = 0; y < so.h; y++) {
            const Uint8 *a = (const Uint8 *)o->pixels + (size_t)(so.y + y) * o->pitch + so.x;
            Uint32 *b = (Uint32 *)((Uint8 *)d->pixels + (size_t)(sd.y + y) * d->pitch) + sd.x;
            if (!chave) {   /* D247: de cuatro en cuatro (lecturas primero: sin dependencias entre ellas) */
                int x = 0;
                for (; x + 4 <= so.w; x += 4) {
                    Uint32 v0 = t[a[x]], v1 = t[a[x + 1]], v2 = t[a[x + 2]], v3 = t[a[x + 3]];
                    b[x] = v0; b[x + 1] = v1; b[x + 2] = v2; b[x + 3] = v3;
                }
                for (; x < so.w; x++) b[x] = t[a[x]];
                continue;
            }
            for (int x = 0; x < so.w; x++)
                if (a[x] != (Uint8)k) b[x] = t[a[x]];
        }
        MEDIR_FIM(ciclos_paleta);
    } else if (bo == 4 && bd == 4) {
        for (int y = 0; y < so.h; y++) {
            const Uint32 *a = (const Uint32 *)((const Uint8 *)o->pixels + (size_t)(so.y + y) * o->pitch) + so.x;
            Uint32 *b = (Uint32 *)((Uint8 *)d->pixels + (size_t)(sd.y + y) * d->pitch) + sd.x;
            if (mezcla) {
                for (int x = 0; x < so.w; x++) {
                    if (chave && (a[x] & 0xFFFFFF) == (k & 0xFFFFFF)) continue;
                    b[x] = mezclar_px(o->format, a[x], d->format, b[x], amod);
                }
                continue;
            }
            if (!chave) { memmove(b, a, (size_t)so.w * 4); continue; }
            for (int x = 0; x < so.w; x++)
                if ((a[x] & 0xFFFFFF) != (k & 0xFFFFFF)) b[x] = a[x];
        }
    } else {
        /* De 32 a 8 bits: por la paleta del destino (lento; los juegos no lo usan). */
        for (int y = 0; y < so.h; y++) {
            const Uint32 *a = (const Uint32 *)((const Uint8 *)o->pixels + (size_t)(so.y + y) * o->pitch) + so.x;
            Uint8 *b = (Uint8 *)d->pixels + (size_t)(sd.y + y) * d->pitch + sd.x;
            for (int x = 0; x < so.w; x++)
                b[x] = (Uint8)SDL_MapRGB(d->format, (Uint8)(a[x] >> 16), (Uint8)(a[x] >> 8), (Uint8)a[x]);
        }
    }
    return 0;
}

/* Blit escalado (vecino más próximo), sin color clave. */
int SDL_UpperBlitScaled(SDL_Surface *o, const SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd)
{
    if (!o || !d) return -1;
    SDL_Rect a = ro ? *ro : (SDL_Rect){ 0, 0, o->w, o->h };
    SDL_Rect b = rd ? *rd : (SDL_Rect){ 0, 0, d->w, d->h };
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) return 0;
    Uint32 t[256];
    if (o->format->BytesPerPixel == 1) paleta32(o, t);
    for (int y = 0; y < b.h; y++) {
        int dy = b.y + y;
        if (dy < d->clip_rect.y || dy >= d->clip_rect.y + d->clip_rect.h) continue;
        int sy = a.y + (int)((long long)y * a.h / b.h);
        if (sy < 0 || sy >= o->h) continue;
        for (int x = 0; x < b.w; x++) {
            int dx = b.x + x;
            if (dx < d->clip_rect.x || dx >= d->clip_rect.x + d->clip_rect.w) continue;
            int sx = a.x + (int)((long long)x * a.w / b.w);
            if (sx < 0 || sx >= o->w) continue;
            const Uint8 *lo = (const Uint8 *)o->pixels + (size_t)sy * o->pitch;
            Uint8 *ld = (Uint8 *)d->pixels + (size_t)dy * d->pitch;
            Uint32 v = o->format->BytesPerPixel == 1 ? t[lo[sx]] : ((const Uint32 *)lo)[sx];
            if (d->format->BytesPerPixel == 4 && o->format->BytesPerPixel == 4 && o->format->Amask &&
                !(o->sagui_blend_set && o->sagui_blend == SDL_BLENDMODE_NONE))
                ((Uint32 *)ld)[dx] = mezclar_px(o->format, v, d->format, ((Uint32 *)ld)[dx], o->sagui_alfa_set ? o->sagui_alfa_mod : 255);
            else if (d->format->BytesPerPixel == 4) ((Uint32 *)ld)[dx] = v;
            else ld[dx] = o->format->BytesPerPixel == 1 ? lo[sx] : (Uint8)SDL_MapRGB(d->format, (Uint8)(v >> 16), (Uint8)(v >> 8), (Uint8)v);
        }
    }
    return 0;
}

SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *s, Uint32 formato, Uint32 flags)
{
    (void)flags;
    if (!s) return NULL;
    SDL_Surface *n = superficie_nova(s->w, s->h, formato, NULL, 0);
    if (!n) return NULL;
    if (n->format->BytesPerPixel == 1 && s->format->palette)
        SDL_SetPaletteColors(n->format->palette, s->format->palette->colors, 0, s->format->palette->ncolors);
    int k = s->sagui_tem_chave;
    s->sagui_tem_chave = 0;
    SDL_UpperBlit(s, NULL, n, NULL);
    s->sagui_tem_chave = k;
    n->sagui_tem_chave = k;
    n->sagui_chave = s->sagui_chave;
    return n;
}
SDL_Surface *SDL_ConvertSurface(SDL_Surface *s, const SDL_PixelFormat *f, Uint32 flags)
{
    return SDL_ConvertSurfaceFormat(s, f ? f->format : SDL_PIXELFORMAT_ARGB8888, flags);
}

/* ------------------------------------------------------------------ la ventana */

/* D230: SDL_ShowCursor(SDL_DISABLE) oculta el puntero del sistema sobre la ventana
 * (JAN_SEM_CURSOR); va en cada estilo que se pide, para no perderlo. */
static int cursor_visivel = 1;
#define ESTILO_BASE (JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK | JAN_ESC_LIVRE | (cursor_visivel ? 0 : JAN_SEM_CURSOR))

struct SDL_Window {
    int id;                 /* la ventana del sistema */
    uint32_t *px;           /* sus píxeles */
    int jl, ja;             /* su tamaño */
    int l, a;               /* la ventana lógica (lo que ve el programa) */
    int em_cheia;
    Uint32 flags;
    char titulo[96];
    uint32_t *tela;         /* el lienzo lógico, l x a */
    SDL_Surface *superficie; /* SDL_GetWindowSurface: el lienzo como superficie */
    SDL_Renderer *renderer;
    /* El escalado del lienzo a la ventana del sistema. */
    int dx, dy, dl, da;
    int *tcol, *tfila, tcol_n, tfila_n;
    int primeiro_quadro;
    /* D247: lo último que se mandó a la ventana, para enviar solo lo que cambia. */
    uint32_t *ant;          /* copia del lienzo tal como se presentó (l x a) */
    size_t ant_n;
    int completo;           /* la próxima presentación es entera (ventana nueva, otra escala...) */
    /* D432: la superficie de la ventana (SDL_UpdateWindowSurface) presentada por la GPU. */
    struct sdlgpu *sup_gpu; /* el motor 3D abierto para subir el lienzo y presentarlo (NULL: no) */
    int sup_nao;            /* no se puede o no se pidió: no se vuelve a intentar */
};

static SDL_Window *janela = NULL;   /* una sola */
static int superficie_gpu(SDL_Window *w);          /* D432 */
static void superficie_gpu_fechar(SDL_Window *w);

static void tratar_evento(evento_t *e);

/* Dónde va el lienzo en la ventana del sistema y las tablas del escalado. */
static void preparar_escala(SDL_Window *w)
{
    int l = w->l, a = w->a;
    int esc = sagui_sdl2_escala < 1 ? 1 : sagui_sdl2_escala;
    if (!w->em_cheia && w->jl == l * esc && w->ja == a * esc) {
        w->dl = w->jl; w->da = w->ja;
    } else {
        /* 320x200 y 640x400 son 16:10 con píxeles cuadrados, pero la VGA los
         * mostraba en una pantalla 4:3: se ven 4:3. Las demás, con la suya. */
        int pn = l, pd = a;
        if (l * 3 == a * 4 || l * 10 == a * 16) { pn = 4; pd = 3; }
        w->da = w->ja;
        w->dl = (int)((long long)w->ja * pn / pd);
        if (w->dl > w->jl) { w->dl = w->jl; w->da = (int)((long long)w->jl * pd / pn); }
    }
    if (w->dl < 1) w->dl = 1;
    if (w->da < 1) w->da = 1;
    w->dx = (w->jl - w->dl) / 2;
    w->dy = (w->ja - w->da) / 2;
    if (w->tcol_n < w->dl) { free(w->tcol); w->tcol = malloc((size_t)w->dl * sizeof(int)); w->tcol_n = w->tcol ? w->dl : 0; }
    if (w->tfila_n < w->da) { free(w->tfila); w->tfila = malloc((size_t)w->da * sizeof(int)); w->tfila_n = w->tfila ? w->da : 0; }
    if (!w->tcol || !w->tfila) { w->dl = w->da = 0; return; }
    for (int x = 0; x < w->dl; x++) w->tcol[x] = (int)((long long)x * l / w->dl);
    for (int y = 0; y < w->da; y++) w->tfila[y] = (int)((long long)y * a / w->da);
    if (w->px) preencher32(w->px, 0, (size_t)w->jl * (size_t)w->ja);   /* las bandas, negras */
    w->completo = 1;
    registrar("[%s] escala: lienzo %dx%d en ventana %dx%d -> %dx%d en (%d,%d)",
              sagui_sdl2_nome, l, a, w->jl, w->ja, w->dl, w->da, w->dx, w->dy);
}

/* D247: solo lo que cambia. El lienzo se compara con lo último que se presentó, por franjas de
 * alto fijo; cada franja con cambios da una caja (las columnas que cambian) y las cajas se
 * funden hasta quedar pocas. Solo esas zonas se escalan a la ventana y solo ellas se avisan al
 * compositor (sis_atualizar_janela acepta un rectángulo, D245): en un juego de estrategia, donde
 * la mayor parte de la pantalla está quieta, es una fracción de la ventana. */
#define MAX_CAIXAS 4
#define MAX_FAIXAS 256
typedef struct { int x0, x1, y0, y1; } zona_t;   /* [x0,x1) x [y0,y1), en el lienzo */

static long zona_area(const zona_t *c) { return (long)(c->x1 - c->x0) * (c->y1 - c->y0); }
static zona_t zona_uniao(const zona_t *a, const zona_t *b)
{
    zona_t u = { a->x0 < b->x0 ? a->x0 : b->x0, a->x1 > b->x1 ? a->x1 : b->x1,
                  a->y0 < b->y0 ? a->y0 : b->y0, a->y1 > b->y1 ? a->y1 : b->y1 };
    return u;
}

/* Cajas con cambios entre el lienzo y la copia anterior (que deja al día). Devuelve cuántas. */
static int achar_mudancas(SDL_Window *w, zona_t *cx)
{
    int l = w->l, a = w->a;
    int alt = (a + MAX_FAIXAS - 1) / MAX_FAIXAS;
    if (alt < 16) alt = 16;
    zona_t lista[MAX_FAIXAS];
    int n = 0;
    for (int y0 = 0; y0 < a; y0 += alt) {
        int y1 = y0 + alt < a ? y0 + alt : a, xmin = l, xmax = 0;
        for (int y = y0; y < y1; y++) {
            const uint32_t *p = w->tela + (size_t)y * l;
            uint32_t *q = w->ant + (size_t)y * l;
            int i = 0;
            /* de ocho en ocho (sin saltos por píxel), y después píxel a píxel dentro del bloque */
            while (i + 8 <= l && !((p[i] ^ q[i]) | (p[i + 1] ^ q[i + 1]) | (p[i + 2] ^ q[i + 2]) | (p[i + 3] ^ q[i + 3]) |
                                   (p[i + 4] ^ q[i + 4]) | (p[i + 5] ^ q[i + 5]) | (p[i + 6] ^ q[i + 6]) | (p[i + 7] ^ q[i + 7])))
                i += 8;
            while (i < l && p[i] == q[i]) i++;
            if (i == l) continue;
            int j = l;   /* uno más que el último distinto */
            while (j - 8 > i && !((p[j - 1] ^ q[j - 1]) | (p[j - 2] ^ q[j - 2]) | (p[j - 3] ^ q[j - 3]) | (p[j - 4] ^ q[j - 4]) |
                                  (p[j - 5] ^ q[j - 5]) | (p[j - 6] ^ q[j - 6]) | (p[j - 7] ^ q[j - 7]) | (p[j - 8] ^ q[j - 8])))
                j -= 8;
            while (p[j - 1] == q[j - 1]) j--;
            j--;
            if (i < xmin) xmin = i;
            if (j + 1 > xmax) xmax = j + 1;
            memcpy(q + i, p + i, (size_t)(j + 1 - i) * 4);
        }
        if (xmin >= xmax) continue;
        zona_t c = { xmin, xmax, y0, y1 };
        /* una franja pegada a la anterior con las mismas columnas: la misma caja */
        if (n && lista[n - 1].y1 == y0 && lista[n - 1].x0 == xmin && lista[n - 1].x1 == xmax) lista[n - 1].y1 = y1;
        else lista[n++] = c;
    }
    /* Fundir las cajas: mientras sobren, o la fusión desperdicie poco, las dos que menos añaden. */
    while (n > 1) {
        int mejor = -1;
        long peor = 0, mejor_p = 0;
        for (int k = 0; k + 1 < n; k++) {
            zona_t u = zona_uniao(&lista[k], &lista[k + 1]);
            long perd = zona_area(&u) - zona_area(&lista[k]) - zona_area(&lista[k + 1]);
            if (mejor < 0 || perd < peor) { mejor = k; peor = perd; mejor_p = zona_area(&u); }
        }
        if (n <= MAX_CAIXAS && peor * 4 > mejor_p) break;
        lista[mejor] = zona_uniao(&lista[mejor], &lista[mejor + 1]);
        for (int k = mejor + 1; k + 1 < n; k++) lista[k] = lista[k + 1];
        n--;
    }
    memcpy(cx, lista, (size_t)n * sizeof lista[0]);
    return n;
}

/* Primer índice de t[0..n) con t[i] >= v (t no decrece). */
static int primeiro_ge(const int *t, int n, int v)
{
    int lo = 0, hi = n;
    while (lo < hi) {
        int m = (lo + hi) / 2;
        if (t[m] >= v) hi = m; else lo = m + 1;
    }
    return lo;
}

/* Escala la caja del lienzo a la ventana y devuelve el rectángulo de la ventana que cambió. */
static retangulo_t escalar_caixa(SDL_Window *w, const zona_t *c)
{
    retangulo_t r;
    if (w->dl == w->l && w->da == w->a) {   /* 1:1: filas copiadas tal cual */
        for (int y = c->y0; y < c->y1; y++)
            memcpy(w->px + (size_t)(w->dy + y) * w->jl + w->dx + c->x0, w->tela + (size_t)y * w->l + c->x0,
                   (size_t)(c->x1 - c->x0) * 4);
        r.x = w->dx + c->x0; r.y = w->dy + c->y0; r.largura = c->x1 - c->x0; r.altura = c->y1 - c->y0;
        return r;
    }
    if (w->dl == 2 * w->l && w->da == 2 * w->a) {   /* x2 exacto: cada fila una vez ampliada y la otra igual */
        for (int y = c->y0; y < c->y1; y++) {
            uint32_t *d = w->px + (size_t)(w->dy + 2 * y) * w->jl + w->dx + 2 * c->x0;
            ampliar2x32(d, d + w->jl, w->tela + (size_t)y * w->l + c->x0, (size_t)(c->x1 - c->x0));
        }
        r.x = w->dx + 2 * c->x0; r.y = w->dy + 2 * c->y0; r.largura = 2 * (c->x1 - c->x0); r.altura = 2 * (c->y1 - c->y0);
        return r;
    }
    /* General: las columnas y filas de la ventana cuyo origen cae en la caja. */
    int wx0 = primeiro_ge(w->tcol, w->dl, c->x0), wx1 = primeiro_ge(w->tcol, w->dl, c->x1);
    int wy0 = primeiro_ge(w->tfila, w->da, c->y0), wy1 = primeiro_ge(w->tfila, w->da, c->y1);
    int prev = -1;
    uint32_t *d = w->px + (size_t)(w->dy + wy0) * w->jl + w->dx + wx0;
    for (int y = wy0; y < wy1; y++, d += w->jl) {
        int sy = w->tfila[y];
        if (sy == prev) { memcpy(d, d - w->jl, (size_t)(wx1 - wx0) * 4); continue; }
        prev = sy;
        const uint32_t *o = w->tela + (size_t)sy * w->l;
        for (int x = wx0; x < wx1; x++) d[x - wx0] = o[w->tcol[x]];
    }
    r.x = w->dx + wx0; r.y = w->dy + wy0; r.largura = wx1 - wx0; r.altura = wy1 - wy0;
    return r;
}

/* Copia el lienzo a la ventana del sistema y la actualiza (solo lo que cambió). */
static int apresentar(SDL_Window *w)
{
    if (!w || !w->px || !w->tela || !w->dl || !w->da) return -1;
    size_t n = (size_t)w->l * (size_t)w->a;
    if (w->ant_n != n || !w->ant) {
        free(w->ant);
        w->ant = malloc(n * 4);
        w->ant_n = w->ant ? n : 0;
        w->completo = 1;
    }
    zona_t cx[MAX_CAIXAS];
    int nc;
    if (w->completo || !w->ant) {
        cx[0] = (zona_t){ 0, w->l, 0, w->a };
        nc = 1;
        if (w->ant) memcpy(w->ant, w->tela, n * 4);
        w->completo = 0;
    } else {
        nc = achar_mudancas(w, cx);
        long tot = 0;
        for (int k = 0; k < nc; k++) tot += zona_area(&cx[k]);
        if (nc > 1 && tot * 10 >= (long)n * 9) { cx[0] = (zona_t){ 0, w->l, 0, w->a }; nc = 1; }
    }
    if (!w->primeiro_quadro) {
        w->primeiro_quadro = 1;
        registrar("[%s] primer cuadro dibujado (%dx%d)", sagui_sdl2_nome, w->jl, w->ja);
    }
    if (nc == 0) return 0;   /* nada cambió: el compositor ya tiene esta imagen */
    int v = 0;
    retangulo_t rs[MAX_CAIXAS];
    for (int k = 0; k < nc; k++) rs[k] = escalar_caixa(w, &cx[k]);
    for (int k = 0; k < nc; k++)
        if (rs[k].largura > 0 && rs[k].altura > 0) v = atualizar_janela(w->id, &rs[k]);
    return v;
}

static void ao_redimensionar(SDL_Window *w, int l, int a, int estado);

/* Espera (poco) a que el kernel confirme la pantalla completa o la salida. */
static void esperar_estado(SDL_Window *w, int alvo)
{
    unsigned limite = tempo_ms() + 700;
    while (w->em_cheia != alvo && (int)(limite - tempo_ms()) > 0) {
        evento_t e;
        if (esperar_evento(w->id, &e, 30) == 1) tratar_evento(&e);
    }
}

static int lienzo_novo(SDL_Window *w, int l, int a)
{
    uint32_t *t = calloc((size_t)l * (size_t)a, 4);
    if (!t) return SDL_SetError("sin memoria para un lienzo de %dx%d", l, a);
    free(w->tela);
    w->tela = t;
    w->l = l;
    w->a = a;
    w->completo = 1;
    if (w->superficie) {   /* la superficie de la ventana apunta al lienzo nuevo */
        w->superficie->flags &= ~SDL_DONTFREE;
        SDL_FreeSurface(w->superficie);
        w->superficie = NULL;
    }
    return 0;
}

SDL_Window *SDL_CreateWindow(const char *titulo, int x, int y, int l, int a, Uint32 flags)
{
    (void)x; (void)y;
    if (janela) { SDL_SetError("la SDL del SaguiOS tiene una sola ventana"); return NULL; }
    if (l <= 0 || a <= 0) { SDL_SetError("ventana de %dx%d", l, a); return NULL; }
    /* D444: una ventana más grande que el máximo del sistema (1000x700) se reduce conservando la proporción
     * (Brumacanto pide 1152x648); el programa sigue viendo su tamaño real con SDL_GetWindowSize. */
    if (l > JANELA_LARG_MAX || a > JANELA_ALT_MAX) {
        int l2 = l, a2 = a;
        if (l2 > JANELA_LARG_MAX) { a2 = (int)((long long)a2 * JANELA_LARG_MAX / l2); l2 = JANELA_LARG_MAX; }
        if (a2 > JANELA_ALT_MAX) { l2 = (int)((long long)l2 * JANELA_ALT_MAX / a2); a2 = JANELA_ALT_MAX; }
        l = l2 < 40 ? 40 : l2; a = a2 < 20 ? 20 : a2;
    }
    SDL_Window *w = calloc(1, sizeof *w);
    if (!w) { SDL_SetError("sin memoria"); return NULL; }
    if (lienzo_novo(w, l, a) < 0) { free(w); return NULL; }
    int esc = sagui_sdl2_escala < 1 ? 1 : sagui_sdl2_escala;
    int jl = l * esc, ja = a * esc;
    if (jl > JANELA_LARG_MAX || ja > JANELA_ALT_MAX) { jl = l; ja = a; }   /* no cabe ampliada */
    sagui_sdl_strlcpy(w->titulo, titulo ? titulo : sagui_sdl2_nome, sizeof w->titulo);
    w->id = criar_janela(w->titulo, jl, ja, &w->px);
    if (w->id < 0 || !w->px) {
        free(w->tela);
        free(w);
        SDL_SetError("no se pudo abrir la ventana");
        return NULL;
    }
    w->jl = jl;
    w->ja = ja;
    w->flags = (flags & ~(SDL_WINDOW_FULLSCREEN_DESKTOP)) | SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
    janela = w;
    estilo_janela(w->id, ESTILO_BASE, 0, 0);   /* F11 del kernel; el Esc llega al programa */
    registrar("[%s] ventana %dx%d creada (lienzo %dx%d)", sagui_sdl2_nome, jl, ja, l, a);
    preparar_escala(w);
    if ((flags & SDL_WINDOW_FULLSCREEN) || sagui_sdl2_cheia_inicial) {
        sagui_sdl2_cheia_inicial = 0;
        SDL_SetWindowFullscreen(w, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    return w;
}

void SDL_DestroyWindow(SDL_Window *w)
{
    if (!w) return;
    if (w->renderer) SDL_DestroyRenderer(w->renderer);
    superficie_gpu_fechar(w);
    if (w->superficie) { w->superficie->flags &= ~SDL_DONTFREE; SDL_FreeSurface(w->superficie); }
    fechar_janela(w->id);
    free(w->tela);
    free(w->ant);
    free(w->tcol);
    free(w->tfila);
    if (janela == w) janela = NULL;
    free(w);
}

/* D425: lo que necesita EGL (sdl2_gl.c) para dibujar en esta ventana. */
int sagui_sdl2_janela_nativa(SDL_Window *w, int *id, uint32_t **px, int *l, int *a)
{
    if (!w || !w->px) return -1;
    *id = w->id; *px = w->px; *l = w->jl; *a = w->ja;
    return 0;
}

void SDL_SetWindowTitle(SDL_Window *w, const char *t)
{
    if (!w || !t) return;
    sagui_sdl_strlcpy(w->titulo, t, sizeof w->titulo);
    titulo_janela(w->id, w->titulo);
}
const char *SDL_GetWindowTitle(SDL_Window *w) { return w ? w->titulo : ""; }

void SDL_GetWindowSize(SDL_Window *w, int *l, int *a)
{
    if (l) *l = w ? w->l : 0;
    if (a) *a = w ? w->a : 0;
}

void SDL_SetWindowSize(SDL_Window *w, int l, int a)
{
    if (!w || l <= 0 || a <= 0 || (l == w->l && a == w->a)) return;
    if (lienzo_novo(w, l, a) < 0) return;
    if (!w->em_cheia) {
        int esc = sagui_sdl2_escala < 1 ? 1 : sagui_sdl2_escala;
        uint32_t *px = NULL;
        if (redimensionar_janela(w->id, l * esc, a * esc, &px) >= 0 && px) { w->px = px; w->jl = l * esc; w->ja = a * esc; }
    }
    preparar_escala(w);
}

void SDL_SetWindowMinimumSize(SDL_Window *w, int l, int a) { (void)w; (void)l; (void)a; }
void SDL_GetWindowPosition(SDL_Window *w, int *x, int *y) { (void)w; if (x) *x = 0; if (y) *y = 0; }
void SDL_SetWindowPosition(SDL_Window *w, int x, int y) { (void)w; (void)x; (void)y; }

int SDL_SetWindowFullscreen(SDL_Window *w, Uint32 flags)
{
    if (!w) return -1;
    int quer = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (quer == w->em_cheia) return 0;
    estilo_janela(w->id, ESTILO_BASE | (quer ? JAN_TELA_CHEIA : 0), 0, 0);
    esperar_estado(w, quer);
    registrar("[%s] pantalla completa %s: ventana %dx%d", sagui_sdl2_nome,
              w->em_cheia ? "activada" : "desactivada", w->jl, w->ja);
    if (!w->em_cheia) {
        /* Al salir, la ventana vuelve a su tamaño. */
        int esc = sagui_sdl2_escala < 1 ? 1 : sagui_sdl2_escala;
        if (w->jl != w->l * esc || w->ja != w->a * esc) ao_redimensionar(w, w->l * esc, w->a * esc, JAN_ESTADO_NORMAL);
    }
    return w->em_cheia == quer ? 0 : SDL_SetError("el sistema no cambió la pantalla completa");
}

Uint32 SDL_GetWindowFlags(SDL_Window *w)
{
    if (!w) return 0;
    return (w->flags & ~SDL_WINDOW_FULLSCREEN_DESKTOP) | (w->em_cheia ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}
Uint32 SDL_GetWindowPixelFormat(SDL_Window *w) { (void)w; return SDL_PIXELFORMAT_RGB888; }
Uint32 SDL_GetWindowID(SDL_Window *w) { return w ? 1 : 0; }
void SDL_SetWindowGrab(SDL_Window *w, SDL_bool g)
{
    if (!w) return;
    if (g) w->flags |= SDL_WINDOW_INPUT_GRABBED;
    else w->flags &= ~SDL_WINDOW_INPUT_GRABBED;
}
SDL_bool SDL_GetWindowGrab(SDL_Window *w) { return (w && (w->flags & SDL_WINDOW_INPUT_GRABBED)) ? SDL_TRUE : SDL_FALSE; }
void SDL_ShowWindow(SDL_Window *w) { (void)w; }
void SDL_HideWindow(SDL_Window *w) { (void)w; }
void SDL_RaiseWindow(SDL_Window *w) { (void)w; }
void SDL_SetWindowIcon(SDL_Window *w, SDL_Surface *i) { (void)w; (void)i; }
void SDL_SetWindowResizable(SDL_Window *w, SDL_bool s) { (void)w; (void)s; }
SDL_Window *SDL_GetKeyboardFocus(void) { return janela; }
SDL_Window *SDL_GetMouseFocus(void) { return janela; }

SDL_Surface *SDL_GetWindowSurface(SDL_Window *w)
{
    if (!w) return NULL;
    if (!w->superficie) {
        w->superficie = superficie_nova(w->l, w->a, SDL_PIXELFORMAT_RGB888, w->tela, w->l * 4);
        if (w->superficie) w->superficie->flags |= SDL_DONTFREE;
    }
    return w->superficie;
}
int SDL_UpdateWindowSurface(SDL_Window *w)
{
    SDL_PumpEvents();
    if (w && !w->sup_nao && superficie_gpu(w)) return 0;   /* D432: presentado por la GPU en la pantalla */
    return apresentar(w);
}
int SDL_UpdateWindowSurfaceRects(SDL_Window *w, const SDL_Rect *rs, int n)
{
    (void)rs; (void)n;
    return SDL_UpdateWindowSurface(w);
}

static void modo_da_tela(SDL_DisplayMode *m)
{
    memset(m, 0, sizeof *m);
    m->format = SDL_PIXELFORMAT_RGB888;
    m->w = janela && janela->em_cheia ? janela->jl : 1024;
    m->h = janela && janela->em_cheia ? janela->ja : 768;
    m->refresh_rate = 60;
}
int SDL_GetNumVideoDisplays(void) { return 1; }
int SDL_GetDesktopDisplayMode(int t, SDL_DisplayMode *m) { (void)t; if (m) modo_da_tela(m); return 0; }
int SDL_GetCurrentDisplayMode(int t, SDL_DisplayMode *m) { (void)t; if (m) modo_da_tela(m); return 0; }
int SDL_GetWindowDisplayMode(SDL_Window *w, SDL_DisplayMode *m) { (void)w; if (m) modo_da_tela(m); return 0; }
int SDL_GetNumDisplayModes(int t) { (void)t; return 1; }
int SDL_GetDisplayMode(int t, int i, SDL_DisplayMode *m) { (void)t; if (i != 0) return -1; if (m) modo_da_tela(m); return 0; }
const char *SDL_GetCurrentVideoDriver(void) { return "sagui"; }
int SDL_GetDisplayBounds(int t, SDL_Rect *r)
{
    SDL_DisplayMode m;
    (void)t;
    modo_da_tela(&m);
    if (r) { r->x = 0; r->y = 0; r->w = m.w; r->h = m.h; }
    return 0;
}
void SDL_DisableScreenSaver(void) {}
void SDL_EnableScreenSaver(void) {}

/* ------------------------------------------------------------------ el renderer */

struct SDL_Texture {
    Uint32 formato;
    int acesso, l, a, pitch;
    Uint8 *px;
    SDL_Renderer *r;
    SDL_BlendMode modo;
    Uint32 mod;             /* SDL_SetTextureColorMod / AlphaMod: 0xAARRGGBB que multiplica (0xFFFFFFFF: nada) */
    int gpu_vaga;           /* D288: hueco de textura en la GPU (-1: ninguno) */
    int gpu_sujo;           /* sus pixeles cambiaron desde que se subieron a la GPU */
};

struct SDL_Renderer {
    SDL_Window *w;
    SDL_Surface *alvo_sup;   /* SDL_CreateSoftwareRenderer: dibuja en esa superficie */
    SDL_Texture *alvo;       /* SDL_SetRenderTarget */
    Uint8 cr, cg, cb, ca;
    int log_l, log_a;        /* SDL_RenderSetLogicalSize (0 = sin) */
    int limpar_pend;         /* D247: SDL_RenderClear sobre la ventana, aún sin pintar */
    Uint32 limpar_cor;
    SDL_BlendMode modo_des;  /* SDL_SetRenderDrawBlendMode */
    struct sdlgpu *gpu;      /* D288: la GPU lleva el lienzo de la ventana (NULL: software) */
    int recorte;             /* D436: SDL_RenderSetClipRect activo */
    SDL_Rect rec;            /* en coordenadas del programa (con el tamaño lógico) */
};

#include "sdl2gpu.h"

/* D436: los modos de mezcla compuestos se guardan como en la SDL real (SDL_COMPOSE_BLENDMODE) */
#define MEZ_COMPOR(oc, dc, opc, oa, da, opa) ((Uint32)(opc) | ((Uint32)(oc) << 4) | ((Uint32)(dc) << 8) | \
                                              ((Uint32)(opa) << 16) | ((Uint32)(oa) << 20) | ((Uint32)(da) << 24))
#define MEZ_PREMULT MEZ_COMPOR(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD, \
                               SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD)
SDL_BlendMode SDL_ComposeCustomBlendMode(SDL_BlendFactor oc, SDL_BlendFactor dc, SDL_BlendOperation opc,
                                         SDL_BlendFactor oa, SDL_BlendFactor da, SDL_BlendOperation opa)
{
    if (oc < SDL_BLENDFACTOR_ZERO || oc > SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA || dc < SDL_BLENDFACTOR_ZERO ||
        dc > SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA || oa < SDL_BLENDFACTOR_ZERO || oa > SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA ||
        da < SDL_BLENDFACTOR_ZERO || da > SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA || opc < SDL_BLENDOPERATION_ADD ||
        opc > SDL_BLENDOPERATION_MAXIMUM || opa < SDL_BLENDOPERATION_ADD || opa > SDL_BLENDOPERATION_MAXIMUM)
        return SDL_BLENDMODE_INVALID;
    return (SDL_BlendMode)MEZ_COMPOR(oc, dc, opc, oa, da, opa);
}

/* El modo de un lote de la GPU (SDLG_M_*) para un modo de la SDL; -1 si gpu3d v1 no lo sabe hacer (va por software). */
static int modo_gpu(SDL_BlendMode m)
{
    switch ((Uint32)m) {
    case SDL_BLENDMODE_NONE: return SDLG_M_NADA;
    case SDL_BLENDMODE_BLEND: return SDLG_M_ALFA;
    case SDL_BLENDMODE_ADD: return SDLG_M_SOMA;
    case MEZ_PREMULT: return SDLG_M_PREMULT;
    case MEZ_COMPOR(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD,
                    SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD): return SDLG_M_ALFA;
    default: return -1;
    }
}

/* Un factor de mezcla (0..255) de un canal k (0 B, 1 G, 2 R, 3 A) para el software. */
static unsigned fator_px(Uint32 f, Uint32 o, Uint32 d, int k)
{
    unsigned oa = o >> 24, da = d >> 24, oc = (o >> (k * 8)) & 255u, dc = (d >> (k * 8)) & 255u;
    switch (f) {
    case SDL_BLENDFACTOR_ZERO: return 0;
    case SDL_BLENDFACTOR_ONE: return 255;
    case SDL_BLENDFACTOR_SRC_COLOR: return oc;
    case SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR: return 255 - oc;
    case SDL_BLENDFACTOR_SRC_ALPHA: return oa;
    case SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA: return 255 - oa;
    case SDL_BLENDFACTOR_DST_COLOR: return dc;
    case SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR: return 255 - dc;
    case SDL_BLENDFACTOR_DST_ALPHA: return da;
    default: return 255 - da;
    }
}

/* Un pixel o (ya modulado) sobre uno d del lienzo con el modo m (D436: ADD, MOD, MUL y los compuestos, ademas de BLEND).
 * El lienzo de la ventana no tiene alfa (se queda opaco); una textura destino si lo guarda. */
static Uint32 mezcla_modo(Uint32 o, Uint32 d, SDL_BlendMode m)
{
    unsigned al = o >> 24;
    switch ((Uint32)m) {
    case SDL_BLENDMODE_NONE: return o;
    case SDL_BLENDMODE_BLEND: {
        if (al == 0) return d;
        if (al == 255) return o | 0xFF000000u;
        unsigned rr = (((o >> 16) & 255) * al + ((d >> 16) & 255) * (255 - al)) / 255;
        unsigned gg = (((o >> 8) & 255) * al + ((d >> 8) & 255) * (255 - al)) / 255;
        unsigned bb = ((o & 255) * al + (d & 255) * (255 - al)) / 255;
        unsigned aa = al + (d >> 24) * (255 - al) / 255;
        return (aa << 24) | (rr << 16) | (gg << 8) | bb;
    }
    case SDL_BLENDMODE_ADD: {
        Uint32 r = d & 0xFF000000u;
        for (int k = 0; k < 24; k += 8) {
            unsigned c = ((d >> k) & 255) + ((o >> k) & 255) * al / 255;
            r |= (Uint32)(c > 255 ? 255 : c) << k;
        }
        return r;
    }
    case SDL_BLENDMODE_MOD: case SDL_BLENDMODE_MUL: {
        Uint32 r = d & 0xFF000000u;
        for (int k = 0; k < 24; k += 8) {
            unsigned oc = (o >> k) & 255, dc = (d >> k) & 255;
            unsigned c = oc * dc / 255;
            if ((Uint32)m == SDL_BLENDMODE_MUL) c += dc * (255 - al) / 255;
            r |= (Uint32)(c > 255 ? 255 : c) << k;
        }
        return r;
    }
    default: {   /* compuesto */
        Uint32 v = (Uint32)m, r = 0;
        for (int k = 0; k < 4; k++) {
            Uint32 fo = k == 3 ? (v >> 20) & 15 : (v >> 4) & 15, fd = k == 3 ? (v >> 24) & 15 : (v >> 8) & 15;
            Uint32 op = k == 3 ? (v >> 16) & 15 : v & 15;
            int oc = (int)(((o >> (k * 8)) & 255) * fator_px(fo, o, d, k) / 255);
            int dc = (int)(((d >> (k * 8)) & 255) * fator_px(fd, o, d, k) / 255);
            int c = op == SDL_BLENDOPERATION_SUBTRACT ? oc - dc : op == SDL_BLENDOPERATION_REV_SUBTRACT ? dc - oc :
                    op == SDL_BLENDOPERATION_MINIMUM ? (oc < dc ? oc : dc) : op == SDL_BLENDOPERATION_MAXIMUM ? (oc > dc ? oc : dc) : oc + dc;
            r |= (Uint32)(c < 0 ? 0 : c > 255 ? 255 : c) << (k * 8);
        }
        return r;
    }
    }
}

/* D436: faltan huecos de textura en la GPU: se avisa en el registro (con un atlas grande no pasa). */
static void sdlg_aviso_desalojo(sdlgpu_t *g)
{
    registrar("[%s] renderizador por GPU: faltan huecos de textura (%d en la GPU): %lu texturas sacadas y vueltas a subir; "
              "mejor menos texturas y mas grandes (atlas)", sagui_sdl2_nome, g->nvagas, g->desalojos);
}

static const SDL_RendererInfo info_renderer = {
    "saguios", SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE, 3,
    { SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_RGB888, SDL_PIXELFORMAT_INDEX8 }, 4096, 4096
};
static const SDL_RendererInfo info_renderer_gpu = {
    "saguios-gpu", SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE, 3,
    { SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_RGB888, SDL_PIXELFORMAT_INDEX8 }, GPU3D_MAX_LADO, GPU3D_MAX_LADO
};

static SDL_Rect recorte_lienzo(SDL_Renderer *r, int l, int a);

/* D288: vuelta al software de este renderizador (lo dibujado se trae antes al lienzo). */
static void gpu_cair(SDL_Renderer *r, int preservar, const char *motivo)
{
    if (!r->gpu) return;
    sdlg_fechar(r->gpu, r->w ? r->w->tela : NULL, preservar);
    r->gpu = NULL;
    registrar("[%s] renderizador por GPU apagado: %s (sigue por software)", sagui_sdl2_nome, motivo);
}
/* rc de una operacion de la GPU: <0 fallo de la GPU, >0 no puede. Siempre cae al software. */
static void gpu_erro(SDL_Renderer *r, int rc)
{
    gpu_cair(r, rc > 0, rc < 0 ? "la GPU fallo" : "la GPU no admite la operacion");
}
/* El estado de la GPU si este renderizador dibuja en ella ahora mismo. */
static sdlgpu_t *gpu_de(SDL_Renderer *r)
{
    if (!r->gpu || r->alvo || r->alvo_sup) return NULL;
    if (!r->w || !r->w->tela) {
        gpu_cair(r, 1, "la ventana no tiene lienzo");
        return NULL;
    }
    if (r->w->l != r->gpu->l || r->w->a != r->gpu->a) {
        /* D432: el lienzo cambio de tamano (p. ej. al pasar a pantalla completa, como hace AoEN): se reabre el
         * destino de la GPU al tamano nuevo en vez de pasar al software. Las texturas se vuelven a subir solas
         * (sdlg_fechar las suelta y quedan sin hueco). Si la GPU no puede, el software de siempre. */
        sdlg_fechar(r->gpu, NULL, 0);
        r->gpu = sdlg_abrir(r->w->l, r->w->a);
        if (!r->gpu) {
            registrar("[%s] renderizador por GPU apagado: no se pudo reabrir a %dx%d (sigue por software)",
                      sagui_sdl2_nome, r->w->l, r->w->a);
            return NULL;
        }
        registrar("[%s] renderizador por GPU: destino reabierto a %dx%d", sagui_sdl2_nome, r->w->l, r->w->a);
    }
    sdlgpu_t *g = r->gpu;
    /* D436: el recorte del renderizador, en pixeles del destino */
    if (r->recorte) {
        SDL_Rect c = recorte_lienzo(r, g->l, g->a);
        g->cx0 = c.x; g->cy0 = c.y; g->cx1 = c.x + c.w; g->cy1 = c.y + c.h;
    } else { g->cx0 = 0; g->cy0 = 0; g->cx1 = g->l; g->cy1 = g->a; }
    return g;
}

int SDL_GetNumRenderDrivers(void) { return 1; }
int SDL_GetRenderDriverInfo(int i, SDL_RendererInfo *info)
{
    if (i != 0 || !info) return SDL_SetError("no hay ese renderer");
    *info = info_renderer;
    return 0;
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int indice, Uint32 flags)
{
    (void)indice;
    if (!w) { SDL_SetError("SDL_CreateRenderer sin ventana"); return NULL; }
    if (w->renderer) return w->renderer;
    SDL_Renderer *r = calloc(1, sizeof *r);
    if (!r) { SDL_SetError("sin memoria"); return NULL; }
    r->w = w;
    r->ca = 255;
    w->renderer = r;
    if (sdlg_quiere(flags)) {   /* D288: la GPU 3D si la hay; si no, el software de siempre */
        r->gpu = w->tela ? sdlg_abrir(w->l, w->a) : NULL;
        registrar(r->gpu ? "[%s] renderizador por GPU: destino %dx%d" : "[%s] renderizador por software (sin GPU 3D que lo lleve; %dx%d)",
                  sagui_sdl2_nome, w->l, w->a);
    }
    return r;
}

SDL_Renderer *SDL_CreateSoftwareRenderer(SDL_Surface *s)
{
    if (!s || s->format->BytesPerPixel != 4) { SDL_SetError("el renderer de software necesita 32 bits"); return NULL; }
    SDL_Renderer *r = calloc(1, sizeof *r);
    if (!r) return NULL;
    r->alvo_sup = s;
    r->ca = 255;
    return r;
}

SDL_Renderer *SDL_GetRenderer(SDL_Window *w) { return w ? w->renderer : NULL; }

void SDL_DestroyRenderer(SDL_Renderer *r)
{
    if (!r) return;
    if (r->w && r->w->renderer == r) r->w->renderer = NULL;
    if (r->gpu) { sdlg_fechar(r->gpu, NULL, 0); r->gpu = NULL; }
    free(r);
}

int SDL_GetRendererInfo(SDL_Renderer *r, SDL_RendererInfo *info)
{
    if (!r || !info) return -1;
    *info = r->gpu ? info_renderer_gpu : info_renderer;
    return 0;
}

/* El lienzo donde dibuja el renderer ahora: la textura destino, la
 * superficie (renderer de software) o el de la ventana. */
static Uint32 *lienzo_cru(SDL_Renderer *r, int *l, int *a, int *pitch_px)
{
    if (r->alvo) { *l = r->alvo->l; *a = r->alvo->a; *pitch_px = r->alvo->pitch / 4; return (Uint32 *)r->alvo->px; }
    if (r->alvo_sup) { *l = r->alvo_sup->w; *a = r->alvo_sup->h; *pitch_px = r->alvo_sup->pitch / 4; return r->alvo_sup->pixels; }
    if (r->w) {
        if (r->limpar_pend) {   /* el borrado que se aplazó, antes de dibujar encima */
            r->limpar_pend = 0;
            preencher32(r->w->tela, r->limpar_cor, (size_t)r->w->l * (size_t)r->w->a);
        }
        *l = r->w->l; *a = r->w->a; *pitch_px = r->w->l;
        return r->w->tela;
    }
    return NULL;
}

/* Para escribir en el lienzo de la ventana con la CPU: si lo llevaba la GPU, se le deja
 * (si no, lo dibujado luego en la GPU pisaria lo de la CPU). */
static Uint32 *lienzo_de(SDL_Renderer *r, int *l, int *a, int *pitch_px)
{
    if (r->gpu && !r->alvo && !r->alvo_sup) gpu_cair(r, 1, "dibujo por software en el lienzo");
    return lienzo_cru(r, l, a, pitch_px);
}

/* Para leer el lienzo (ReadPixels): trae antes el cuadro de la GPU. */
static Uint32 *lienzo_ler(SDL_Renderer *r, int *l, int *a, int *pitch_px)
{
    sdlgpu_t *g = gpu_de(r);
    if (g) { int rc = sdlg_ler(g, r->w->tela); if (rc) gpu_erro(r, rc); }
    return lienzo_cru(r, l, a, pitch_px);
}

int SDL_GetRendererOutputSize(SDL_Renderer *r, int *l, int *a)
{
    if (!r) return -1;
    int ll = 0, aa = 0, p;
    lienzo_cru(r, &ll, &aa, &p);
    if (l) *l = ll;
    if (a) *a = aa;
    return 0;
}

/* El tamaño lógico se aplica al copiar: las coordenadas del programa se
 * escalan al lienzo (el lienzo ya es la ventana lógica). */
int SDL_RenderSetLogicalSize(SDL_Renderer *r, int l, int a)
{
    if (!r) return -1;
    r->log_l = l;
    r->log_a = a;
    return 0;
}
void SDL_RenderGetLogicalSize(SDL_Renderer *r, int *l, int *a)
{
    if (l) *l = r ? r->log_l : 0;
    if (a) *a = r ? r->log_a : 0;
}
int SDL_RenderSetIntegerScale(SDL_Renderer *r, SDL_bool s) { (void)r; (void)s; return 0; }
int SDL_RenderSetViewport(SDL_Renderer *r, const SDL_Rect *rect) { (void)r; (void)rect; return 0; }
int SDL_SetRenderDrawBlendMode(SDL_Renderer *r, SDL_BlendMode m) { if (r) r->modo_des = m; return 0; }

int SDL_SetRenderDrawColor(SDL_Renderer *r, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca)
{
    if (!r) return -1;
    r->cr = cr; r->cg = cg; r->cb = cb; r->ca = ca;
    return 0;
}

/* Un rectángulo de coordenadas del programa en el lienzo (con el tamaño lógico). */
static SDL_Rect para_lienzo(SDL_Renderer *r, const SDL_Rect *rect, int l, int a)
{
    /* Con una textura como destino el tamaño lógico no cuenta (como en la SDL real): sin rectángulo, la textura entera (D383). */
    SDL_Rect d = rect ? *rect : (SDL_Rect){ 0, 0, (r->log_l && !r->alvo) ? r->log_l : l, (r->log_a && !r->alvo) ? r->log_a : a };
    if (r->log_l > 0 && r->log_a > 0 && !r->alvo) {
        d.x = (int)((long long)d.x * l / r->log_l);
        d.w = (int)((long long)d.w * l / r->log_l);
        d.y = (int)((long long)d.y * a / r->log_a);
        d.h = (int)((long long)d.h * a / r->log_a);
    }
    return d;
}

/* D436: el recorte (SDL_RenderSetClipRect) en pixeles del lienzo de l x a, ya dentro de el. */
static SDL_Rect recorte_lienzo(SDL_Renderer *r, int l, int a)
{
    SDL_Rect todo = { 0, 0, l, a }, c;
    if (!r->recorte) return todo;
    SDL_Rect q = para_lienzo(r, &r->rec, l, a);
    if (!SDL_IntersectRect(&q, &todo, &c)) c = (SDL_Rect){ 0, 0, 0, 0 };
    return c;
}

int SDL_RenderSetClipRect(SDL_Renderer *r, const SDL_Rect *rect)
{
    if (!r) return -1;
    r->recorte = rect != NULL;
    if (rect) r->rec = *rect;
    return 0;
}
void SDL_RenderGetClipRect(SDL_Renderer *r, SDL_Rect *rect)
{
    if (!rect) return;
    if (r && r->recorte) *rect = r->rec;
    else *rect = (SDL_Rect){ 0, 0, 0, 0 };
}
SDL_bool SDL_RenderIsClipEnabled(SDL_Renderer *r) { return r && r->recorte ? SDL_TRUE : SDL_FALSE; }

int SDL_RenderFillRect(SDL_Renderer *r, const SDL_Rect *rect)
{
    if (!r) return -1;
    SDL_BlendMode modo = r->modo_des;
    if (modo == SDL_BLENDMODE_BLEND && r->ca == 255) modo = SDL_BLENDMODE_NONE;
    sdlgpu_t *g = gpu_de(r);
    if (g) {
        SDL_Rect d = para_lienzo(r, rect, g->l, g->a);
        if (d.w <= 0 || d.h <= 0) return 0;
        int mg = modo_gpu(modo);
        int rc = mg < 0 ? 1 : sdlg_rect(g, d.x, d.y, d.w, d.h, r->cr, r->cg, r->cb, r->ca, mg);
        if (!rc) return 0;
        gpu_erro(r, rc);
    }
    int l, a, p;
    Uint32 *t = lienzo_de(r, &l, &a, &p);
    if (!t) return -1;
    SDL_Rect d = para_lienzo(r, rect, l, a), cl = recorte_lienzo(r, l, a), c;
    if (!SDL_IntersectRect(&d, &cl, &c)) return 0;
    Uint32 cor = 0xFF000000u | ((Uint32)r->cr << 16) | ((Uint32)r->cg << 8) | r->cb;
    if (modo == SDL_BLENDMODE_NONE) {
        for (int y = c.y; y < c.y + c.h; y++) preencher32(t + (size_t)y * p + c.x, cor, (size_t)c.w);
        return 0;
    }
    Uint32 o = ((Uint32)r->ca << 24) | (cor & 0xFFFFFFu);
    int opaco = !r->alvo;   /* el lienzo de la ventana no tiene alfa */
    for (int y = c.y; y < c.y + c.h; y++)
        for (int x = c.x; x < c.x + c.w; x++) {
            Uint32 v = mezcla_modo(o, t[(size_t)y * p + x], modo);
            t[(size_t)y * p + x] = opaco ? v | 0xFF000000u : v;
        }
    return 0;
}
int SDL_RenderClear(SDL_Renderer *r)
{
    if (!r) return -1;
    MEDIR_INI();
    sdlgpu_t *g = gpu_de(r);
    if (g) {
        int rc = sdlg_limpar(g, r->cr, r->cg, r->cb);
        if (!rc) { MEDIR_FIM(ciclos_escala); return 0; }
        gpu_erro(r, rc);
    }
    if (!r->alvo && !r->alvo_sup && r->w) {
        /* D247: sobre la ventana, el borrado espera: casi siempre el RenderCopy siguiente cubre
         * todo el lienzo y pintarlo antes es un relleno de megabytes que no se ve. */
        r->limpar_pend = 1;
        r->limpar_cor = 0xFF000000u | ((Uint32)r->cr << 16) | ((Uint32)r->cg << 8) | r->cb;
        MEDIR_FIM(ciclos_escala);
        return 0;
    }
    int ll = r->log_l, la = r->log_a, rc = r->recorte;
    SDL_BlendMode md = r->modo_des;
    r->log_l = r->log_a = 0;   /* todo el lienzo, sin tamaño lógico ni recorte, y sin mezcla (como la SDL) */
    r->recorte = 0; r->modo_des = SDL_BLENDMODE_NONE;
    int v = SDL_RenderFillRect(r, NULL);
    r->log_l = ll; r->log_a = la; r->recorte = rc; r->modo_des = md;
    MEDIR_FIM(ciclos_escala);
    return v;
}
int SDL_RenderDrawPoint(SDL_Renderer *r, int x, int y)
{
    SDL_Rect q = { x, y, 1, 1 };
    return SDL_RenderFillRect(r, &q);
}
int SDL_RenderDrawLine(SDL_Renderer *r, int x1, int y1, int x2, int y2)
{
    if (y1 == y2 || x1 == x2) {   /* horizontal o vertical: un solo rectangulo */
        SDL_Rect q = { x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2, abs(x2 - x1) + 1, abs(y2 - y1) + 1 };
        return SDL_RenderFillRect(r, &q);
    }
    int dx = abs(x2 - x1), dy = -abs(y2 - y1), sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, e = dx + dy;
    for (;;) {
        SDL_RenderDrawPoint(r, x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x1 += sx; }
        if (e2 <= dx) { e += dx; y1 += sy; }
    }
    return 0;
}
int SDL_RenderDrawRect(SDL_Renderer *r, const SDL_Rect *q)
{
    if (!q) return 0;
    SDL_RenderDrawLine(r, q->x, q->y, q->x + q->w - 1, q->y);
    SDL_RenderDrawLine(r, q->x, q->y + q->h - 1, q->x + q->w - 1, q->y + q->h - 1);
    SDL_RenderDrawLine(r, q->x, q->y, q->x, q->y + q->h - 1);
    SDL_RenderDrawLine(r, q->x + q->w - 1, q->y, q->x + q->w - 1, q->y + q->h - 1);
    return 0;
}

SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 formato, int acesso, int l, int a)
{
    if (l <= 0 || a <= 0) { SDL_SetError("textura de %dx%d", l, a); return NULL; }
    if (SDL_BYTESPERPIXEL(formato) != 4) formato = SDL_PIXELFORMAT_ARGB8888;   /* se guarda en 32 bits */
    SDL_Texture *t = calloc(1, sizeof *t);
    if (!t) { SDL_SetError("sin memoria"); return NULL; }
    t->px = calloc((size_t)l * (size_t)a, 4);
    if (!t->px) { free(t); SDL_SetError("sin memoria para una textura de %dx%d", l, a); return NULL; }
    t->formato = formato;
    t->acesso = acesso;
    t->l = l;
    t->a = a;
    t->pitch = l * 4;
    t->r = r;
    t->mod = 0xFFFFFFFFu;
    t->gpu_vaga = -1;
    t->gpu_sujo = 1;
    return t;
}

SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *r, SDL_Surface *s)
{
    if (!s) return NULL;
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, s->w, s->h);
    if (!t) return NULL;
    SDL_Surface *d = SDL_CreateRGBSurfaceWithFormatFrom(t->px, t->l, t->a, 32, t->pitch, SDL_PIXELFORMAT_ARGB8888);
    if (d) {
        SDL_UpperBlit(s, NULL, d, NULL);
        SDL_FreeSurface(d);
    }
    return t;
}

void SDL_DestroyTexture(SDL_Texture *t)
{
    if (!t) return;
    if (t->r && t->r->alvo == t) t->r->alvo = NULL;
    if (t->r && t->r->gpu && t->gpu_vaga >= 0) {   /* soltar su hueco en la GPU */
        sdlgpu_t *g = t->r->gpu;
        if (g->vaga[t->gpu_vaga] == t) {
            sdlg_adiado(g);   /* D432: si era la copia aplazada, al destino antes de soltarla */
            sdlg_despejar(g);
            g->vaga[t->gpu_vaga] = NULL;
            sdlg_chamar(GPU3D_OP_TEXTURA, (unsigned)t->gpu_vaga, 0, 0, 0);
        }
    }
    free(t->px);
    free(t);
}

int SDL_QueryTexture(SDL_Texture *t, Uint32 *f, int *acesso, int *l, int *a)
{
    if (!t) return -1;
    if (f) *f = t->formato;
    if (acesso) *acesso = t->acesso;
    if (l) *l = t->l;
    if (a) *a = t->a;
    return 0;
}

static int actualizar_textura(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch);
int SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch)
{
    MEDIR_INI();
    int v = actualizar_textura(t, rect, pixels, pitch);
    MEDIR_FIM(ciclos_escala);
    return v;
}
static int actualizar_textura(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch)
{
    if (!t || !pixels) return -1;
    t->gpu_sujo = 1;
    SDL_Rect r = rect ? *rect : (SDL_Rect){ 0, 0, t->l, t->a };
    SDL_Rect todo = { 0, 0, t->l, t->a }, c;
    if (!SDL_IntersectRect(&r, &todo, &c)) return 0;
    for (int y = 0; y < c.h; y++)
        memcpy(t->px + (size_t)(c.y + y) * t->pitch + (size_t)c.x * 4,
               (const Uint8 *)pixels + (size_t)(y + (c.y - r.y)) * pitch + (size_t)(c.x - r.x) * 4, (size_t)c.w * 4);
    return 0;
}

int SDL_LockTexture(SDL_Texture *t, const SDL_Rect *rect, void **pixels, int *pitch)
{
    if (!t) return -1;
    t->gpu_sujo = 1;
    int x = rect ? rect->x : 0, y = rect ? rect->y : 0;
    if (pixels) *pixels = t->px + (size_t)y * t->pitch + (size_t)x * 4;
    if (pitch) *pitch = t->pitch;
    return 0;
}
void SDL_UnlockTexture(SDL_Texture *t) { if (t) t->gpu_sujo = 1; }
int SDL_SetTextureBlendMode(SDL_Texture *t, SDL_BlendMode m) { if (t) t->modo = m; return 0; }
int SDL_SetTextureColorMod(SDL_Texture *t, Uint8 r, Uint8 g, Uint8 b)
{
    if (!t) return -1;
    t->mod = (t->mod & 0xFF000000u) | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
    return 0;
}
int SDL_SetTextureAlphaMod(SDL_Texture *t, Uint8 a)
{
    if (!t) return -1;
    t->mod = (t->mod & 0x00FFFFFFu) | ((Uint32)a << 24);
    return 0;
}

/* t->mod aplicado a un pixel ARGB (cada canal por mod/255). */
static inline Uint32 modular_px(Uint32 v, Uint32 m)
{
    Uint32 o = 0;
    for (int s = 0; s < 32; s += 8)
        o |= (((((v >> s) & 255u) * ((m >> s) & 255u)) + 127u) / 255u) << s;
    return o;
}

/* Seno y coseno de un angulo en grados (x87: no hay libm en todos los programas). */
static void sen_cos(double grados, double *sn, double *cs)
{
    double rad = grados * 0.017453292519943295, s, c;
    __asm__("fsin" : "=t"(s) : "0"(rad));
    __asm__("fcos" : "=t"(c) : "0"(rad));
    *sn = s; *cs = c;
}

/* Un pixel de textura sobre uno del lienzo (modulacion y mezcla de la textura; D436: todos los modos). */
static inline Uint32 pixel_copia(Uint32 v, Uint32 f, const SDL_Texture *t)
{
    if (t->mod != 0xFFFFFFFFu) v = modular_px(v, t->mod);
    if (t->modo == SDL_BLENDMODE_NONE) return v;
    if (t->modo == SDL_BLENDMODE_BLEND) {
        unsigned al = v >> 24;
        if (al == 0) return f;
        if (al == 255) return v;
    }
    return mezcla_modo(v, f, t->modo) | 0xFF000000u;
}

/* Copia con giro (SDL_RenderCopyEx con angulo): para cada pixel del destino, el de la textura
 * que le toca por la rotacion inversa (vecino mas proximo); el giro es en el sentido de las
 * agujas del reloj alrededor de «centro» (por defecto el centro del destino). */
static void copia_girada(Uint32 *dst, SDL_Rect cl, int p, const SDL_Texture *t, SDL_Rect o, SDL_Rect d,
                         double angulo, double cx, double cy, SDL_RendererFlip espelho)
{
    int l = cl.x + cl.w, a = cl.y + cl.h;
    double sn, cs;
    sen_cos(angulo, &sn, &cs);
    /* La caja del destino girado, recortada al lienzo. */
    double xs[4] = { d.x, d.x + d.w, d.x + d.w, d.x }, ys[4] = { d.y, d.y, d.y + d.h, d.y + d.h };
    double mx0 = 1e9, my0 = 1e9, mx1 = -1e9, my1 = -1e9;
    for (int i = 0; i < 4; i++) {
        double dx = xs[i] - cx, dy = ys[i] - cy;
        double X = cx + dx * cs - dy * sn, Y = cy + dx * sn + dy * cs;
        if (X < mx0) mx0 = X;
        if (X > mx1) mx1 = X;
        if (Y < my0) my0 = Y;
        if (Y > my1) my1 = Y;
    }
    int x0 = (int)mx0 - 1, y0 = (int)my0 - 1, x1 = (int)mx1 + 2, y1 = (int)my1 + 2;
    if (x0 < cl.x) x0 = cl.x;
    if (y0 < cl.y) y0 = cl.y;
    if (x1 > l) x1 = l;
    if (y1 > a) y1 = a;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            double px = cx + dx * cs + dy * sn - d.x, py = cy - dx * sn + dy * cs - d.y;
            if (px < 0 || py < 0 || px >= d.w || py >= d.h) continue;
            int ix = (int)(px * o.w / d.w), iy = (int)(py * o.h / d.h);
            if (espelho & SDL_FLIP_HORIZONTAL) ix = o.w - 1 - ix;
            if (espelho & SDL_FLIP_VERTICAL) iy = o.h - 1 - iy;
            int sx = o.x + ix, sy = o.y + iy;
            if (sx < 0 || sx >= t->l || sy < 0 || sy >= t->a) continue;
            Uint32 *q = dst + (size_t)y * p + x;
            *q = pixel_copia(((const Uint32 *)(t->px + (size_t)sy * t->pitch))[sx], *q, t);
        }
}

int SDL_RenderCopyEx(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *ro, const SDL_Rect *rd,
                     double angulo, const SDL_Point *centro, SDL_RendererFlip espelho)
{
    if (!r || !t) return -1;
    sdlgpu_t *g = gpu_de(r);
    if (g) {
        SDL_Rect o = ro ? *ro : (SDL_Rect){ 0, 0, t->l, t->a };
        SDL_Rect d = para_lienzo(r, rd, g->l, g->a);
        if (o.w <= 0 || o.h <= 0 || d.w <= 0 || d.h <= 0) return 0;
        int v = sdlg_textura(g, t);
        if (v >= 0) {
            float u0 = (float)o.x / t->l, u1 = (float)(o.x + o.w) / t->l;
            float v0 = (float)o.y / t->a, v1 = (float)(o.y + o.h) / t->a, tmp;
            if (espelho & SDL_FLIP_HORIZONTAL) { tmp = u0; u0 = u1; u1 = tmp; }
            if (espelho & SDL_FLIP_VERTICAL) { tmp = v0; v0 = v1; v1 = tmp; }
            double cx = d.x + (centro ? centro->x * (double)d.w / (rd ? rd->w : d.w) : d.w / 2.0);
            double cy = d.y + (centro ? centro->y * (double)d.h / (rd ? rd->h : d.h) : d.h / 2.0);
            double sn = 0, cs = 1;
            if (angulo != 0.0) sen_cos(angulo, &sn, &cs);
            sdlg_pt_t p[4] = { { (float)d.x, (float)d.y, u0, v0, t->mod }, { (float)(d.x + d.w), (float)d.y, u1, v0, t->mod },
                               { (float)(d.x + d.w), (float)(d.y + d.h), u1, v1, t->mod }, { (float)d.x, (float)(d.y + d.h), u0, v1, t->mod } };
            if (angulo != 0.0)
                for (int i = 0; i < 4; i++) {
                    double dx = p[i].x - cx, dy = p[i].y - cy;
                    p[i].x = (float)(cx + dx * cs - dy * sn);
                    p[i].y = (float)(cy + dx * sn + dy * cs);
                }
            /* D432: una copia opaca, sin giro ni espejo, que cubre todo el lienzo, a pantalla completa:
             * se aplaza; si nada se dibuja encima, la GPU la llevara directamente a la pantalla. */
            if (g->pres && r->w->em_cheia && angulo == 0.0 && !espelho && t->modo == SDL_BLENDMODE_NONE && !r->recorte &&
                t->mod == 0xFFFFFFFFu && d.x == 0 && d.y == 0 && d.w == g->l && d.h == g->a &&
                o.x >= 0 && o.y >= 0 && o.x + o.w <= t->l && o.y + o.h <= t->a) {
                sdlg_adiar(g, t, o.x, o.y, o.w, o.h, p);
                return 0;
            }
            int mg = modo_gpu(t->modo);
            int rc = mg < 0 ? 1 : sdlg_poligono(g, v, mg, 0, p, 4);
            if (!rc) return 0;
            gpu_erro(r, rc);
        } else {
            gpu_erro(r, v == -1 ? -1 : 1);
        }
    }
    if (r->limpar_pend && !r->alvo && !r->alvo_sup && r->w && t->modo == SDL_BLENDMODE_NONE && !r->recorte) {
        /* ¿La copia cubre todo el lienzo sin dejar huecos? Entonces el borrado aplazado sobra. */
        SDL_Rect dd = para_lienzo(r, rd, r->w->l, r->w->a);
        SDL_Rect oo = ro ? *ro : (SDL_Rect){ 0, 0, t->l, t->a };
        if (dd.x <= 0 && dd.y <= 0 && dd.x + dd.w >= r->w->l && dd.y + dd.h >= r->w->a &&
            oo.w > 0 && oo.h > 0 && oo.x >= 0 && oo.y >= 0 && oo.x + oo.w <= t->l && oo.y + oo.h <= t->a)
            r->limpar_pend = 0;
    }
    int l, a, p;
    Uint32 *dst = lienzo_de(r, &l, &a, &p);
    if (!dst) return -1;
    SDL_Rect o = ro ? *ro : (SDL_Rect){ 0, 0, t->l, t->a };
    SDL_Rect d = para_lienzo(r, rd, l, a);
    if (o.w <= 0 || o.h <= 0 || d.w <= 0 || d.h <= 0) return 0;
    int mistura = (t->modo != SDL_BLENDMODE_NONE);
    int modular = t->mod != 0xFFFFFFFFu;
    SDL_Rect cl = recorte_lienzo(r, l, a);   /* D436 */
    if (cl.w <= 0 || cl.h <= 0) return 0;
    if (angulo != 0.0) {
        SDL_Rect dd = para_lienzo(r, rd, l, a);
        double cx = dd.x + (centro ? centro->x * (double)dd.w / (rd ? rd->w : dd.w) : dd.w / 2.0);
        double cy = dd.y + (centro ? centro->y * (double)dd.h / (rd ? rd->h : dd.h) : dd.h / 2.0);
        copia_girada(dst, cl, p, t, o, d, angulo, cx, cy, espelho);
        return 0;
    }
    /* El caso de los juegos: la textura entera, del mismo tamaño, sin espejo. */
    if (!mistura && !modular && !espelho && !r->recorte && o.w == d.w && o.h == d.h && o.x == 0 && o.y == 0 && d.x == 0 && d.y == 0 &&
        d.w <= l && d.h <= a && o.w <= t->l && o.h <= t->a) {
        for (int y = 0; y < d.h; y++)
            memcpy(dst + (size_t)y * p, t->px + (size_t)y * t->pitch, (size_t)d.w * 4);
        return 0;
    }
    /* Escalado por vecino más próximo. La columna de origen de cada columna de destino se calcula
     * una vez (no por píxel y fila: la división de 64 bits es lo más caro, y esto corre en cada
     * cuadro de un emulador a 3x), y de una fila a la siguiente que cae en la misma fila de origen
     * se copia la ya hecha (sin mezcla). */
    static int *colmapa = NULL;
    static int colmapa_cap = 0;
    if (d.w > colmapa_cap) {
        int *n = realloc(colmapa, (size_t)d.w * sizeof *n);
        if (!n) return SDL_SetError("sin memoria");
        colmapa = n;
        colmapa_cap = d.w;
    }
    int x0 = d.x < cl.x ? cl.x - d.x : 0, x1 = d.x + d.w > cl.x + cl.w ? cl.x + cl.w - d.x : d.w;   /* columnas de destino dentro del recorte */
    for (int x = 0; x < d.w; x++) {
        int sx = o.x + (int)((long long)(espelho & SDL_FLIP_HORIZONTAL ? d.w - 1 - x : x) * o.w / d.w);
        colmapa[x] = sx < 0 || sx >= t->l ? -1 : sx;
    }
    int ant_sy = -1, ant_yy = -1;
    for (int y = 0; y < d.h; y++) {
        int yy = d.y + y;
        if (yy < cl.y || yy >= cl.y + cl.h) continue;
        int sy = o.y + (int)((long long)(espelho & SDL_FLIP_VERTICAL ? d.h - 1 - y : y) * o.h / d.h);
        if (sy < 0 || sy >= t->a) continue;
        Uint32 *ld = dst + (size_t)yy * p;
        if (!mistura && sy == ant_sy && ant_yy >= 0 && x1 > x0) {
            memcpy(ld + d.x + x0, dst + (size_t)ant_yy * p + d.x + x0, (size_t)(x1 - x0) * 4);
            ant_yy = yy;
            continue;
        }
        const Uint32 *lo = (const Uint32 *)(t->px + (size_t)sy * t->pitch);
        for (int x = x0; x < x1; x++) {
            int sx = colmapa[x];
            if (sx < 0) continue;
            int xx = d.x + x;
            Uint32 v = lo[sx];
            if (mistura || modular) v = pixel_copia(v, ld[xx], t);
            ld[xx] = v;
        }
        ant_sy = sy;
        ant_yy = yy;
    }
    return 0;
}
int SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *ro, const SDL_Rect *rd)
{
    MEDIR_INI();
    int v = SDL_RenderCopyEx(r, t, ro, rd, 0.0, NULL, SDL_FLIP_NONE);
    MEDIR_FIM(ciclos_escala);
    return v;
}

/* ------------------------------------------------------------------ D436: SDL_RenderGeometry */

int SDL_GetTextureColorMod(SDL_Texture *t, Uint8 *r, Uint8 *g, Uint8 *b)
{
    if (!t) return -1;
    if (r) *r = (Uint8)(t->mod >> 16);
    if (g) *g = (Uint8)(t->mod >> 8);
    if (b) *b = (Uint8)t->mod;
    return 0;
}
int SDL_GetTextureAlphaMod(SDL_Texture *t, Uint8 *a) { if (!t) return -1; if (a) *a = (Uint8)(t->mod >> 24); return 0; }
int SDL_GetTextureBlendMode(SDL_Texture *t, SDL_BlendMode *m) { if (!t) return -1; if (m) *m = t->modo; return 0; }

/* Lado potencia de 2 (la GPU solo repite esas texturas). */
static int pot2(int v) { return v > 0 && !(v & (v - 1)); }

/* Un triángulo por software: centros de píxel, vecino más próximo, color interpolado que multiplica, la mezcla del modo
 * m y el recorte cl. rep: las coordenadas de textura se repiten (si no, se sujetan al borde). */
static void triangulo_sw(Uint32 *dst, int p, SDL_Rect cl, const SDL_Texture *t, const SDL_Vertex *v0, const SDL_Vertex *v1,
                         const SDL_Vertex *v2, SDL_BlendMode m, int rep, int opaco)
{
    float x0 = v0->position.x, y0 = v0->position.y, x1 = v1->position.x, y1 = v1->position.y, x2 = v2->position.x, y2 = v2->position.y;
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (area == 0.0f || area != area) return;
    float mnx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2), mxx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    float mny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), mxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    int ix0 = (int)(mnx - 1.0f), ix1 = (int)(mxx + 1.0f), iy0 = (int)(mny - 1.0f), iy1 = (int)(mxy + 1.0f);
    if (ix0 < cl.x) ix0 = cl.x;
    if (iy0 < cl.y) iy0 = cl.y;
    if (ix1 > cl.x + cl.w - 1) ix1 = cl.x + cl.w - 1;
    if (iy1 > cl.y + cl.h - 1) iy1 = cl.y + cl.h - 1;
    float inv = 1.0f / area;
    const SDL_Color *c0 = &v0->color, *c1 = &v1->color, *c2 = &v2->color;
    for (int y = iy0; y <= iy1; y++) {
        float py = (float)y + 0.5f;
        for (int x = ix0; x <= ix1; x++) {
            float px = (float)x + 0.5f;
            /* pesos baricentricos (con la regla de «dentro si >= 0» y el signo del área) */
            float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * inv;
            float w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * inv;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            Uint32 o;
            unsigned cr = (unsigned)(c0->r * w0 + c1->r * w1 + c2->r * w2 + 0.5f), cg = (unsigned)(c0->g * w0 + c1->g * w1 + c2->g * w2 + 0.5f);
            unsigned cb = (unsigned)(c0->b * w0 + c1->b * w1 + c2->b * w2 + 0.5f), ca = (unsigned)(c0->a * w0 + c1->a * w1 + c2->a * w2 + 0.5f);
            Uint32 cor = ((ca > 255 ? 255u : ca) << 24) | ((cr > 255 ? 255u : cr) << 16) | ((cg > 255 ? 255u : cg) << 8) | (cb > 255 ? 255u : cb);
            if (t) {
                float u = v0->tex_coord.x * w0 + v1->tex_coord.x * w1 + v2->tex_coord.x * w2;
                float vv = v0->tex_coord.y * w0 + v1->tex_coord.y * w1 + v2->tex_coord.y * w2;
                int tx = (int)(u * (float)t->l + 65536.0f) - 65536, ty = (int)(vv * (float)t->a + 65536.0f) - 65536;
                if (rep) { tx &= t->l - 1; ty &= t->a - 1; }
                else { tx = tx < 0 ? 0 : tx >= t->l ? t->l - 1 : tx; ty = ty < 0 ? 0 : ty >= t->a ? t->a - 1 : ty; }
                o = modular_px(((const Uint32 *)(t->px + (size_t)ty * t->pitch))[tx], cor);
            } else o = cor;
            Uint32 *q = dst + (size_t)y * p + x;
            Uint32 n = mezcla_modo(o, *q, m);
            *q = opaco ? n | 0xFF000000u : n;
        }
    }
}

int SDL_RenderGeometry(SDL_Renderer *r, SDL_Texture *t, const SDL_Vertex *vs, int nv, const int *ind, int ni)
{
    if (!r || !vs || nv < 3) return r && vs ? 0 : -1;
    int n = ind ? ni : nv;
    if (n % 3) return SDL_SetError("SDL_RenderGeometry: %d vertices no son triangulos", n);
    for (int i = 0; ind && i < ni; i++) if (ind[i] < 0 || ind[i] >= nv) return SDL_SetError("indice fuera de rango");
    SDL_BlendMode modo = t ? t->modo : r->modo_des;
    /* ¿Repetir la textura? Si alguna coordenada sale de [0, 1] y los lados son potencia de 2 (D436: lo usa AoEN para el
     * terreno); si no son potencia de 2, se sujeta al borde, como la SDL real. */
    int rep = 0;
    if (t && pot2(t->l) && pot2(t->a))
        for (int i = 0; i < nv; i++)
            if (vs[i].tex_coord.x < 0.0f || vs[i].tex_coord.x > 1.0f || vs[i].tex_coord.y < 0.0f || vs[i].tex_coord.y > 1.0f) { rep = 1; break; }
    float sx = 1.0f, sy = 1.0f;   /* tamaño lógico */
    sdlgpu_t *g = gpu_de(r);
    if (g) {
        int mg = modo_gpu(modo), vaga = -1, rc = 0;
        if (mg < 0) rc = 1;
        else if (t) { vaga = sdlg_textura(g, t); if (vaga < 0) rc = vaga == -1 ? -1 : 1; }
        if (r->log_l > 0 && r->log_a > 0) { sx = (float)g->l / (float)r->log_l; sy = (float)g->a / (float)r->log_a; }
        Uint32 mod = t ? t->mod : 0xFFFFFFFFu;
        if (!rc && (rc = sdlg_adiado(g)) == 0) {
            for (int i = 0; i + 2 < n && !rc; i += 3) {
                sdlg_pt_t q[3];
                for (int k = 0; k < 3; k++) {
                    const SDL_Vertex *v = &vs[ind ? ind[i + k] : i + k];
                    Uint32 c = ((Uint32)v->color.a << 24) | ((Uint32)v->color.r << 16) | ((Uint32)v->color.g << 8) | v->color.b;
                    q[k].x = v->position.x * sx; q[k].y = v->position.y * sy;
                    q[k].u = v->tex_coord.x; q[k].v = v->tex_coord.y;
                    q[k].cor = mod == 0xFFFFFFFFu ? c : modular_px(c, mod);
                    if (q[k].x != q[k].x || q[k].y != q[k].y || q[k].u != q[k].u || q[k].v != q[k].v) q[k].x = q[k].y = q[k].u = q[k].v = 0;
                }
                rc = sdlg_poligono_int(g, vaga, mg, rep, q, 3);
            }
        }
        if (!rc) return 0;
        gpu_erro(r, rc);
        sx = sy = 1.0f;
    }
    int l, a, p;
    Uint32 *dst = lienzo_de(r, &l, &a, &p);
    if (!dst) return -1;
    if (r->log_l > 0 && r->log_a > 0 && !r->alvo) { sx = (float)l / (float)r->log_l; sy = (float)a / (float)r->log_a; }
    SDL_Rect cl = recorte_lienzo(r, l, a);
    if (cl.w <= 0 || cl.h <= 0) return 0;
    if (t && t->mod != 0xFFFFFFFFu) {   /* la modulación de la textura multiplica el color de cada vértice */
        for (int i = 0; i + 2 < n; i += 3) {
            SDL_Vertex q[3];
            for (int k = 0; k < 3; k++) {
                q[k] = vs[ind ? ind[i + k] : i + k];
                Uint32 c = modular_px(((Uint32)q[k].color.a << 24) | ((Uint32)q[k].color.r << 16) | ((Uint32)q[k].color.g << 8) | q[k].color.b, t->mod);
                q[k].color.a = (Uint8)(c >> 24); q[k].color.r = (Uint8)(c >> 16); q[k].color.g = (Uint8)(c >> 8); q[k].color.b = (Uint8)c;
                q[k].position.x *= sx; q[k].position.y *= sy;
            }
            triangulo_sw(dst, p, cl, t, &q[0], &q[1], &q[2], modo, rep, !r->alvo);
        }
        return 0;
    }
    for (int i = 0; i + 2 < n; i += 3) {
        SDL_Vertex q[3];
        for (int k = 0; k < 3; k++) { q[k] = vs[ind ? ind[i + k] : i + k]; q[k].position.x *= sx; q[k].position.y *= sy; }
        triangulo_sw(dst, p, cl, t, &q[0], &q[1], &q[2], modo, rep, !r->alvo);
    }
    return 0;
}

int SDL_SetRenderTarget(SDL_Renderer *r, SDL_Texture *t)
{
    if (!r) return -1;
    r->alvo = t;
    if (t) t->gpu_sujo = 1;   /* se va a dibujar en ella con la CPU */
    return 0;
}

int SDL_RenderReadPixels(SDL_Renderer *r, const SDL_Rect *rect, Uint32 formato, void *pixels, int pitch)
{
    (void)formato;
    if (!r || !pixels) return -1;
    int l, a, p;
    Uint32 *t = lienzo_ler(r, &l, &a, &p);
    if (!t) return -1;
    SDL_Rect q = rect ? *rect : (SDL_Rect){ 0, 0, l, a }, todo = { 0, 0, l, a }, c;
    if (!SDL_IntersectRect(&q, &todo, &c)) return 0;
    for (int y = 0; y < c.h; y++)
        memcpy((Uint8 *)pixels + (size_t)y * pitch, t + (size_t)(c.y + y) * p + c.x, (size_t)c.w * 4);
    return 0;
}

/* Para la prueba h246: sumas FNV-1a de la ventana del sistema y del lienzo lógico. */
static unsigned fnv32(const uint32_t *p, size_t n)
{
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
unsigned sagui_sdl2_hash_janela(void) { return janela && janela->px ? fnv32(janela->px, (size_t)janela->jl * janela->ja) : 0; }
unsigned sagui_sdl2_hash_lienzo(void) { return janela && janela->tela ? fnv32(janela->tela, (size_t)janela->l * janela->a) : 0; }

static int hay_hilos;   /* se creó algún hilo (también el de audio): es_principal() distingue al principal */
static int hay_hilos_programa;   /* el programa creó hilos suyos (SDL_CreateThread); el de audio no cuenta */
static void bombear_audio(void);
static int es_principal(void);   /* D393: el audio solo se mueve en el hilo principal */

/* Cuenta un cuadro y, cada ~3 s, escribe la media de cada fase (en microsegundos). */
static void medir_cuadro(void)
{
    if (medir_estado <= 0) return;
    unsigned long long ahora = tsc_ler();
    unsigned ms = tempo_ms();
    if (!medir_ini_ms) { medir_ini_ms = ms; medir_ini_tsc = ahora; medir_cuadros = 0; ciclos_paleta = ciclos_escala = ciclos_apres = 0; return; }
    medir_cuadros++;
    if ((int)(ms - medir_ini_ms) < 3000 || !medir_cuadros) return;
    double us_ciclo = (double)(ms - medir_ini_ms) * 1000.0 / (double)(ahora - medir_ini_tsc);
    double n = medir_cuadros;
    double total = (double)(ahora - medir_ini_tsc) * us_ciclo / n;
    double pal = ciclos_paleta * us_ciclo / n, esc = ciclos_escala * us_ciclo / n, apr = ciclos_apres * us_ciclo / n;
    double jogo = total - pal - esc - apr;
    registrar("[%s] cuadro: juego %d us, paleta %d us, escalado %d us, presentar %d us, fps %d.%d",
              sagui_sdl2_nome, (int)jogo, (int)pal, (int)esc, (int)apr,
              (int)(1e6 / total), (int)(1e7 / total) % 10);
    medir_ini_ms = ms; medir_ini_tsc = ahora; medir_cuadros = 0;
    ciclos_paleta = ciclos_escala = ciclos_apres = 0;
}

/* D476: limitador de FPS general. SDL_SAGUI_FPS_MAX (pista o variable de entorno; la pista manda): 0 o ausente = ilimitado,
 * si no, cuadros por segundo (los juegos ofrecen 30, 60 o ilimitado). Se llama al final de SDL_RenderPresent y de
 * SDL_GL_SwapWindow y DUERME lo que falte (SDL_Delay: no gasta CPU y sigue bombeando el audio, así que el sonido no se
 * corta). El plazo de cada cuadro se acumula (base + n * 1000 / fps) para que el ritmo medio sea exacto aunque el
 * milisegundo no divida a 1000; si el programa va tarde más de 100 ms, se reinicia la cuenta en vez de correr para alcanzar. */
void sagui_sdl_fps_limitar(void)
{
    static int env_leido, env_fps;
    static unsigned base, n;
    static int fps_ant;
    const char *h = SDL_GetHint(SDL_HINT_SAGUI_FPS_MAX);
    int fps;
    if (h && *h) fps = atoi(h);
    else {
        if (!env_leido) { const char *e = getenv(SDL_HINT_SAGUI_FPS_MAX); env_fps = e && *e ? atoi(e) : 0; env_leido = 1; }
        fps = env_fps;
    }
    if (fps <= 0) { fps_ant = 0; return; }
    if (fps > 1000) fps = 1000;
    unsigned ahora = tempo_ms();
    if (fps != fps_ant || (int)(ahora - (base + n * 1000u / (unsigned)fps)) > 100) { fps_ant = fps; base = ahora; n = 0; return; }
    n++;
    unsigned plazo = base + n * 1000u / (unsigned)fps;
    int falta = (int)(plazo - ahora);
    if (falta > 0) SDL_Delay((Uint32)falta);
    if (n >= 100000u) { base = tempo_ms(); n = 0; }
}

static void presentar_sin_limite(SDL_Renderer *r);
void SDL_RenderPresent(SDL_Renderer *r)
{
    presentar_sin_limite(r);
    if (r && r->w) sagui_sdl_fps_limitar();
}

static void presentar_sin_limite(SDL_Renderer *r)
{
    if (!r || !r->w) return;
    bombear_audio();
    MEDIR_INI();
    sdlgpu_t *g = gpu_de(r);
    if (g && g->pres && r->w->em_cheia) {
        /* D432: a pantalla completa la GPU copia el cuadro a la pantalla y no vuelve a la RAM. */
        SDL_Window *w = r->w;
        int rc = sdlg_apresentar(g, w->id, w->dx, w->dy, w->dl, w->da);
        if (rc == 0) {
            if (!g->excl) {
                g->excl = 1;
                registrar("[%s] presentar por la GPU en la pantalla (D432): lienzo %dx%d -> %dx%d+%d+%d", sagui_sdl2_nome,
                          g->l, g->a, w->dl, w->da, w->dx, w->dy);
            }
            if (!w->primeiro_quadro) {
                w->primeiro_quadro = 1;
                registrar("[%s] primer cuadro dibujado (%dx%d)", sagui_sdl2_nome, w->jl, w->ja);
            }
            MEDIR_FIM(ciclos_apres);
            medir_cuadro();
            return;
        }
        if (rc < 0) { gpu_erro(r, rc); g = NULL; }
    }
    if (g) {   /* D288: el cuadro vuelve de la GPU al lienzo y se presenta como siempre */
        if (g->excl) {
            g->excl = 0;
            r->w->completo = 1;   /* el compositor no tiene los cuadros que fueron directos */
            registrar("[%s] presentar: la pantalla vuelve al compositor (%lu cuadros directos)", sagui_sdl2_nome, g->pres_n);
        }
        int rc = sdlg_ler(g, r->w->tela);
        if (rc) gpu_erro(r, rc);
    }
    if (r->limpar_pend && !r->alvo && !r->alvo_sup) { int l, a, p; lienzo_cru(r, &l, &a, &p); }
    apresentar(r->w);
    MEDIR_FIM(ciclos_apres);
    medir_cuadro();
}

/* D432: los programas que pintan en la superficie de la ventana (SDL_UpdateWindowSurface: Tzar, OpenTTD...)
 * no tienen renderizador. Con SDL_SAGUI_GPU=1 (solo con la variable: es experimental) y la ventana a pantalla
 * completa, el lienzo se sube entero a una textura de la GPU cuando cambia y la GPU lo copia, escalado, a la
 * pantalla (GPU3D_OP_APRESENTAR): el escalado y las copias del compositor dejan de hacerlos la CPU. Si el
 * kernel dice «ahora no» (un aviso, Alt+Tab...), ese cuadro va por el camino de siempre. 1: presentado. */
static void superficie_gpu_fechar(SDL_Window *w)
{
    if (!w || !w->sup_gpu) return;
    sdlg_chamar(GPU3D_OP_FECHAR, 0, 0, 0, 0);
    free(w->sup_gpu);
    w->sup_gpu = NULL;
    w->completo = 1;
}

static int superficie_gpu(SDL_Window *w)
{
    if (w->renderer || !w->tela || !w->px) return 0;
    if (!w->em_cheia) { superficie_gpu_fechar(w); return 0; }   /* en ventana, el motor queda libre para otros */
    if (!w->sup_gpu) {
        const char *e = getenv("SDL_SAGUI_GPU");
        if (!e || e[0] != '1') { w->sup_nao = 1; return 0; }
        sdlgpu_t *g = sdlg_abrir(8, 8);   /* el destino del 3D no se usa: solo una textura con el lienzo */
        if (g && !g->pres) { sdlg_chamar(GPU3D_OP_FECHAR, 0, 0, 0, 0); free(g); g = NULL; }
        if (!g) {
            w->sup_nao = 1;
            registrar("[%s] superficie de la ventana por software (no hay GPU 3D que presente en la pantalla)", sagui_sdl2_nome);
            return 0;
        }
        w->sup_gpu = g;
        w->completo = 1;
        registrar("[%s] superficie de la ventana por la GPU (D432): lienzo %dx%d -> %dx%d+%d+%d", sagui_sdl2_nome, w->l, w->a,
                  w->dl, w->da, w->dx, w->dy);
    }
    sdlgpu_t *g = w->sup_gpu;
    /* ¿Cambió algo desde lo último presentado? (la misma comparación que el camino de software) */
    size_t n = (size_t)w->l * (size_t)w->a;
    if (w->ant_n != n || !w->ant) {
        free(w->ant);
        w->ant = malloc(n * 4);
        w->ant_n = w->ant ? n : 0;
        w->completo = 1;
    }
    if (!w->completo && w->ant && g->excl) {
        zona_t cx[MAX_CAIXAS];
        if (achar_mudancas(w, cx) == 0) return 1;   /* la pantalla ya lo tiene */
    } else if (w->ant) memcpy(w->ant, w->tela, n * 4);
    w->completo = 0;
    int rc = sdlg_chamar(GPU3D_OP_TEXTURA, 0, (unsigned)w->l, (unsigned)w->a, w->tela);
    if (rc == 0) {
        gpu3d_apresentar_t a;
        memset(&a, 0, sizeof a);
        a.fonte = 0; a.janela = (unsigned)w->id;
        a.ol = (unsigned short)w->l; a.oa = (unsigned short)w->a;
        a.dx = (unsigned short)w->dx; a.dy = (unsigned short)w->dy; a.dl = (unsigned short)w->dl; a.da = (unsigned short)w->da;
        a.flags = g->pres_flags;
        rc = sdlg_chamar(GPU3D_OP_APRESENTAR, 0, 0, 0, &a);
        if (rc == 0) {
            if (!g->excl) g->excl = 1;
            g->pres_n++;
            if (!w->primeiro_quadro) {
                w->primeiro_quadro = 1;
                registrar("[%s] primer cuadro dibujado (%dx%d)", sagui_sdl2_nome, w->jl, w->ja);
            }
            return 1;
        }
    }
    /* Ahora no (rc > 0) o la GPU no puede con este lienzo o falló: este cuadro, por el camino de siempre. */
    w->completo = 1;
    if (g->excl) g->excl = 0;
    g->pres_nao++;
    if (rc < 0 || g->pres_nao > 600 + 4 * g->pres_n) {
        /* la GPU falló, o casi nunca se puede (el lienzo no cabe en una textura...): se deja */
        registrar("[%s] superficie de la ventana por la GPU: se deja (%s; %lu cuadros directos)", sagui_sdl2_nome,
                  rc < 0 ? "la GPU fallo" : "casi nunca se puede", g->pres_n);
        superficie_gpu_fechar(w);
        w->sup_nao = 1;
    }
    return 0;
}

/* Los cuadros directos y los que no, del renderizador o de la superficie de la ventana (r NULL). */
int sagui_sdl2_gpu_estado(SDL_Renderer *r, unsigned long *directos, unsigned long *por_ram)
{
    sdlgpu_t *g = r ? r->gpu : janela ? janela->sup_gpu : NULL;
    if (directos) *directos = g ? g->pres_n : 0;
    if (por_ram) *por_ram = g ? (r ? g->ler_n : g->pres_nao) : 0;
    return g ? 1 : 0;
}

/* ------------------------------------------------------------------ ratón y cursor */

static int rato_x = 0, rato_y = 0;
static Uint32 rato_botoes = 0;
static int rato_rel_x = 0, rato_rel_y = 0;
struct SDL_Cursor { int nada; };
static SDL_Cursor cursor_sistema;
static SDL_Cursor *cursor_atual = &cursor_sistema;

Uint32 SDL_GetMouseState(int *x, int *y)
{
    if (x) *x = rato_x;
    if (y) *y = rato_y;
    return rato_botoes;
}
Uint32 SDL_GetGlobalMouseState(int *x, int *y) { return SDL_GetMouseState(x, y); }
Uint32 SDL_GetRelativeMouseState(int *x, int *y)
{
    if (x) *x = rato_rel_x;
    if (y) *y = rato_rel_y;
    rato_rel_x = rato_rel_y = 0;
    return rato_botoes;
}

static void enfileirar(const SDL_Event *ev);

/* El SaguiOS no mueve el ratón del sistema: se apunta la posición pedida
 * y se encola el movimiento, que es lo que leen los programas (D156). */
void SDL_WarpMouseInWindow(SDL_Window *w, int x, int y)
{
    (void)w;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_MOUSEMOTION;
    ev.motion.windowID = 1;
    ev.motion.state = rato_botoes;
    ev.motion.x = x;
    ev.motion.y = y;
    ev.motion.xrel = x - rato_x;
    ev.motion.yrel = y - rato_y;
    rato_x = x;
    rato_y = y;
    enfileirar(&ev);
}

/* El ratón relativo (capturado) no existe: el programa lo sabe por el -1 y
 * sigue con el absoluto. */
int SDL_SetRelativeMouseMode(SDL_bool ativo) { return ativo ? SDL_SetError("sin ratón relativo") : 0; }
SDL_bool SDL_GetRelativeMouseMode(void) { return SDL_FALSE; }

/* D230: SDL_DISABLE oculta el puntero del sistema sobre la ventana (con el
 * foco; fuera de ella, en los menús y al cerrar, el kernel lo vuelve a
 * mostrar). Como en la SDL 2, devuelve el estado que queda. Los cursores
 * propios (SDL_CreateColorCursor) se aceptan y no se dibujan: el programa
 * que quiere el suyo lo pinta él y oculta el del sistema. */
int SDL_ShowCursor(int estado)
{
    if ((estado == SDL_ENABLE || estado == SDL_DISABLE) && estado != cursor_visivel) {
        cursor_visivel = estado;
        if (janela) cursor_janela(janela->id, cursor_visivel != 0);
        registrar("[%s] puntero del sistema %s", sagui_sdl2_nome, cursor_visivel ? "visible" : "oculto");
    }
    return cursor_visivel;
}
SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *s, int hx, int hy)
{
    (void)s; (void)hx; (void)hy;
    return calloc(1, sizeof(SDL_Cursor));
}
SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id) { (void)id; return calloc(1, sizeof(SDL_Cursor)); }
SDL_Cursor *SDL_CreateCursor(const Uint8 *d, const Uint8 *m, int l, int a, int hx, int hy)
{
    (void)d; (void)m; (void)l; (void)a; (void)hx; (void)hy;
    return calloc(1, sizeof(SDL_Cursor));
}
void SDL_SetCursor(SDL_Cursor *c) { if (c) cursor_atual = c; }
SDL_Cursor *SDL_GetCursor(void) { return cursor_atual; }
void SDL_FreeCursor(SDL_Cursor *c)
{
    if (!c || c == &cursor_sistema) return;
    if (cursor_atual == c) cursor_atual = &cursor_sistema;
    free(c);
}
int SDL_CaptureMouse(SDL_bool s) { (void)s; return 0; }

/* ------------------------------------------------------------------ teclado */

/* Cada tecla que se sigue: su código de SDL 2, su código de rastreo del
 * conjunto 1 (0x80 | código para las de prefijo E0, como el mapa del
 * kernel) y su carácter sin y con Mayús en el teclado de EE. UU. */
typedef struct {
    Uint16 sdl;
    Uint8 sc;
    char normal, maius;
} tecla_t;

static const tecla_t teclas[] = {
    { SDL_SCANCODE_ESCAPE, 0x01, 27, 27 },
    { SDL_SCANCODE_1, 0x02, '1', '!' }, { SDL_SCANCODE_2, 0x03, '2', '@' }, { SDL_SCANCODE_3, 0x04, '3', '#' },
    { SDL_SCANCODE_4, 0x05, '4', '$' }, { SDL_SCANCODE_5, 0x06, '5', '%' }, { SDL_SCANCODE_6, 0x07, '6', '^' },
    { SDL_SCANCODE_7, 0x08, '7', '&' }, { SDL_SCANCODE_8, 0x09, '8', '*' }, { SDL_SCANCODE_9, 0x0A, '9', '(' },
    { SDL_SCANCODE_0, 0x0B, '0', ')' }, { SDL_SCANCODE_MINUS, 0x0C, '-', '_' }, { SDL_SCANCODE_EQUALS, 0x0D, '=', '+' },
    { SDL_SCANCODE_BACKSPACE, 0x0E, 8, 8 }, { SDL_SCANCODE_TAB, 0x0F, 9, 9 },
    { SDL_SCANCODE_Q, 0x10, 'q', 'Q' }, { SDL_SCANCODE_W, 0x11, 'w', 'W' }, { SDL_SCANCODE_E, 0x12, 'e', 'E' },
    { SDL_SCANCODE_R, 0x13, 'r', 'R' }, { SDL_SCANCODE_T, 0x14, 't', 'T' }, { SDL_SCANCODE_Y, 0x15, 'y', 'Y' },
    { SDL_SCANCODE_U, 0x16, 'u', 'U' }, { SDL_SCANCODE_I, 0x17, 'i', 'I' }, { SDL_SCANCODE_O, 0x18, 'o', 'O' },
    { SDL_SCANCODE_P, 0x19, 'p', 'P' }, { SDL_SCANCODE_LEFTBRACKET, 0x1A, '[', '{' },
    { SDL_SCANCODE_RIGHTBRACKET, 0x1B, ']', '}' }, { SDL_SCANCODE_RETURN, 0x1C, 13, 13 },
    { SDL_SCANCODE_LCTRL, 0x1D, 0, 0 },
    { SDL_SCANCODE_A, 0x1E, 'a', 'A' }, { SDL_SCANCODE_S, 0x1F, 's', 'S' }, { SDL_SCANCODE_D, 0x20, 'd', 'D' },
    { SDL_SCANCODE_F, 0x21, 'f', 'F' }, { SDL_SCANCODE_G, 0x22, 'g', 'G' }, { SDL_SCANCODE_H, 0x23, 'h', 'H' },
    { SDL_SCANCODE_J, 0x24, 'j', 'J' }, { SDL_SCANCODE_K, 0x25, 'k', 'K' }, { SDL_SCANCODE_L, 0x26, 'l', 'L' },
    { SDL_SCANCODE_SEMICOLON, 0x27, ';', ':' }, { SDL_SCANCODE_APOSTROPHE, 0x28, '\'', '"' },
    { SDL_SCANCODE_GRAVE, 0x29, '`', '~' }, { SDL_SCANCODE_LSHIFT, 0x2A, 0, 0 },
    { SDL_SCANCODE_BACKSLASH, 0x2B, '\\', '|' },
    { SDL_SCANCODE_Z, 0x2C, 'z', 'Z' }, { SDL_SCANCODE_X, 0x2D, 'x', 'X' }, { SDL_SCANCODE_C, 0x2E, 'c', 'C' },
    { SDL_SCANCODE_V, 0x2F, 'v', 'V' }, { SDL_SCANCODE_B, 0x30, 'b', 'B' }, { SDL_SCANCODE_N, 0x31, 'n', 'N' },
    { SDL_SCANCODE_M, 0x32, 'm', 'M' }, { SDL_SCANCODE_COMMA, 0x33, ',', '<' }, { SDL_SCANCODE_PERIOD, 0x34, '.', '>' },
    { SDL_SCANCODE_SLASH, 0x35, '/', '?' }, { SDL_SCANCODE_RSHIFT, 0x36, 0, 0 },
    { SDL_SCANCODE_KP_MULTIPLY, 0x37, '*', '*' }, { SDL_SCANCODE_LALT, 0x38, 0, 0 },
    { SDL_SCANCODE_SPACE, 0x39, ' ', ' ' }, { SDL_SCANCODE_CAPSLOCK, 0x3A, 0, 0 },
    { SDL_SCANCODE_F1, 0x3B, 0, 0 }, { SDL_SCANCODE_F2, 0x3C, 0, 0 }, { SDL_SCANCODE_F3, 0x3D, 0, 0 },
    { SDL_SCANCODE_F4, 0x3E, 0, 0 }, { SDL_SCANCODE_F5, 0x3F, 0, 0 }, { SDL_SCANCODE_F6, 0x40, 0, 0 },
    { SDL_SCANCODE_F7, 0x41, 0, 0 }, { SDL_SCANCODE_F8, 0x42, 0, 0 }, { SDL_SCANCODE_F9, 0x43, 0, 0 },
    { SDL_SCANCODE_F10, 0x44, 0, 0 }, { SDL_SCANCODE_NUMLOCKCLEAR, 0x45, 0, 0 },
    { SDL_SCANCODE_SCROLLLOCK, 0x46, 0, 0 },
    { SDL_SCANCODE_KP_7, 0x47, 0, 0 }, { SDL_SCANCODE_KP_8, 0x48, 0, 0 }, { SDL_SCANCODE_KP_9, 0x49, 0, 0 },
    { SDL_SCANCODE_KP_MINUS, 0x4A, '-', '-' }, { SDL_SCANCODE_KP_4, 0x4B, 0, 0 }, { SDL_SCANCODE_KP_5, 0x4C, 0, 0 },
    { SDL_SCANCODE_KP_6, 0x4D, 0, 0 }, { SDL_SCANCODE_KP_PLUS, 0x4E, '+', '+' }, { SDL_SCANCODE_KP_1, 0x4F, 0, 0 },
    { SDL_SCANCODE_KP_2, 0x50, 0, 0 }, { SDL_SCANCODE_KP_3, 0x51, 0, 0 }, { SDL_SCANCODE_KP_0, 0x52, 0, 0 },
    { SDL_SCANCODE_KP_PERIOD, 0x53, 0, 0 }, { SDL_SCANCODE_F11, 0x57, 0, 0 }, { SDL_SCANCODE_F12, 0x58, 0, 0 },
    { SDL_SCANCODE_KP_ENTER, 0x80 | 0x1C, 0, 0 }, { SDL_SCANCODE_RCTRL, 0x80 | 0x1D, 0, 0 },
    { SDL_SCANCODE_KP_DIVIDE, 0x80 | 0x35, '/', '/' }, { SDL_SCANCODE_RALT, 0x80 | 0x38, 0, 0 },
    { SDL_SCANCODE_HOME, 0x80 | 0x47, 0, 0 }, { SDL_SCANCODE_UP, 0x80 | 0x48, 0, 0 },
    { SDL_SCANCODE_PAGEUP, 0x80 | 0x49, 0, 0 }, { SDL_SCANCODE_LEFT, 0x80 | 0x4B, 0, 0 },
    { SDL_SCANCODE_RIGHT, 0x80 | 0x4D, 0, 0 }, { SDL_SCANCODE_END, 0x80 | 0x4F, 0, 0 },
    { SDL_SCANCODE_DOWN, 0x80 | 0x50, 0, 0 }, { SDL_SCANCODE_PAGEDOWN, 0x80 | 0x51, 0, 0 },
    { SDL_SCANCODE_INSERT, 0x80 | 0x52, 0, 0 }, { SDL_SCANCODE_DELETE, 0x80 | 0x53, 0, 0 },
};
#define N_TECLAS ((int)(sizeof teclas / sizeof teclas[0]))

static Uint8 estado_teclas[SDL_NUM_SCANCODES];   /* SDL_GetKeyboardState */
static Uint16 mods_atual = KMOD_NONE;
static int texto_ativo = 1;   /* en la SDL 2 de escritorio, el texto está activo desde el inicio */
static int foco_visto = 0, foco_atual = 1;

static const tecla_t *tecla_por_sdl(int sc)
{
    for (int i = 0; i < N_TECLAS; i++)
        if (teclas[i].sdl == sc) return &teclas[i];
    return NULL;
}

/* El código de tecla (SDLK_*) de un código de rastreo, como en EE. UU. */
SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode s)
{
    const tecla_t *t = tecla_por_sdl(s);
    if (t && t->normal && s != SDL_SCANCODE_KP_MULTIPLY && s != SDL_SCANCODE_KP_MINUS && s != SDL_SCANCODE_KP_PLUS &&
        s != SDL_SCANCODE_KP_DIVIDE) {
        if (s == SDL_SCANCODE_RETURN) return SDLK_RETURN;
        return (SDL_Keycode)(unsigned char)t->normal;
    }
    if (s == SDL_SCANCODE_DELETE) return SDLK_DELETE;
    return SDL_SCANCODE_TO_KEYCODE(s);
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode k)
{
    if (k & SDLK_SCANCODE_MASK) return (SDL_Scancode)(k & ~SDLK_SCANCODE_MASK);
    if (k == SDLK_DELETE) return SDL_SCANCODE_DELETE;
    for (int i = 0; i < N_TECLAS; i++)
        if (teclas[i].normal && (unsigned char)teclas[i].normal == (unsigned)k && teclas[i].sdl < SDL_SCANCODE_KP_DIVIDE)
            return (SDL_Scancode)teclas[i].sdl;
    return SDL_SCANCODE_UNKNOWN;
}

const char *SDL_GetScancodeName(SDL_Scancode s)
{
    static char b[16];
    switch (s) {
    case SDL_SCANCODE_RETURN: return "Return";
    case SDL_SCANCODE_ESCAPE: return "Escape";
    case SDL_SCANCODE_BACKSPACE: return "Backspace";
    case SDL_SCANCODE_TAB: return "Tab";
    case SDL_SCANCODE_SPACE: return "Space";
    case SDL_SCANCODE_UP: return "Up";
    case SDL_SCANCODE_DOWN: return "Down";
    case SDL_SCANCODE_LEFT: return "Left";
    case SDL_SCANCODE_RIGHT: return "Right";
    case SDL_SCANCODE_LSHIFT: return "Left Shift";
    case SDL_SCANCODE_RSHIFT: return "Right Shift";
    case SDL_SCANCODE_LCTRL: return "Left Ctrl";
    case SDL_SCANCODE_RCTRL: return "Right Ctrl";
    case SDL_SCANCODE_LALT: return "Left Alt";
    case SDL_SCANCODE_RALT: return "Right Alt";
    default: break;
    }
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12) { snprintf(b, sizeof b, "F%d", s - SDL_SCANCODE_F1 + 1); return b; }
    const tecla_t *t = tecla_por_sdl(s);
    if (t && t->normal > 32) { b[0] = (char)toupper((unsigned char)t->normal); b[1] = 0; return b; }
    return "";
}
const char *SDL_GetKeyName(SDL_Keycode k)
{
    static char b[4];
    if (!(k & SDLK_SCANCODE_MASK) && k > 32 && k < 127) { b[0] = (char)toupper(k); b[1] = 0; return b; }
    return SDL_GetScancodeName(SDL_GetScancodeFromKey(k));
}

const Uint8 *SDL_GetKeyboardState(int *n) { if (n) *n = SDL_NUM_SCANCODES; return estado_teclas; }
SDL_Keymod SDL_GetModState(void) { return (SDL_Keymod)mods_atual; }
void SDL_SetModState(SDL_Keymod m) { mods_atual = (Uint16)m; }
void SDL_StartTextInput(void) { texto_ativo = 1; }
void SDL_StopTextInput(void) { texto_ativo = 0; }
SDL_bool SDL_IsTextInputActive(void) { return texto_ativo ? SDL_TRUE : SDL_FALSE; }

static Uint16 mod_de(int sc)
{
    switch (sc) {
    case SDL_SCANCODE_LSHIFT: return KMOD_LSHIFT;
    case SDL_SCANCODE_RSHIFT: return KMOD_RSHIFT;
    case SDL_SCANCODE_LCTRL: return KMOD_LCTRL;
    case SDL_SCANCODE_RCTRL: return KMOD_RCTRL;
    case SDL_SCANCODE_LALT: return KMOD_LALT;
    case SDL_SCANCODE_RALT: return KMOD_RALT;
    default: return 0;
    }
}

static void evento_tecla(Uint32 tipo, int sc, int repetida)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    Uint16 m = mod_de(sc);
    if (m) {
        if (tipo == SDL_KEYDOWN) mods_atual |= m;
        else mods_atual &= (Uint16)~m;
    }
    ev.type = tipo;
    ev.key.windowID = 1;
    ev.key.state = tipo == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
    ev.key.repeat = (Uint8)repetida;
    ev.key.keysym.scancode = (SDL_Scancode)sc;
    ev.key.keysym.sym = SDL_GetKeyFromScancode((SDL_Scancode)sc);
    ev.key.keysym.mod = mods_atual;
    if (sc > 0 && sc < SDL_NUM_SCANCODES) estado_teclas[sc] = tipo == SDL_KEYDOWN;
    enfileirar(&ev);
}

/* Lo que da el kernel en un EV_TECLA (un carácter Latin-1 o TECLA_*) en
 * el código de rastreo de SDL 2. Los códigos 1..26 son a la vez Ctrl+letra
 * y caracteres de control (8 = retroceso, 9 = tabulador, 10 = Intro): los
 * distingue el Ctrl (D156). */
static int scancode_do_kernel(unsigned t, unsigned mods_kernel)
{
    switch (t) {
    case TECLA_CIMA: return SDL_SCANCODE_UP;
    case TECLA_BAIXO: return SDL_SCANCODE_DOWN;
    case TECLA_ESQUERDA: return SDL_SCANCODE_LEFT;
    case TECLA_DIREITA: return SDL_SCANCODE_RIGHT;
    case TECLA_HOME: return SDL_SCANCODE_HOME;
    case TECLA_END: return SDL_SCANCODE_END;
    case TECLA_PGUP: return SDL_SCANCODE_PAGEUP;
    case TECLA_PGDN: return SDL_SCANCODE_PAGEDOWN;
    case TECLA_INSERT: return SDL_SCANCODE_INSERT;
    case TECLA_DELETE: return SDL_SCANCODE_DELETE;
    case TECLA_ESC: return SDL_SCANCODE_ESCAPE;
    default: break;
    }
    if (t >= TECLA_F1 && t <= TECLA_F12) return SDL_SCANCODE_F1 + (int)(t - TECLA_F1);
    if (t >= 1 && t <= 26) {
        if (!(mods_kernel & 2)) {
            if (t == 8) return SDL_SCANCODE_BACKSPACE;
            if (t == 9) return SDL_SCANCODE_TAB;
            if (t == 10 || t == 13) return SDL_SCANCODE_RETURN;
        }
        return SDL_SCANCODE_A + (int)(t - 1);
    }
    if (t == 27) return SDL_SCANCODE_ESCAPE;
    if (t == 127) return SDL_SCANCODE_DELETE;
    if (t < 127) {
        for (int i = 0; i < N_TECLAS; i++)
            if (teclas[i].sdl < SDL_SCANCODE_KP_DIVIDE && (teclas[i].normal == (char)t || teclas[i].maius == (char)t))
                return teclas[i].sdl;
    }
    return SDL_SCANCODE_UNKNOWN;
}

/* El texto de una tecla, en UTF-8 (el kernel da Latin-1). */
static void evento_texto(unsigned t)
{
    if (!texto_ativo || t < 32 || t == 127 || t > 255) return;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_TEXTINPUT;
    ev.text.windowID = 1;
    if (t < 0x80) {
        ev.text.text[0] = (char)t;
    } else {
        ev.text.text[0] = (char)(0xC0 | (t >> 6));
        ev.text.text[1] = (char)(0x80 | (t & 0x3F));
    }
    enfileirar(&ev);
}

static void evento_janela(Uint8 qual)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_WINDOWEVENT;
    ev.window.windowID = 1;
    ev.window.event = qual;
    enfileirar(&ev);
}

/* Compara con el mapa de teclas que el kernel dice que están pulsadas:
 * KEYDOWN de las que se pulsaron y KEYUP de las que se soltaron. Sin foco,
 * todo se da por suelto (y la ventana avisa con FOCUS_LOST). */
static void teclas_do_mapa(void)
{
    unsigned char mapa[32];
    memset(mapa, 0, sizeof mapa);
    int foco = teclas_seguradas(mapa);
    if (foco) foco_visto = 1;
    if (foco_visto && foco != foco_atual) {
        foco_atual = foco;
        if (janela) {
            if (foco) janela->flags |= SDL_WINDOW_INPUT_FOCUS;
            else janela->flags &= ~SDL_WINDOW_INPUT_FOCUS;
        }
        evento_janela(foco ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST);
        registrar("[%s] foco %s", sagui_sdl2_nome, foco ? "ganado" : "perdido");
    }
    for (int i = 0; i < N_TECLAS; i++) {
        int agora = foco && SEGURADA(mapa, teclas[i].sc);
        int sc = teclas[i].sdl;
        if (agora && !estado_teclas[sc]) evento_tecla(SDL_KEYDOWN, sc, 0);
        else if (!agora && estado_teclas[sc]) evento_tecla(SDL_KEYUP, sc, 0);
    }
}

/* ------------------------------------------------------------------ eventos */

static SDL_Event fila[TAM_FILA];
static int fila_ini = 0, fila_n = 0;
static Uint8 eventos_ignorados[0x90];   /* por (tipo >> 8): 1 = SDL_IGNORE */

static int ignorado(Uint32 tipo) { return (tipo >> 8) < sizeof eventos_ignorados && eventos_ignorados[tipo >> 8]; }

static void enfileirar(const SDL_Event *ev)
{
    if (ignorado(ev->type)) return;
    if (fila_n >= TAM_FILA) return;   /* llena: se pierde (como la SDL) */
    SDL_Event *d = &fila[(fila_ini + fila_n) % TAM_FILA];
    *d = *ev;
    d->common.timestamp = tempo_ms();
    fila_n++;
}

/* De la ventana del sistema a la ventana lógica. */
static void pos_logica(int *x, int *y)
{
    SDL_Window *w = janela;
    if (!w || !w->dl || !w->da) return;
    int sx = (int)((long long)(*x - w->dx) * w->l / w->dl), sy = (int)((long long)(*y - w->dy) * w->a / w->da);
    *x = sx < 0 ? 0 : sx >= w->l ? w->l - 1 : sx;
    *y = sy < 0 ? 0 : sy >= w->a ? w->a - 1 : sy;
}

static void ao_redimensionar(SDL_Window *w, int l, int a, int estado)
{
    int cheia = (estado == JAN_ESTADO_TELA_CHEIA);
    if (l != w->jl || a != w->ja) {
        uint32_t *px = NULL;
        if (redimensionar_janela(w->id, l, a, &px) < 0 || !px) return;
        w->px = px; w->jl = l; w->ja = a;
    }
    w->em_cheia = cheia;
    preparar_escala(w);
    apresentar(w);
    evento_janela(SDL_WINDOWEVENT_EXPOSED);
    registrar("[%s] ventana %dx%d (%s)", sagui_sdl2_nome, l, a, cheia ? "pantalla completa" : "ventana");
}

static void tratar_evento(evento_t *pe)
{
    evento_t e = *pe;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    switch (e.tipo) {
    case EV_REDIMENSIONAR:
        if (janela) ao_redimensionar(janela, e.x, e.y, (int)e.tecla);
        break;
    case EV_TECLA: {
        int sc = scancode_do_kernel(e.tecla, e.modificadores);
        if (sc != SDL_SCANCODE_UNKNOWN) {
            /* D436: si el kernel dice que va con Ctrl y el mapa no vio la tecla Ctrl (p. ej. Ctrl+F mandada de golpe),
             * el evento lleva el Ctrl igualmente (AoEN: Ctrl+F, el HUD) */
            Uint16 antes = mods_atual;
            if ((e.modificadores & 2) && !(mods_atual & KMOD_CTRL)) mods_atual |= KMOD_LCTRL;
            /* Ya pulsada (el mapa la vio): la repetición del teclado. */
            evento_tecla(SDL_KEYDOWN, sc, estado_teclas[sc] ? 1 : 0);
            mods_atual = antes;
        }
        evento_texto(e.tecla);
        break;
    }
    case EV_MOVER: {
        int x = e.x, y = e.y;
        pos_logica(&x, &y);
        if (x == rato_x && y == rato_y) break;
        ev.type = SDL_MOUSEMOTION;
        ev.motion.windowID = 1;
        ev.motion.state = rato_botoes;
        ev.motion.x = x;
        ev.motion.y = y;
        ev.motion.xrel = x - rato_x;
        ev.motion.yrel = y - rato_y;
        rato_rel_x += ev.motion.xrel;
        rato_rel_y += ev.motion.yrel;
        rato_x = x;
        rato_y = y;
        enfileirar(&ev);
        break;
    }
    case EV_CLIQUE:
    case EV_SOLTOU: {
        int x = e.x, y = e.y;
        pos_logica(&x, &y);
        Uint8 b = (e.botoes & 1) ? SDL_BUTTON_LEFT : (e.botoes & 4) ? SDL_BUTTON_MIDDLE : SDL_BUTTON_RIGHT;
        /* Si el ratón se movió sin EV_MOVER (fuera de la ventana), primero el movimiento. */
        if (x != rato_x || y != rato_y) {
            SDL_Event m;
            memset(&m, 0, sizeof m);
            m.type = SDL_MOUSEMOTION;
            m.motion.windowID = 1;
            m.motion.state = rato_botoes;
            m.motion.x = x;
            m.motion.y = y;
            m.motion.xrel = x - rato_x;
            m.motion.yrel = y - rato_y;
            rato_x = x;
            rato_y = y;
            enfileirar(&m);
        }
        ev.type = e.tipo == EV_CLIQUE ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        ev.button.windowID = 1;
        ev.button.button = b;
        ev.button.state = e.tipo == EV_CLIQUE ? SDL_PRESSED : SDL_RELEASED;
        ev.button.clicks = (Uint8)(e.cliques ? e.cliques : 1);
        ev.button.x = x;
        ev.button.y = y;
        if (e.tipo == EV_CLIQUE) rato_botoes |= SDL_BUTTON(b);
        else rato_botoes &= ~SDL_BUTTON(b);
        enfileirar(&ev);
        break;
    }
    case EV_RODA:
        ev.type = SDL_MOUSEWHEEL;
        ev.wheel.windowID = 1;
        ev.wheel.y = -(int)e.cliques;   /* el kernel: + hacia abajo; la SDL: + hacia arriba */
        ev.wheel.preciseY = (float)ev.wheel.y;
        ev.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
        enfileirar(&ev);
        break;
    case EV_RODA_H:   /* D407: rueda horizontal (panel táctil); en la SDL, + hacia la derecha como aquí */
        ev.type = SDL_MOUSEWHEEL;
        ev.wheel.windowID = 1;
        ev.wheel.x = (int)e.cliques;
        ev.wheel.preciseX = (float)ev.wheel.x;
        ev.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
        enfileirar(&ev);
        break;
    case EV_FECHAR:
        evento_janela(SDL_WINDOWEVENT_CLOSE);
        ev.type = SDL_QUIT;
        enfileirar(&ev);
        registrar("[%s] cerrar la ventana", sagui_sdl2_nome);
        break;
    default:
        break;
    }
}

static void eventos_do_sistema(void)
{
    if (!janela) return;
    evento_t e;
    while (esperar_evento(janela->id, &e, 0) == 1) tratar_evento(&e);
    teclas_do_mapa();
}

static void joy_atualizar(int forcar);

void SDL_PumpEvents(void)
{
    bombear_audio();
    eventos_do_sistema();
    joy_atualizar(0);   /* D307: conexiones y estado de los mandos */
}

int SDL_PollEvent(SDL_Event *e)
{
    SDL_PumpEvents();
    if (fila_n <= 0) return 0;
    if (e) {
        *e = fila[fila_ini];
        fila_ini = (fila_ini + 1) % TAM_FILA;
        fila_n--;
    }
    return 1;
}

int SDL_WaitEventTimeout(SDL_Event *e, int ms)
{
    unsigned fim = tempo_ms() + (unsigned)ms;
    for (;;) {
        if (SDL_PollEvent(e)) return 1;
        if (ms >= 0 && (int)(fim - tempo_ms()) <= 0) return 0;
        dormir(5);
    }
}
int SDL_WaitEvent(SDL_Event *e) { return SDL_WaitEventTimeout(e, -1); }

int SDL_PushEvent(SDL_Event *e)
{
    if (!e) return -1;
    if (ignorado(e->type)) return 0;
    enfileirar(e);
    return 1;
}

int SDL_PeepEvents(SDL_Event *es, int n, SDL_eventaction acao, Uint32 min, Uint32 max)
{
    if (acao == SDL_ADDEVENT) {
        int k = 0;
        for (; k < n; k++) enfileirar(&es[k]);
        return k;
    }
    int achados = 0;
    for (int i = 0; i < fila_n && achados < n;) {
        SDL_Event *x = &fila[(fila_ini + i) % TAM_FILA];
        if (x->type < min || x->type > max) { i++; continue; }
        if (es) es[achados] = *x;
        achados++;
        if (acao == SDL_GETEVENT) {
            for (int j = i; j < fila_n - 1; j++)
                fila[(fila_ini + j) % TAM_FILA] = fila[(fila_ini + j + 1) % TAM_FILA];
            fila_n--;
        } else {
            i++;
        }
    }
    return achados;
}

SDL_bool SDL_HasEvent(Uint32 tipo) { return SDL_PeepEvents(NULL, 1, SDL_PEEKEVENT, tipo, tipo) > 0 ? SDL_TRUE : SDL_FALSE; }
void SDL_FlushEvents(Uint32 min, Uint32 max)
{
    SDL_Event lixo;
    while (SDL_PeepEvents(&lixo, 1, SDL_GETEVENT, min, max) > 0) {}
}
void SDL_FlushEvent(Uint32 tipo) { SDL_FlushEvents(tipo, tipo); }

Uint8 SDL_EventState(Uint32 tipo, int estado)
{
    unsigned i = tipo >> 8;
    if (i >= sizeof eventos_ignorados) return SDL_ENABLE;
    Uint8 antes = eventos_ignorados[i] ? SDL_IGNORE : SDL_ENABLE;
    if (estado == SDL_IGNORE) eventos_ignorados[i] = 1;
    else if (estado == SDL_ENABLE) eventos_ignorados[i] = 0;
    return antes;
}

Uint32 SDL_RegisterEvents(int n)
{
    static Uint32 proximo = SDL_USEREVENT;
    Uint32 r = proximo;
    if (n <= 0 || proximo + (Uint32)n > SDL_LASTEVENT) return (Uint32)-1;
    proximo += (Uint32)n;
    return r;
}

/* ------------------------------------------------------------------ reloj */

Uint32 SDL_GetTicks(void) { return tempo_ms(); }
Uint64 SDL_GetTicks64(void) { return tempo_ms(); }
Uint64 SDL_GetPerformanceCounter(void) { return tempo_ms(); }
Uint64 SDL_GetPerformanceFrequency(void) { return 1000; }

void SDL_Delay(Uint32 ms)
{
    bombear_audio();
    /* Las esperas largas, a trozos: el audio no se queda sin datos. */
    while (ms > 20) {
        dormir(20);
        ms -= 20;
        bombear_audio();
    }
    if (ms) dormir(ms);
}

/* Sin hilos no hay a quién llamar a destiempo. */
SDL_TimerID SDL_AddTimer(Uint32 i, SDL_TimerCallback f, void *p)
{
    (void)i; (void)f; (void)p;
    SDL_SetError("SDL_AddTimer: no hay temporizadores en la SDL del SaguiOS");
    return 0;
}
SDL_bool SDL_RemoveTimer(SDL_TimerID id) { (void)id; return SDL_FALSE; }

/* ------------------------------------------------------------------ audio */

/* D393 (hilo de audio de la SDL, 2026-11-22). Hasta ahora el audio se movía desde el hilo principal (SDL_PollEvent,
 * SDL_Delay, SDL_RenderPresent): un juego que se atascaba 400 ms en un cuadro (trozos nuevos en Brumacanto) dejaba la
 * tarjeta sin datos y el sonido se cortaba. Ahora SDL_PauseAudio(0) arranca un HILO DE AUDIO propio, como en la SDL de
 * verdad: alimenta el flujo del sistema al ritmo de la tarjeta, por delante de la latencia del kernel (D436: latencia + 60 ms,
 * como mínimo 120), opcionalmente con «alta prioridad de audio» (PLAN_AUDIO, D436, SDL_SAGUI_AUDIO_PRIO=1)
 * y llama a la llamada de vuelta del programa DESDE ESE HILO, con el cerrojo del dispositivo tomado (SDL_LockAudio y
 * SDL_LockAudioDevice son ahora de verdad; recursivos). La cola de SDL_QueueAudio lleva su propio cerrojo corto.
 *   - SDL_SAGUI_AUDIO_HILO=0 (variable de entorno, o la pista del mismo nombre) vuelve al modo de antes: el principal
 *     mueve el audio. SDL_SAGUI_AUDIO_PRIO=1 (o la pista) pide la alta prioridad (de serie NO: con ella Freeciv se quedaba
 *     sin contestar al servidor, ver D393).
 *   - El hilo solo toca el flujo de audio y la llamada de vuelta: nada de vídeo ni de eventos (regla de D393).
 *   - Formato: el sistema toca U8 y S16; si el programa pide otro (S8, U16, S32, F32) y no permite cambiarlo, la SDL se lo
 *     deja tal cual y convierte aquí al escribir (como la SDL de verdad); si lo permite, se le da S16 como antes. */

static int fmt_bytes(SDL_AudioFormat f);
static double leer_muestra(const Uint8 *p, SDL_AudioFormat f);
static void escribir_muestra(Uint8 *p, SDL_AudioFormat f, double v);

static SDL_AudioSpec audio_spec;              /* lo que ve el programa (su formato, aunque se convierta) */
static SDL_AudioFormat audio_fmt_sis;         /* lo que se escribe en el flujo del sistema: U8 o S16 */
static int audio_conv;                        /* el formato del programa no es el del sistema */
static int audio_fd = -1;
static int audio_aberto = 0, audio_pausado = 1;
static Uint8 *audio_buf = NULL;               /* un bloque en el formato del programa */
static Uint8 *audio_buf_sis = NULL;           /* y convertido al del sistema (solo con conversión) */
static unsigned audio_bloco = 0;              /* bytes del bloque en el formato del programa */
static Uint32 audio_mudo_marca = 0;           /* sin tarjeta: el reloj del último bloque dado */
static Uint64 audio_mudo_credito = 0;         /* y los bytes que ya tocaba haber pedido */
/* La cola de SDL_QueueAudio (si no hay llamada de vuelta): bytes en el formato del programa. */
static Uint8 *fila_audio = NULL;
static Uint32 fila_audio_ini = 0, fila_audio_n = 0, fila_audio_cap = 0;
static mutex_t fila_cerrojo;
static int fila_cerrojo_listo;
static SDL_mutex *audio_cerrojo;              /* el cerrojo del dispositivo (recursivo); vive hasta salir */
#define AUDIO_ADIANTO_MS 120                  /* lo que se deja por delante en el flujo del sistema (mínimo) */
#define AUDIO_ADIANTO_EXTRA_MS 60             /* y sobre la latencia del kernel (D436) */

/* El hilo de audio y lo que anota. */
static thread_t *audio_hilo;
static volatile int audio_hilo_parar;
static volatile int audio_hilo_vivo;          /* el hilo corre: el principal ya no mueve el audio */
static int audio_lat_kernel_ms = 100;
static struct { long bloques, vacios; unsigned hueco_max, falhas0, falhas; Uint32 ult; int prio; } audio_est;

static int bits_de(SDL_AudioFormat f) { return SDL_AUDIO_BITSIZE(f); }

static int audio_formato_convertible(SDL_AudioFormat f)
{
    return f == AUDIO_S8 || f == AUDIO_U16LSB || f == AUDIO_S16LSB || f == AUDIO_S32LSB || f == AUDIO_F32LSB;
}

/* ¿Hilo de audio? La variable manda; si no, la pista; de serie, sí. */
static int audio_hilo_deseado(void)
{
    const char *e = getenv("SDL_SAGUI_AUDIO_HILO");
    if (!e || !*e) e = SDL_GetHint("SDL_SAGUI_AUDIO_HILO");
    return !(e && *e && (e[0] == '0' || e[0] == 'n' || e[0] == 'N' || e[0] == 'f' || e[0] == 'F'));
}

/* Alta prioridad de audio (PLAN_AUDIO, D436): de serie apagada (Freeciv, con ella y con el servidor en la misma CPU, dejaba de
 * contestar a los pings del servidor). SDL_SAGUI_AUDIO_PRIO=1 (variable o pista) la pide. */
static int audio_prio_deseada(void)
{
    const char *e = getenv("SDL_SAGUI_AUDIO_PRIO");
    if (!e || !*e) e = SDL_GetHint("SDL_SAGUI_AUDIO_PRIO");
    return e && *e && e[0] != '0' && e[0] != 'n' && e[0] != 'N' && e[0] != 'f' && e[0] != 'F';
}

static void audio_cerrojo_listo(void)
{
    if (!audio_cerrojo) audio_cerrojo = SDL_CreateMutex();
    if (!fila_cerrojo_listo) { mutex_criar(&fila_cerrojo); fila_cerrojo_listo = 1; }
}

/* Convierte n muestras del formato del programa al del sistema. */
static void audio_convertir(const Uint8 *o, Uint8 *d, unsigned n)
{
    int bo = fmt_bytes(audio_spec.format), bd = fmt_bytes(audio_fmt_sis);
    for (unsigned i = 0; i < n; i++)
        escribir_muestra(d + (size_t)i * bd, audio_fmt_sis, leer_muestra(o + (size_t)i * bo, audio_spec.format));
}

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *nome, int captura, const SDL_AudioSpec *pedido,
                                      SDL_AudioSpec *obtido, int mudancas)
{
    (void)nome;
    if (captura) { SDL_SetError("no hay grabación"); return 0; }
    if (audio_aberto) { SDL_SetError("el audio ya está abierto"); return 0; }
    if (!pedido) return 0;
    audio_cerrojo_listo();
    audio_spec = *pedido;
    /* El sistema toca 8 bits sin signo y 16 con signo. Otro formato: si el programa deja cambiarlo, se le da S16; si no,
     * se queda el suyo y se convierte al escribir. */
    audio_fmt_sis = pedido->format == AUDIO_U8 ? AUDIO_U8 : AUDIO_S16SYS;
    audio_conv = 0;
    if (pedido->format != AUDIO_U8 && pedido->format != AUDIO_S16SYS) {
        if (!(mudancas & SDL_AUDIO_ALLOW_FORMAT_CHANGE) && audio_formato_convertible(pedido->format)) audio_conv = 1;
        else audio_spec.format = AUDIO_S16SYS;
    }
    if (!audio_spec.channels) audio_spec.channels = 2;
    if (audio_spec.channels > 2) audio_spec.channels = 2;
    if (audio_spec.freq < 4000 || audio_spec.freq > 96000) audio_spec.freq = 22050;
    if (!audio_spec.samples) audio_spec.samples = 1024;
    int bits = bits_de(audio_fmt_sis);
    int bp = fmt_bytes(audio_spec.format);
    audio_spec.silence = audio_spec.format == AUDIO_U8 ? 0x80 : 0;
    audio_spec.size = (Uint32)audio_spec.samples * audio_spec.channels * (unsigned)bp;
    /* el bloque, en cuadros: que su tamaño en el sistema no pase de 16 KB */
    unsigned cuadros = audio_spec.samples, maxc = 16384u / (audio_spec.channels * (unsigned)(bits / 8));
    if (cuadros > maxc) cuadros = maxc;
    audio_bloco = cuadros * audio_spec.channels * (unsigned)bp;
    free(audio_buf); free(audio_buf_sis);
    audio_buf = malloc(audio_bloco);
    audio_buf_sis = audio_conv ? malloc(cuadros * audio_spec.channels * (unsigned)(bits / 8)) : NULL;
    if (!audio_buf || (audio_conv && !audio_buf_sis)) {
        free(audio_buf); free(audio_buf_sis); audio_buf = audio_buf_sis = NULL;
        SDL_SetError("sin memoria para el audio");
        return 0;
    }
    audio_fd = audio_abrir(audio_spec.freq, audio_spec.channels, bits);
    if (audio_fd < 0) audio_fd = -1;   /* sin tarjeta: mudo, pero se llama igual al programa */
    audio_aberto = 1;
    audio_pausado = 1;
    fila_audio_ini = fila_audio_n = 0;
    memset(&audio_est, 0, sizeof audio_est);
    registrar("[%s] audio %d Hz, %d canales, %d bits%s, bloque %u (%s)", sagui_sdl2_nome, audio_spec.freq,
              audio_spec.channels, bits, audio_conv ? " (convertido desde el formato del programa)" : "", audio_bloco,
              audio_fd >= 0 ? "con tarjeta" : "mudo");
    if (obtido) *obtido = audio_spec;
    return 1;
}

int SDL_OpenAudio(SDL_AudioSpec *pedido, SDL_AudioSpec *obtido)
{
    SDL_AudioDeviceID d = SDL_OpenAudioDevice(NULL, 0, pedido, obtido ? obtido : pedido, 0);
    return d ? 0 : -1;
}

/* ---- el bloque: de la llamada de vuelta o de la cola, convertido y al flujo del sistema ---- */

/* Pasa un bloque al flujo del sistema (o, sin tarjeta, lo tira). Devuelve 0 si no había nada que dar (cola vacía). */
static int audio_dar_bloque(int escribir)
{
    unsigned n = audio_bloco;   /* bytes en el formato del programa */
    if (audio_spec.callback) {
        memset(audio_buf, audio_spec.silence, audio_bloco);
        SDL_LockMutex(audio_cerrojo);   /* como la SDL: la llamada de vuelta va con el cerrojo del dispositivo tomado */
        audio_spec.callback(audio_spec.userdata, audio_buf, (int)audio_bloco);
        SDL_UnlockMutex(audio_cerrojo);
    } else {
        unsigned marco = (unsigned)fmt_bytes(audio_spec.format) * audio_spec.channels;
        mutex_tomar(&fila_cerrojo);
        n = fila_audio_n < audio_bloco ? fila_audio_n : audio_bloco;
        n -= n % marco;
        if (n) {
            memcpy(audio_buf, fila_audio + fila_audio_ini, n);
            fila_audio_ini += n;
            fila_audio_n -= n;
            if (!fila_audio_n) fila_audio_ini = 0;
        }
        mutex_largar(&fila_cerrojo);
        if (!n) return 0;
    }
    if (!escribir || audio_fd < 0) return 1;
    if (audio_conv) {
        unsigned muestras = n / (unsigned)fmt_bytes(audio_spec.format);
        audio_convertir(audio_buf, audio_buf_sis, muestras);
        audio_escrever(audio_fd, audio_buf_sis, muestras * (unsigned)fmt_bytes(audio_fmt_sis));
    } else {
        audio_escrever(audio_fd, audio_buf, n);
    }
    return 1;
}

/* Sin tarjeta (o sin sonido en el sistema): el programa que mezcla en su llamada de vuelta espera que se la llame al ritmo
 * del dispositivo (gnuboy espera cada bloque) y el de la cola, que se vacíe: se hace al ritmo del reloj y se tira lo que sale. */
static void audio_dar_mudo(void)
{
    Uint32 ahora = tempo_ms();
    if (!audio_mudo_marca) { audio_mudo_marca = ahora ? ahora : 1; audio_mudo_credito = 0; }
    Uint32 bytes_s = (Uint32)audio_spec.freq * audio_spec.channels * (Uint32)fmt_bytes(audio_spec.format);
    audio_mudo_credito += (Uint64)(ahora - audio_mudo_marca) * bytes_s / 1000;
    audio_mudo_marca = ahora;
    for (int vezes = 0; vezes < 8 && audio_mudo_credito >= audio_bloco; vezes++) {
        if (!audio_dar_bloque(0)) { audio_mudo_credito = 0; break; }
        audio_mudo_credito -= audio_bloco;
    }
    if (audio_mudo_credito > 4u * audio_bloco) audio_mudo_credito = 0;   /* se paró mucho: no se recupera */
}

static int audio_adelanto_ms(void)
{
    int a = audio_lat_kernel_ms + AUDIO_ADIANTO_EXTRA_MS;
    return a > AUDIO_ADIANTO_MS ? a : AUDIO_ADIANTO_MS;
}

static void audio_leer_kernel(void)
{
    static info_audio_t ia;   /* grande (los flujos y las salidas): fuera de la pila del hilo */
    if (info_sistema(INFO_AUDIO, &ia, sizeof ia) < 0 || ia.estado != AUDIO_FUNCIONANDO) return;
    audio_lat_kernel_ms = ia.latencia_ms < 10 ? 10 : ia.latencia_ms > 1000 ? 1000 : (int)ia.latencia_ms;
    if (!audio_est.falhas0) audio_est.falhas0 = ia.falhas + 1;
    audio_est.falhas = ia.falhas + 1 - audio_est.falhas0;
}

/* El hilo: da bloques en cuanto la tarjeta tiene sitio, sin esperar a ningún cuadro del juego. */
static void *audio_hilo_fn(void *arg)
{
    (void)arg;
    int prio = 0, lleno = 0, vacio_antes = 0;
    if (audio_prio_deseada()) {
        int r = planificador_hilo_audio(1);   /* solo vale para el hilo que llama; el kernel la quita a quien gasta mucho */
        prio = r >= 0;
        audio_est.prio = r < 0 ? -1 : 1;
    }
    Uint32 lat_visto = 0;
    while (!audio_hilo_parar) {
        if (audio_pausado) { audio_mudo_marca = 0; dormir(5); continue; }
        if (audio_fd < 0) { audio_dar_mudo(); dormir(5); continue; }
        Uint32 ahora = tempo_ms();
        if (!lat_visto || ahora - lat_visto >= 500) { audio_leer_kernel(); lat_visto = ahora ? ahora : 1; }
        int pend = audio_pendente(audio_fd);
        if (pend >= audio_adelanto_ms() || audio_livre(audio_fd) < (int)(audio_bloco * (unsigned)fmt_bytes(audio_fmt_sis) / (unsigned)fmt_bytes(audio_spec.format))) {
            lleno = 1;
            dormir(5);
            continue;
        }
        /* el flujo propio vacío (lo pendiente no llega ni a la latencia del kernel): el kernel ya mete silencio; una vez por racha */
        int vacio = lleno && pend + 5 < audio_lat_kernel_ms;
        if (vacio && !vacio_antes) audio_est.vacios++;
        vacio_antes = vacio;
        if (audio_est.ult && ahora - audio_est.ult > audio_est.hueco_max) audio_est.hueco_max = ahora - audio_est.ult;
        audio_est.ult = ahora;
        if (!audio_dar_bloque(1)) { dormir(5); continue; }   /* cola vacía: nada que dar */
        audio_est.bloques++;
    }
    if (prio) planificador_hilo_audio(0);
    return NULL;
}

static void audio_hilo_arrancar(void)
{
    if (audio_hilo || audio_hilo_vivo || !audio_hilo_deseado()) return;
    audio_hilo_parar = 0;
    hay_hilos = 1; atomico_barreira();   /* desde ahora hay hilos: es_principal() distingue al principal */
    audio_hilo = thread_create(audio_hilo_fn, NULL, 0);
    if (!audio_hilo) { registrar("[%s] audio: no se pudo crear el hilo, el principal mueve el audio", sagui_sdl2_nome); return; }
    audio_hilo_vivo = 1;
    registrar("[%s] audio: hilo propio (latencia del kernel + %d ms por delante, alta prioridad %s)", sagui_sdl2_nome,
              AUDIO_ADIANTO_EXTRA_MS, audio_prio_deseada() ? "pedida" : "no pedida");
}

static void audio_hilo_parar_y_esperar(void)
{
    if (!audio_hilo) return;
    audio_hilo_parar = 1;
    thread_join(audio_hilo);
    audio_hilo = NULL;
    audio_hilo_vivo = 0;
    registrar("[%s] audio: hilo parado; %ld bloques, sin datos %ld veces, hueco máximo %u ms, alta prioridad %s, cortes de la tarjeta %u",
              sagui_sdl2_nome, audio_est.bloques, audio_est.vacios, audio_est.hueco_max,
              audio_est.prio > 0 ? "sí" : audio_est.prio < 0 ? "no disponible" : "no", audio_est.falhas);
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID d, int pausa)
{
    (void)d;
    audio_pausado = pausa != 0;
    audio_mudo_marca = 0;
    if (!pausa && audio_aberto) audio_hilo_arrancar();
}
void SDL_PauseAudio(int pausa) { SDL_PauseAudioDevice(1, pausa); }
SDL_AudioStatus SDL_GetAudioDeviceStatus(SDL_AudioDeviceID d)
{
    (void)d;
    return !audio_aberto ? SDL_AUDIO_STOPPED : audio_pausado ? SDL_AUDIO_PAUSED : SDL_AUDIO_PLAYING;
}
SDL_AudioStatus SDL_GetAudioStatus(void) { return SDL_GetAudioDeviceStatus(1); }

/* El cerrojo del dispositivo: lo toma la llamada de vuelta del programa; el programa lo toma para tocar lo que comparte con ella. */
void SDL_LockAudio(void) { if (audio_cerrojo) SDL_LockMutex(audio_cerrojo); }
void SDL_UnlockAudio(void) { if (audio_cerrojo) SDL_UnlockMutex(audio_cerrojo); }
void SDL_LockAudioDevice(SDL_AudioDeviceID d) { (void)d; SDL_LockAudio(); }
void SDL_UnlockAudioDevice(SDL_AudioDeviceID d) { (void)d; SDL_UnlockAudio(); }

void SDL_CloseAudioDevice(SDL_AudioDeviceID d)
{
    (void)d;
    if (!audio_aberto) return;
    audio_pausado = 1;
    audio_hilo_parar_y_esperar();
    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
    audio_aberto = 0;
    free(audio_buf);
    audio_buf = NULL;
    free(audio_buf_sis);
    audio_buf_sis = NULL;
    mutex_tomar(&fila_cerrojo);
    free(fila_audio);
    fila_audio = NULL;
    fila_audio_ini = fila_audio_n = fila_audio_cap = 0;
    mutex_largar(&fila_cerrojo);
}
void SDL_CloseAudio(void) { SDL_CloseAudioDevice(1); }

int SDL_QueueAudio(SDL_AudioDeviceID d, const void *dados, Uint32 n)
{
    (void)d;
    if (!audio_aberto) return SDL_SetError("el audio no está abierto");
    if (audio_spec.callback) return SDL_SetError("SDL_QueueAudio con llamada de vuelta");
    int r = 0;
    mutex_tomar(&fila_cerrojo);
    if (fila_audio_ini && fila_audio_ini + fila_audio_n + n > fila_audio_cap) {   /* hueco delante: se compacta */
        memmove(fila_audio, fila_audio + fila_audio_ini, fila_audio_n);
        fila_audio_ini = 0;
    }
    if (fila_audio_ini + fila_audio_n + n > fila_audio_cap) {
        Uint32 c = fila_audio_cap ? fila_audio_cap * 2 : 65536;
        while (c < fila_audio_n + n) c *= 2;
        Uint8 *novo = realloc(fila_audio, c);
        if (!novo) r = SDL_SetError("sin memoria para la cola de audio");
        else { fila_audio = novo; fila_audio_cap = c; }
    }
    if (!r) {
        memcpy(fila_audio + fila_audio_ini + fila_audio_n, dados, n);
        fila_audio_n += n;
    }
    mutex_largar(&fila_cerrojo);
    if (r) return r;
    bombear_audio();
    return 0;
}
Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID d)
{
    (void)d;
    bombear_audio();
    if (!fila_cerrojo_listo) return 0;
    mutex_tomar(&fila_cerrojo);
    Uint32 n = fila_audio_n;
    mutex_largar(&fila_cerrojo);
    return n;
}
void SDL_ClearQueuedAudio(SDL_AudioDeviceID d)
{
    (void)d;
    if (!fila_cerrojo_listo) return;
    mutex_tomar(&fila_cerrojo);
    fila_audio_ini = fila_audio_n = 0;
    mutex_largar(&fila_cerrojo);
}

void SDL_MixAudioFormat(Uint8 *d, const Uint8 *o, SDL_AudioFormat formato, Uint32 n, int volume)
{
    if (volume <= 0) return;
    if (volume > SDL_MIX_MAXVOLUME) volume = SDL_MIX_MAXVOLUME;
    if (formato == AUDIO_S16SYS) {
        Sint16 *dd = (Sint16 *)d;
        const Sint16 *oo = (const Sint16 *)o;
        for (Uint32 i = 0; i < n / 2; i++) {
            int v = dd[i] + (oo[i] * volume) / SDL_MIX_MAXVOLUME;
            dd[i] = (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    } else if (formato == AUDIO_S8) {
        for (Uint32 i = 0; i < n; i++) {
            int v = (Sint8)d[i] + ((Sint8)o[i] * volume) / SDL_MIX_MAXVOLUME;
            d[i] = (Uint8)(Sint8)(v > 127 ? 127 : v < -128 ? -128 : v);
        }
    } else {
        for (Uint32 i = 0; i < n; i++) {
            int v = (d[i] - 128) + ((o[i] - 128) * volume) / SDL_MIX_MAXVOLUME;
            v = v > 127 ? 127 : v < -128 ? -128 : v;
            d[i] = (Uint8)(v + 128);
        }
    }
}
void SDL_MixAudio(Uint8 *d, const Uint8 *o, Uint32 n, int volume) { SDL_MixAudioFormat(d, o, audio_spec.format, n, volume); }

int SDL_GetNumAudioDevices(int captura) { return captura ? 0 : 1; }
const char *SDL_GetAudioDeviceName(int i, int captura) { (void)i; return captura ? NULL : "SaguiOS"; }
const char *SDL_GetCurrentAudioDriver(void) { return "saguios"; }

/* Modo de antes (SDL_SAGUI_AUDIO_HILO=0, o si no se pudo crear el hilo): pasa audio al flujo del sistema desde el hilo
 * principal mientras quede menos de AUDIO_ADIANTO_MS por tocar y quepa otro bloque. Con el hilo de audio en marcha, no
 * hace nada de audio. En los dos casos hace avanzar la OpenAL, si está. */
static void bombear_audio(void)
{
    if (!es_principal()) return;   /* D393: un hilo de trabajo no mueve el audio */
    if (sagui_al_bombear) sagui_al_bombear();
    if (audio_hilo_vivo) return;   /* lo mueve el hilo de audio */
    if (!audio_aberto || audio_pausado || !audio_buf) return;
    if (audio_fd < 0) { audio_dar_mudo(); return; }
    unsigned sis = audio_bloco * (unsigned)fmt_bytes(audio_fmt_sis) / (unsigned)fmt_bytes(audio_spec.format);
    for (int vezes = 0; vezes < 8; vezes++) {
        if (audio_pendente(audio_fd) >= AUDIO_ADIANTO_MS) return;
        if (audio_livre(audio_fd) < (int)sis) return;
        if (!audio_dar_bloque(1)) return;
    }
}

/* ------------------------------------------------------------------ mandos (D307) */

/* Los mandos del sistema (libsagui mandos.h) se leen por sondeo en SDL_PumpEvents (como mucho
 * cada 4 ms). Con SDL_INIT_JOYSTICK o SDL_INIT_GAMECONTROLLER se avisa de la conexión y la
 * desconexión (SDL_JOYDEVICEADDED/REMOVED y SDL_CONTROLLERDEVICEADDED/REMOVED; al empezar, uno
 * por cada mando ya conectado, como la SDL 2). El identificador de instancia es el del kernel
 * (cambia al reconectar). Un SDL_GameController es el mismo joystick con un mapeo: el del
 * Xbox 360, el de los Logitech Dual Action / F310 en modo D, o uno genérico. */
#define JOY_MAX MANDOS_MAX

struct SDL_Joystick {
    int      abertos;            /* SDL_JoystickOpen y SDL_GameControllerOpen cuentan */
    int      controlador;        /* abierto también como SDL_GameController */
    SDL_JoystickID id;
    info_mando_t info;
    estado_mando_t est;
    int      ligado;             /* sigue conectado */
};
struct SDL_GameController { SDL_Joystick *j; };

static info_mando_t joy_lista[JOY_MAX];
static int joy_n;
static unsigned joy_gen = 0xFFFFFFFFu;
static SDL_Joystick joys[JOY_MAX];
static SDL_GameController ctls[JOY_MAX];
static int joy_ev = SDL_ENABLE, ctl_ev = SDL_ENABLE;
static unsigned joy_ultimo, joy_ultima_lista;
static SDL_JoystickID joy_avisados[JOY_MAX];   /* los que ya tuvieron su DEVICEADDED */
static int joy_n_avisados, joy_avisos_ligados;

/* Mapeo para SDL_GameController: índice de botón o de eje del joystick, o -1. */
typedef struct {
    signed char botao[SDL_CONTROLLER_BUTTON_MAX];   /* los 4 de la cruceta: -1 = por el hat 0 */
    signed char eixo[SDL_CONTROLLER_AXIS_MAX];
    signed char gatilho_botao[2];                   /* gatillos que son botones (Logitech) */
    int         gatilho_eixo;                       /* 1 = los gatillos son ejes de -32768 a 32767 */
} mapa_t;

static void mapa_de(const info_mando_t *m, mapa_t *p)
{
    memset(p, -1, sizeof *p);
    p->gatilho_eixo = 0;
    if (m->mapa == MANDO_MAPA_XBOX360) {
        static const signed char b[] = { 0, 1, 2, 3, 6, 8, 7, 9, 10, 4, 5 };
        for (int i = 0; i < 11; i++) p->botao[i] = b[i];
        p->eixo[SDL_CONTROLLER_AXIS_LEFTX] = 0; p->eixo[SDL_CONTROLLER_AXIS_LEFTY] = 1;
        p->eixo[SDL_CONTROLLER_AXIS_RIGHTX] = 3; p->eixo[SDL_CONTROLLER_AXIS_RIGHTY] = 4;
        p->eixo[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 2; p->eixo[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] = 5;
        p->gatilho_eixo = 1;
        return;
    }
    if (m->mapa == MANDO_MAPA_DUALACTION) {
        /* 1=X 2=A 3=B 4=Y 5=LB 6=RB 7=LT 8=RT 9=Back 10=Start 11=L3 12=R3 */
        static const signed char b[] = { 1, 2, 0, 3, 8, -1, 9, 10, 11, 4, 5 };
        for (int i = 0; i < 11; i++) p->botao[i] = b[i];
        p->gatilho_botao[0] = 6; p->gatilho_botao[1] = 7;
    } else {
        /* Genérico: 1-4 A B X Y, 5-6 hombros, 7-8 gatillos, 9 Back, 10 Start, 11-12 sticks. */
        static const signed char b[] = { 0, 1, 2, 3, 8, -1, 9, 10, 11, 4, 5 };
        for (int i = 0; i < 11; i++) p->botao[i] = b[i] < m->n_botoes ? b[i] : -1;
        if (m->n_botoes > 6) p->gatilho_botao[0] = 6;
        if (m->n_botoes > 7) p->gatilho_botao[1] = 7;
    }
    for (int i = 0; i < 4 && i < m->n_eixos; i++) p->eixo[i] = (signed char)i;
}

static void joy_listar(void)
{
    unsigned gen = 0;
    int n = mandos_listar(joy_lista, JOY_MAX, &gen);
    joy_n = n < 0 ? 0 : n;
    joy_gen = gen;
}

static int joy_indice_de(SDL_JoystickID id)
{
    for (int i = 0; i < joy_n; i++) if ((SDL_JoystickID)joy_lista[i].id == id) return i;
    return -1;
}

static SDL_Joystick *joy_por_id(SDL_JoystickID id)
{
    for (int i = 0; i < JOY_MAX; i++) if (joys[i].abertos && joys[i].id == id) return &joys[i];
    return NULL;
}

int SDL_NumJoysticks(void) { joy_listar(); return joy_n; }

const char *SDL_JoystickNameForIndex(int i)
{
    if (i < 0 || i >= joy_n) joy_listar();
    return i >= 0 && i < joy_n ? joy_lista[i].nome : NULL;
}

SDL_JoystickID SDL_JoystickGetDeviceInstanceID(int i)
{
    if (i < 0 || i >= joy_n) joy_listar();
    return i >= 0 && i < joy_n ? (SDL_JoystickID)joy_lista[i].id : -1;
}

static SDL_Joystick *joy_abrir(int i)
{
    joy_listar();
    if (i < 0 || i >= joy_n) { SDL_SetError("no hay mando %d", i); return NULL; }
    SDL_Joystick *j = joy_por_id((SDL_JoystickID)joy_lista[i].id);
    if (!j) {
        for (int k = 0; k < JOY_MAX && !j; k++) if (!joys[k].abertos) j = &joys[k];
        if (!j) { SDL_SetError("demasiados mandos abiertos"); return NULL; }
        memset(j, 0, sizeof *j);
        j->info = joy_lista[i];
        j->id = (SDL_JoystickID)joy_lista[i].id;
        j->ligado = 1;
        if (mando_ler(i, &j->est) < 0 || (SDL_JoystickID)j->est.id != j->id) memset(&j->est, 0, sizeof j->est);
        registrar("[%s] joystick abierto: %s (%d ejes, %d botones, %d hats)", sagui_sdl2_nome, j->info.nome,
                  j->info.n_eixos, j->info.n_botoes, j->info.n_hats);
    }
    j->abertos++;
    return j;
}

SDL_Joystick *SDL_JoystickOpen(int i) { return joy_abrir(i); }

void SDL_JoystickClose(SDL_Joystick *j)
{
    if (!j || !j->abertos) return;
    if (--j->abertos == 0) j->controlador = 0;
}

const char *SDL_JoystickName(SDL_Joystick *j) { return j ? j->info.nome : NULL; }
int SDL_JoystickNumAxes(SDL_Joystick *j) { return j ? j->info.n_eixos : -1; }
int SDL_JoystickNumButtons(SDL_Joystick *j) { return j ? j->info.n_botoes : -1; }
int SDL_JoystickNumHats(SDL_Joystick *j) { return j ? j->info.n_hats : -1; }
int SDL_JoystickNumBalls(SDL_Joystick *j) { (void)j; return 0; }
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int e) { return j && e >= 0 && e < j->info.n_eixos ? j->est.eixos[e] : 0; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int b) { return j && b >= 0 && b < j->info.n_botoes ? (j->est.botoes >> b) & 1 : 0; }
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int h) { return j && h >= 0 && h < j->info.n_hats ? j->est.hats[h] : SDL_HAT_CENTERED; }
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *j) { return j ? j->id : -1; }
SDL_bool SDL_JoystickGetAttached(SDL_Joystick *j) { return j && j->ligado ? SDL_TRUE : SDL_FALSE; }
SDL_Joystick *SDL_JoystickFromInstanceID(SDL_JoystickID id) { return joy_por_id(id); }
Uint16 SDL_JoystickGetVendor(SDL_Joystick *j) { return j ? j->info.fabricante : 0; }
Uint16 SDL_JoystickGetProduct(SDL_Joystick *j) { return j ? j->info.produto : 0; }

/* GUID como el de la SDL 2 para USB: bus 0x03, fabricante y producto en little endian. */
static SDL_JoystickGUID guid_de(const info_mando_t *m)
{
    SDL_JoystickGUID g;
    memset(&g, 0, sizeof g);
    g.data[0] = m->tipo == MANDO_TIPO_PUERTO ? 0 : 0x03;
    g.data[4] = (Uint8)m->fabricante; g.data[5] = (Uint8)(m->fabricante >> 8);
    g.data[8] = (Uint8)m->produto; g.data[9] = (Uint8)(m->produto >> 8);
    return g;
}
SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *j)
{
    if (j) return guid_de(&j->info);
    SDL_JoystickGUID g;
    memset(&g, 0, sizeof g);
    return g;
}
SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int i)
{
    if (i < 0 || i >= joy_n) joy_listar();
    if (i >= 0 && i < joy_n) return guid_de(&joy_lista[i]);
    return SDL_JoystickGetGUID(NULL);
}
void SDL_JoystickGetGUIDString(SDL_JoystickGUID g, char *t, int max)
{
    static const char h[] = "0123456789abcdef";
    int k = 0;
    for (int i = 0; i < 16 && k + 2 < max; i++) { t[k++] = h[g.data[i] >> 4]; t[k++] = h[g.data[i] & 15]; }
    if (max > 0) t[k < max ? k : max - 1] = 0;
}

/* ---- SDL_GameController ---- */

SDL_bool SDL_IsGameController(int i)
{
    if (i < 0 || i >= joy_n) joy_listar();
    if (i < 0 || i >= joy_n) return SDL_FALSE;
    const info_mando_t *m = &joy_lista[i];
    return m->mapa != MANDO_MAPA_NENHUM || (m->n_botoes >= 2 && (m->n_eixos >= 2 || m->n_hats >= 1)) ? SDL_TRUE : SDL_FALSE;
}

SDL_GameController *SDL_GameControllerOpen(int i)
{
    if (!SDL_IsGameController(i)) { SDL_SetError("el mando %d no se puede usar como SDL_GameController", i); return NULL; }
    SDL_Joystick *j = joy_abrir(i);
    if (!j) return NULL;
    if (!j->controlador)
        registrar("[%s] mando abierto como SDL_GameController: %s (mapeo %s)", sagui_sdl2_nome, j->info.nome,
                  j->info.mapa == MANDO_MAPA_XBOX360 ? "Xbox 360" : j->info.mapa == MANDO_MAPA_DUALACTION ? "Logitech" : "generico");
    j->controlador = 1;
    SDL_GameController *c = &ctls[j - joys];
    c->j = j;
    return c;
}

void SDL_GameControllerClose(SDL_GameController *c) { if (c && c->j) SDL_JoystickClose(c->j); }
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *c) { return c ? c->j : NULL; }

SDL_GameController *SDL_GameControllerFromInstanceID(SDL_JoystickID id)
{
    SDL_Joystick *j = joy_por_id(id);
    return j && j->controlador ? &ctls[j - joys] : NULL;
}

SDL_bool SDL_GameControllerGetAttached(SDL_GameController *c) { return c && c->j ? SDL_JoystickGetAttached(c->j) : SDL_FALSE; }
const char *SDL_GameControllerName(SDL_GameController *c) { return c && c->j ? c->j->info.nome : NULL; }
const char *SDL_GameControllerNameForIndex(int i) { return SDL_IsGameController(i) ? SDL_JoystickNameForIndex(i) : NULL; }

static Sint16 ctl_eixo(const SDL_Joystick *j, const estado_mando_t *e, SDL_GameControllerAxis a)
{
    mapa_t p;
    mapa_de(&j->info, &p);
    if (a < 0 || a >= SDL_CONTROLLER_AXIS_MAX) return 0;
    bool gatilho = a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
    if (gatilho && p.gatilho_botao[a - SDL_CONTROLLER_AXIS_TRIGGERLEFT] >= 0)
        return ((e->botoes >> p.gatilho_botao[a - SDL_CONTROLLER_AXIS_TRIGGERLEFT]) & 1) ? 32767 : 0;
    int k = p.eixo[a];
    if (k < 0 || k >= j->info.n_eixos) return 0;
    int v = e->eixos[k];
    if (gatilho && p.gatilho_eixo) v = (v + 32768) / 2;   /* los gatillos van de 0 a 32767 */
    return (Sint16)v;
}

static Uint8 ctl_botao(const SDL_Joystick *j, const estado_mando_t *e, SDL_GameControllerButton b)
{
    if (b < 0 || b >= SDL_CONTROLLER_BUTTON_MAX) return 0;
    if (b >= SDL_CONTROLLER_BUTTON_DPAD_UP) {
        unsigned h = j->info.n_hats ? e->hats[0] : 0;
        static const unsigned bit[4] = { SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT };
        return (h & bit[b - SDL_CONTROLLER_BUTTON_DPAD_UP]) ? 1 : 0;
    }
    mapa_t p;
    mapa_de(&j->info, &p);
    int k = p.botao[b];
    return k >= 0 && k < j->info.n_botoes ? (e->botoes >> k) & 1 : 0;
}

Sint16 SDL_GameControllerGetAxis(SDL_GameController *c, SDL_GameControllerAxis e) { return c && c->j ? ctl_eixo(c->j, &c->j->est, e) : 0; }
Uint8 SDL_GameControllerGetButton(SDL_GameController *c, SDL_GameControllerButton b) { return c && c->j ? ctl_botao(c->j, &c->j->est, b) : 0; }

char *SDL_GameControllerMapping(SDL_GameController *c)
{
    static char t[160];
    if (!c || !c->j) return NULL;
    formatar_texto(t, sizeof t, "%s,%s", c->j->info.nome,
                   c->j->info.mapa == MANDO_MAPA_XBOX360 ? "xbox360" : c->j->info.mapa == MANDO_MAPA_DUALACTION ? "logitech" : "generico");
    return t;
}
int SDL_GameControllerAddMapping(const char *m) { (void)m; return 0; }          /* los mapeos son fijos */
int SDL_GameControllerAddMappingsFromFile(const char *a) { (void)a; return 0; }

int SDL_GameControllerRumble(SDL_GameController *c, Uint16 b, Uint16 a, Uint32 ms)
{
    (void)c; (void)b; (void)a; (void)ms;
    return SDL_SetError("el mando no vibra");
}
const char *SDL_GameControllerGetStringForButton(SDL_GameControllerButton b)
{
    static const char *const n[] = { "a", "b", "x", "y", "back", "guide", "start", "leftstick", "rightstick",
                                     "leftshoulder", "rightshoulder", "dpup", "dpdown", "dpleft", "dpright" };
    return b >= 0 && b < SDL_CONTROLLER_BUTTON_MAX ? n[b] : NULL;
}
const char *SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis e)
{
    static const char *const n[] = { "leftx", "lefty", "rightx", "righty", "lefttrigger", "righttrigger" };
    return e >= 0 && e < SDL_CONTROLLER_AXIS_MAX ? n[e] : NULL;
}
int SDL_GameControllerEventState(int e)
{
    if (e == SDL_ENABLE || e == SDL_IGNORE) ctl_ev = e;
    return ctl_ev;
}
int SDL_JoystickEventState(int e)
{
    if (e == SDL_ENABLE || e == SDL_IGNORE) joy_ev = e;
    return joy_ev;
}

/* Conexiones y desconexiones: compara la lista con los ya avisados. */
static void joy_avisos(void)
{
    if (!(sub_sistemas & (SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER))) { joy_avisos_ligados = 0; return; }
    unsigned agora = tempo_ms();
    if (joy_avisos_ligados && agora - joy_ultima_lista < 250) return;
    joy_ultima_lista = agora;
    unsigned gen_antes = joy_gen;
    joy_listar();
    if (joy_avisos_ligados && joy_gen == gen_antes) return;
    joy_avisos_ligados = 1;
    SDL_Event ev;
    /* Los que se fueron. */
    for (int k = 0; k < joy_n_avisados;) {
        SDL_JoystickID id = joy_avisados[k];
        if (joy_indice_de(id) >= 0) { k++; continue; }
        SDL_Joystick *j = joy_por_id(id);
        if (j) { j->ligado = 0; memset(&j->est, 0, sizeof j->est); }
        if (joy_ev == SDL_ENABLE) { memset(&ev, 0, sizeof ev); ev.type = SDL_JOYDEVICEREMOVED; ev.jdevice.which = id; enfileirar(&ev); }
        if (ctl_ev == SDL_ENABLE && (sub_sistemas & SDL_INIT_GAMECONTROLLER)) {
            memset(&ev, 0, sizeof ev); ev.type = SDL_CONTROLLERDEVICEREMOVED; ev.cdevice.which = id; enfileirar(&ev);
        }
        joy_avisados[k] = joy_avisados[--joy_n_avisados];
    }
    /* Los nuevos (al empezar, todos). */
    for (int i = 0; i < joy_n; i++) {
        SDL_JoystickID id = (SDL_JoystickID)joy_lista[i].id;
        bool ja = false;
        for (int k = 0; k < joy_n_avisados; k++) if (joy_avisados[k] == id) ja = true;
        if (ja || joy_n_avisados >= JOY_MAX) continue;
        joy_avisados[joy_n_avisados++] = id;
        if (joy_ev == SDL_ENABLE) { memset(&ev, 0, sizeof ev); ev.type = SDL_JOYDEVICEADDED; ev.jdevice.which = i; enfileirar(&ev); }
        if (ctl_ev == SDL_ENABLE && (sub_sistemas & SDL_INIT_GAMECONTROLLER) && SDL_IsGameController(i)) {
            memset(&ev, 0, sizeof ev); ev.type = SDL_CONTROLLERDEVICEADDED; ev.cdevice.which = i; enfileirar(&ev);
        }
    }
}

/* Lee los abiertos y avisa de los cambios. */
static void joy_atualizar(int forcar)
{
    int alguno = (sub_sistemas & (SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER)) != 0;
    for (int k = 0; k < JOY_MAX && !alguno; k++) alguno = joys[k].abertos;
    if (!alguno) return;   /* nadie usa mandos: no se pregunta nada al sistema */
    unsigned agora = tempo_ms();
    joy_avisos();
    if (!forcar && agora - joy_ultimo < 4) return;
    joy_ultimo = agora;
    for (int k = 0; k < JOY_MAX; k++) {
        SDL_Joystick *j = &joys[k];
        if (!j->abertos || !j->ligado) continue;
        int i = joy_indice_de(j->id);
        estado_mando_t e;
        if (i < 0 || mando_ler(i, &e) < 0 || (SDL_JoystickID)e.id != j->id) continue;
        SDL_Event ev;
        if (joy_ev == SDL_ENABLE) {
            for (int a = 0; a < j->info.n_eixos && a < MANDO_EIXOS_MAX; a++)
                if (e.eixos[a] != j->est.eixos[a]) {
                    memset(&ev, 0, sizeof ev);
                    ev.jaxis.type = SDL_JOYAXISMOTION; ev.jaxis.timestamp = agora; ev.jaxis.which = j->id;
                    ev.jaxis.axis = (Uint8)a; ev.jaxis.value = e.eixos[a];
                    enfileirar(&ev);
                }
            for (int h = 0; h < j->info.n_hats && h < MANDO_HATS_MAX; h++)
                if (e.hats[h] != j->est.hats[h]) {
                    memset(&ev, 0, sizeof ev);
                    ev.jhat.type = SDL_JOYHATMOTION; ev.jhat.timestamp = agora; ev.jhat.which = j->id;
                    ev.jhat.hat = (Uint8)h; ev.jhat.value = e.hats[h];
                    enfileirar(&ev);
                }
            Uint32 mudou = e.botoes ^ j->est.botoes;
            for (int b = 0; b < j->info.n_botoes && b < 32; b++)
                if (mudou & (1u << b)) {
                    memset(&ev, 0, sizeof ev);
                    bool p = (e.botoes >> b) & 1;
                    ev.jbutton.type = p ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP; ev.jbutton.timestamp = agora;
                    ev.jbutton.which = j->id; ev.jbutton.button = (Uint8)b; ev.jbutton.state = p ? SDL_PRESSED : SDL_RELEASED;
                    enfileirar(&ev);
                }
        }
        if (j->controlador && ctl_ev == SDL_ENABLE) {
            for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
                Sint16 v = ctl_eixo(j, &e, (SDL_GameControllerAxis)a);
                if (v == ctl_eixo(j, &j->est, (SDL_GameControllerAxis)a)) continue;
                memset(&ev, 0, sizeof ev);
                ev.caxis.type = SDL_CONTROLLERAXISMOTION; ev.caxis.timestamp = agora; ev.caxis.which = j->id;
                ev.caxis.axis = (Uint8)a; ev.caxis.value = v;
                enfileirar(&ev);
            }
            for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
                Uint8 v = ctl_botao(j, &e, (SDL_GameControllerButton)b);
                if (v == ctl_botao(j, &j->est, (SDL_GameControllerButton)b)) continue;
                memset(&ev, 0, sizeof ev);
                ev.cbutton.type = v ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP; ev.cbutton.timestamp = agora;
                ev.cbutton.which = j->id; ev.cbutton.button = (Uint8)b; ev.cbutton.state = v ? SDL_PRESSED : SDL_RELEASED;
                enfileirar(&ev);
                static int avisados;
                if (avisados < 4) {   /* para el registro de las pruebas: las primeras pulsaciones */
                    avisados++;
                    registrar("[%s] mando: boton %s %s", sagui_sdl2_nome, SDL_GameControllerGetStringForButton((SDL_GameControllerButton)b),
                              v ? "pulsado" : "suelto");
                }
            }
        }
        j->est = e;
    }
}

void SDL_JoystickUpdate(void) { joy_atualizar(1); }
void SDL_GameControllerUpdate(void) { joy_atualizar(1); }

/* ------------------------------------------------------------------ mensajes, portapapeles, rutas */

int SDL_ShowSimpleMessageBox(Uint32 flags, const char *titulo, const char *msg, SDL_Window *w)
{
    (void)flags; (void)w;
    registrar("[%s] mensaje: %s: %s", sagui_sdl2_nome, titulo ? titulo : "", msg ? msg : "");
    mensagem(titulo ? titulo : sagui_sdl2_nome, msg ? msg : "");
    return 0;
}

/* Una caja con hasta 4 botones: la ventana de la libsagui a mano (dialogo() de gui.c solo
 * tiene dos). Devuelve en *botao el buttonid del pulsado; si se cierra la ventana o se pulsa
 * Esc, el del botón ESCAPEKEY_DEFAULT (o -1); Intro, el de RETURNKEY_DEFAULT (o el primero). */
int SDL_ShowMessageBox(const SDL_MessageBoxData *d, int *botao)
{
    if (!d || d->numbuttons < 1 || !d->buttons) return SDL_SetError("SDL_ShowMessageBox: sin botones");
    int n = d->numbuttons > 4 ? 4 : d->numbuttons;
    const char *msg = d->message ? d->message : "";
    int lin = 1;
    for (const char *t = msg; *t; t++) if (*t == '\n') lin++;
    int alt = 76 + lin * 16, larg = 380;
    registrar("[%s] caja de mensaje: %s: %s", sagui_sdl2_nome, d->title ? d->title : "", msg);
    jan_t *j = jan_abrir(d->title ? d->title : sagui_sdl2_nome, larg, alt);
    if (!j) { if (botao) *botao = -1; return SDL_SetError("no se puede abrir la ventana"); }
    jan_estilo(j, JAN_DIALOGO, 0, 0);
    botao_t bs[4];
    int esc = -1, intro = d->buttons[0].buttonid;
    for (int i = 0; i < n; i++) {
        /* de derecha a izquierda, en el orden en que se dan (SDL pone el primero a la izquierda) */
        bs[i] = (botao_t){ larg - 16 - 90 * (n - i) - 6 * (n - 1 - i), alt - 40, 90, 26, d->buttons[i].text, false };
        if (d->buttons[i].flags & SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT) esc = d->buttons[i].buttonid;
        if (d->buttons[i].flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) intro = d->buttons[i].buttonid;
    }
    int resp = -2;
    for (;;) {
        g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
        char linha[200];
        int y = 16;
        for (const char *t = msg; ; ) {
            size_t k = 0;
            while (t[k] && t[k] != '\n' && k < sizeof linha - 1) k++;
            memcpy(linha, t, k);
            linha[k] = 0;
            rotulo(j, 16, y, linha, COR_TEXTO);
            y += 16;
            t += k;
            if (*t == '\n') t++; else break;
        }
        for (int i = 0; i < n; i++) botao_desenhar(j, &bs[i]);
        jan_mostrar(j);
        evento_t e;
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) { resp = esc; break; }
        if (e.tipo == EV_TECLA && e.tecla == '\n') { resp = intro; break; }
        for (int i = 0; i < n; i++)
            if (botao_tratar(j, &bs[i], &e)) resp = d->buttons[i].buttonid;
        if (resp != -2) break;
    }
    jan_fechar(j);
    if (botao) *botao = resp;
    return 0;
}

/* BMP sin compresión de 24 bits (de arriba abajo no: el formato va de abajo arriba). */
int SDL_SaveBMP(SDL_Surface *s, const char *nome)
{
    if (!s || !s->pixels || !nome) return SDL_SetError("SDL_SaveBMP: superficie o nombre vacíos");
    int l = s->w, a = s->h, fila = (l * 3 + 3) & ~3;
    FILE *f = fopen(nome, "wb");
    if (!f) return SDL_SetError("no se puede crear %s", nome);
    Uint8 cab[54];
    memset(cab, 0, sizeof cab);
    Uint32 tam = 54 + (Uint32)fila * (Uint32)a;
    cab[0] = 'B'; cab[1] = 'M';
    for (int i = 0; i < 4; i++) cab[2 + i] = (Uint8)(tam >> (8 * i));
    cab[10] = 54; cab[14] = 40;
    for (int i = 0; i < 4; i++) { cab[18 + i] = (Uint8)((Uint32)l >> (8 * i)); cab[22 + i] = (Uint8)((Uint32)a >> (8 * i)); }
    cab[26] = 1; cab[28] = 24;
    for (int i = 0; i < 4; i++) cab[34 + i] = (Uint8)(((Uint32)fila * (Uint32)a) >> (8 * i));
    fwrite(cab, 1, sizeof cab, f);
    Uint8 *lin = calloc(1, (size_t)fila);
    if (!lin) { fclose(f); return SDL_SetError("sin memoria"); }
    int bpp = s->format->BytesPerPixel;
    for (int y = a - 1; y >= 0; y--) {
        const Uint8 *o = (const Uint8 *)s->pixels + (size_t)y * (size_t)s->pitch;
        for (int x = 0; x < l; x++) {
            Uint32 px = bpp == 1 ? o[x] : ((const Uint32 *)o)[x];
            Uint8 r, g, b;
            SDL_GetRGB(px, s->format, &r, &g, &b);
            lin[x * 3] = b; lin[x * 3 + 1] = g; lin[x * 3 + 2] = r;
        }
        fwrite(lin, 1, (size_t)fila, f);
    }
    free(lin);
    fclose(f);
    return 0;
}

char *SDL_GetClipboardText(void)
{
    char *b = malloc(4096);
    if (!b) return NULL;
    if (colar_texto(b, 4096) < 0) b[0] = 0;
    return b;
}
int SDL_SetClipboardText(const char *t) { return t ? (copiar_texto(t, (unsigned)strlen(t)) >= 0 ? 0 : -1) : -1; }
SDL_bool SDL_HasClipboardText(void)
{
    char b[2];
    return colar_texto(b, sizeof b) > 0 ? SDL_TRUE : SDL_FALSE;
}

/* Los programas portados viven en /apps/bin y guardan en /home/<programa>. */
char *SDL_GetBasePath(void) { return SDL_strdup("/apps/bin/"); }
char *SDL_GetPrefPath(const char *org, const char *app)
{
    (void)org;
    char c[160];
    snprintf(c, sizeof c, "/home/%s", app && *app ? app : "sdl");
    for (char *p = c + 6; *p; p++) *p = (char)tolower((unsigned char)*p);
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) < 0) criar_pasta(c);
    size_t n = strlen(c);
    char *r = malloc(n + 2);
    if (!r) return NULL;
    memcpy(r, c, n);
    r[n] = '/';
    r[n + 1] = 0;
    return r;
}

/* ------------------------------------------------------------------ inicio */

int SDL_Init(Uint32 flags) { sub_sistemas |= flags; return 0; }
int SDL_InitSubSystem(Uint32 flags) { sub_sistemas |= flags; return 0; }
void SDL_QuitSubSystem(Uint32 flags)
{
    sub_sistemas &= ~flags;
    if ((flags & SDL_INIT_AUDIO) && audio_aberto) SDL_CloseAudio();
}
Uint32 SDL_WasInit(Uint32 flags) { return flags ? (sub_sistemas & flags) : sub_sistemas; }
void SDL_SetMainReady(void) {}

void SDL_Quit(void)
{
    if (audio_aberto) SDL_CloseAudio();
    if (janela) SDL_DestroyWindow(janela);
    sub_sistemas = 0;
}

/* ------------------------------------------------- hilos (D383, D393) */

/* Hasta la D393 esto eran cerrojos nulos y SDL_CreateThread fallaba. Ahora son de verdad, sobre los
 * hilos del SaguiOS (D167: thread_create, futex, atómicas de la libsagui):
 *   - el cerrojo es recursivo (como el de la SDL): dueño, profundidad y un mutex_t debajo. Los
 *     programas de antes (Chocolate Doom: el audio llamado desde SDL_Delay con el cerrojo ya tomado
 *     en el mismo hilo) siguen igual, porque el mismo hilo lo vuelve a tomar;
 *   - la condición es un contador de avisos y un futex. En el hilo principal la espera se corta cada
 *     2 ms para mover el audio (que solo se mueve en el principal): quien esperaba a que la llamada
 *     de vuelta del audio le avisara (OPL_Delay de Chocolate Doom) sigue funcionando sin otro hilo;
 *   - el semáforo es el de la libsagui con espera con plazo;
 *   - SDL_GetCPUCount cuenta las CPU arrancadas (INFO_NUCLEOS, D157): sin --smp, una. */

static int pid_proceso;  /* el tid del hilo principal es el pid del proceso */

static int hilo_actual(void) { return thread_id(); }
static int es_principal(void)
{
    if (!hay_hilos) return 1;
    if (!pid_proceso) pid_proceso = meu_pid();
    return thread_id() == pid_proceso;
}

struct SDL_mutex { mutex_t m; volatile uint32_t dono; int prof; };
struct SDL_cond { volatile uint32_t avisos; volatile uint32_t esperando; };

SDL_mutex *SDL_CreateMutex(void)
{
    SDL_mutex *m = calloc(1, sizeof(SDL_mutex));
    if (m) mutex_criar(&m->m);
    return m;
}

int SDL_LockMutex(SDL_mutex *m)
{
    if (!m) return SDL_SetError("cerrojo nulo");
    uint32_t eu = (uint32_t)hilo_actual();
    if (m->prof > 0 && m->dono == eu) { m->prof++; return 0; }
    mutex_tomar(&m->m);
    m->dono = eu;
    m->prof = 1;
    return 0;
}

int SDL_TryLockMutex(SDL_mutex *m)
{
    if (!m) return SDL_SetError("cerrojo nulo");
    uint32_t eu = (uint32_t)hilo_actual();
    if (m->prof > 0 && m->dono == eu) { m->prof++; return 0; }
    if (!mutex_tentar(&m->m)) return SDL_MUTEX_TIMEDOUT;
    m->dono = eu;
    m->prof = 1;
    return 0;
}

int SDL_UnlockMutex(SDL_mutex *m)
{
    if (!m) return SDL_SetError("cerrojo nulo");
    if (m->prof <= 0 || m->dono != (uint32_t)hilo_actual()) return SDL_SetError("el cerrojo no es de este hilo");
    if (--m->prof == 0) { m->dono = 0; mutex_largar(&m->m); }
    return 0;
}

void SDL_DestroyMutex(SDL_mutex *m) { free(m); }

SDL_cond *SDL_CreateCond(void) { return calloc(1, sizeof(SDL_cond)); }
void SDL_DestroyCond(SDL_cond *c) { free(c); }

int SDL_CondSignal(SDL_cond *c)
{
    if (!c) return SDL_SetError("condición nula");
    atomico_inc32(&c->avisos);
    if (c->esperando) futex_acordar(&c->avisos, 1);
    return 0;
}

int SDL_CondBroadcast(SDL_cond *c)
{
    if (!c) return SDL_SetError("condición nula");
    atomico_inc32(&c->avisos);
    if (c->esperando) futex_acordar(&c->avisos, 0x7fffffff);
    return 0;
}

/* Suelta el cerrojo (del todo, aunque esté tomado varias veces), espera un aviso o el plazo y lo
 * vuelve a tomar con la misma profundidad. ms = SDL_MUTEX_MAXWAIT: sin plazo. */
int SDL_CondWaitTimeout(SDL_cond *c, SDL_mutex *m, Uint32 ms)
{
    if (!c || !m) return SDL_SetError("condición o cerrojo nulos");
    uint32_t eu = (uint32_t)hilo_actual();
    if (m->prof <= 0 || m->dono != eu) return SDL_SetError("el cerrojo no es de este hilo");
    int prof = m->prof, principal = es_principal(), r = 0;
    atomico_inc32(&c->esperando);
    uint32_t visto = c->avisos;   /* tras la atómica: un aviso posterior cambia el valor y el futex no duerme */
    m->prof = 0; m->dono = 0;
    mutex_largar(&m->m);
    if (principal) {
        /* el principal mueve el audio cada 2 ms; vuelve con el primer aviso o al acabar el plazo */
        unsigned fim = tempo_ms() + (ms == SDL_MUTEX_MAXWAIT ? 0u : (unsigned)ms);
        for (;;) {
            unsigned trozo = 2;
            if (ms != SDL_MUTEX_MAXWAIT) {
                int falta = (int)(fim - tempo_ms());
                if (falta <= 0) { r = SDL_MUTEX_TIMEDOUT; break; }
                if ((unsigned)falta < trozo) trozo = (unsigned)falta;
            }
            futex_esperar(&c->avisos, visto, trozo);
            if (c->avisos != visto) break;
            bombear_audio();
            if (c->avisos != visto) break;
            if (ms == SDL_MUTEX_MAXWAIT && !hay_hilos_programa) break;   /* sin hilos del programa nadie avisa: se vuelve a mirar (como antes del hilo de audio) */
        }
    } else if (ms == SDL_MUTEX_MAXWAIT) {
        while (c->avisos == visto) futex_esperar(&c->avisos, visto, 0);
    } else {
        unsigned fim = tempo_ms() + (unsigned)ms;
        while (c->avisos == visto) {
            int falta = (int)(fim - tempo_ms());
            if (falta <= 0) { r = SDL_MUTEX_TIMEDOUT; break; }
            futex_esperar(&c->avisos, visto, (unsigned)falta);
        }
    }
    atomico_dec32(&c->esperando);
    mutex_tomar(&m->m);
    m->dono = eu;
    m->prof = prof;
    return r;
}

int SDL_CondWait(SDL_cond *c, SDL_mutex *m) { return SDL_CondWaitTimeout(c, m, SDL_MUTEX_MAXWAIT); }

struct SDL_semaphore { semaforo_t s; };

SDL_sem *SDL_CreateSemaphore(Uint32 valor)
{
    SDL_sem *s = calloc(1, sizeof(SDL_sem));
    if (s) semaforo_criar(&s->s, valor);
    return s;
}

void SDL_DestroySemaphore(SDL_sem *s) { free(s); }
int SDL_SemTryWait(SDL_sem *s) { if (!s) return SDL_SetError("semáforo nulo"); return semaforo_tentar(&s->s) ? 0 : SDL_MUTEX_TIMEDOUT; }
int SDL_SemPost(SDL_sem *s) { if (!s) return SDL_SetError("semáforo nulo"); semaforo_sinalizar(&s->s); return 0; }
Uint32 SDL_SemValue(SDL_sem *s) { return s ? semaforo_valor(&s->s) : 0; }

int SDL_SemWaitTimeout(SDL_sem *s, Uint32 ms)
{
    if (!s) return SDL_SetError("semáforo nulo");
    if (semaforo_tentar(&s->s)) return 0;
    if (ms == SDL_MUTEX_MAXWAIT && !es_principal()) { semaforo_tomar(&s->s); return 0; }
    /* con plazo, o en el principal (que mueve el audio cada 2 ms) */
    unsigned fim = tempo_ms() + (ms == SDL_MUTEX_MAXWAIT ? 0u : (unsigned)ms);
    for (;;) {
        if (semaforo_tentar(&s->s)) return 0;
        unsigned trozo = es_principal() ? 2u : 20u;
        if (ms != SDL_MUTEX_MAXWAIT) {
            int falta = (int)(fim - tempo_ms());
            if (falta <= 0) return SDL_MUTEX_TIMEDOUT;
            if ((unsigned)falta < trozo) trozo = (unsigned)falta;
        }
        atomico_inc32(&s->s.esperando);
        if (s->s.valor == 0) futex_esperar(&s->s.valor, 0, trozo);
        atomico_dec32(&s->s.esperando);
        if (es_principal()) bombear_audio();
    }
}

int SDL_SemWait(SDL_sem *s) { return SDL_SemWaitTimeout(s, SDL_MUTEX_MAXWAIT); }

/* Atómicas: lock xadd / cmpxchg de la libsagui (barrera completa en x86). */
SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int viejo, int nuevo) { return atomico_cas32((volatile uint32_t *)&a->value, (uint32_t)viejo, (uint32_t)nuevo) ? SDL_TRUE : SDL_FALSE; }
int SDL_AtomicSet(SDL_atomic_t *a, int v) { return (int)atomico_troca32((volatile uint32_t *)&a->value, (uint32_t)v); }
int SDL_AtomicGet(SDL_atomic_t *a) { atomico_barreira(); return *(volatile int *)&a->value; }
int SDL_AtomicAdd(SDL_atomic_t *a, int v) { return (int)atomico_add32((volatile uint32_t *)&a->value, (uint32_t)v); }
void SDL_MemoryBarrierReleaseFunction(void) { atomico_barreira(); }
void SDL_MemoryBarrierAcquireFunction(void) { atomico_barreira(); }

struct SDL_Thread {
    thread_t *t;
    SDL_ThreadFunction fn;
    void *datos;
    int estado;
    char nombre[32];
};

static void *inicio_hilo_sdl(void *arg)
{
    SDL_Thread *h = arg;
    h->estado = h->fn(h->datos);
    return (void *)(long)h->estado;
}

SDL_Thread *SDL_CreateThreadWithStackSize(SDL_ThreadFunction fn, const char *nombre, size_t pila, void *datos)
{
    if (!fn) { SDL_SetError("hilo sin función"); return NULL; }
    SDL_Thread *h = calloc(1, sizeof(SDL_Thread));
    if (!h) { SDL_SetError("sin memoria"); return NULL; }
    h->fn = fn; h->datos = datos;
    sagui_sdl_strlcpy(h->nombre, nombre ? nombre : "SDL", sizeof h->nombre);
    if (!hay_hilos) { hay_hilos = 1; atomico_barreira(); }
    hay_hilos_programa = 1;
    /* pila 0: la del kernel (1 MB con guarda, D186); si se pide más, una propia */
    h->t = thread_create(inicio_hilo_sdl, h, pila > 960u * 1024u ? (unsigned)pila : 0u);
    if (!h->t) { free(h); SDL_SetError("no se pudo crear el hilo"); return NULL; }
    return h;
}

SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *nombre, void *datos)
{
    return SDL_CreateThreadWithStackSize(fn, nombre, 0, datos);
}

void SDL_WaitThread(SDL_Thread *t, int *estado)
{
    if (!t) return;
    thread_join(t->t);
    if (estado) *estado = t->estado;
    free(t);
}

/* Se deja de seguir al hilo: su SDL_Thread queda (unos bytes) porque el hilo puede seguir usándolo. */
void SDL_DetachThread(SDL_Thread *t) { (void)t; }
SDL_threadID SDL_ThreadID(void) { return (SDL_threadID)hilo_actual(); }
SDL_threadID SDL_GetThreadID(SDL_Thread *t) { return t ? (SDL_threadID)thread_tid(t->t) : SDL_ThreadID(); }
const char *SDL_GetThreadName(SDL_Thread *t) { return t ? t->nombre : NULL; }
int SDL_SetThreadPriority(SDL_ThreadPriority prioridad) { (void)prioridad; return 0; }

SDL_JoystickGUID SDL_JoystickGetGUIDFromString(const char *texto)
{
    SDL_JoystickGUID g;
    memset(&g, 0, sizeof g);
    for (int i = 0; i < 16 && texto && texto[2 * i] && texto[2 * i + 1]; i++) {
        char h[3] = { texto[2 * i], texto[2 * i + 1], 0 };
        g.data[i] = (Uint8)strtoul(h, NULL, 16);
    }
    return g;
}

/* ------------------------------------------------ conversión de audio (D383) */

static int fmt_bytes(SDL_AudioFormat f) { return SDL_AUDIO_BITSIZE(f) / 8; }

static double leer_muestra(const Uint8 *p, SDL_AudioFormat f)
{
    switch (f) {
    case AUDIO_U8: return ((int)p[0] - 128) / 128.0;
    case AUDIO_S8: return (Sint8)p[0] / 128.0;
    case AUDIO_U16LSB: return ((int)(p[0] | (p[1] << 8)) - 32768) / 32768.0;
    case AUDIO_S16LSB: return (Sint16)(p[0] | (p[1] << 8)) / 32768.0;
    case AUDIO_S32LSB: return (Sint32)((Uint32)p[0] | ((Uint32)p[1] << 8) | ((Uint32)p[2] << 16) | ((Uint32)p[3] << 24)) / 2147483648.0;
    case AUDIO_F32LSB: { float v; memcpy(&v, p, 4); return v; }
    }
    return 0;
}

static void escribir_muestra(Uint8 *p, SDL_AudioFormat f, double v)
{
    if (v > 0.999969) v = 0.999969;
    if (v < -1.0) v = -1.0;
    switch (f) {
    case AUDIO_U8: p[0] = (Uint8)((int)(v * 128.0) + 128); break;
    case AUDIO_S8: p[0] = (Uint8)(Sint8)(int)(v * 128.0); break;
    case AUDIO_U16LSB: { int x = (int)(v * 32768.0) + 32768; p[0] = x & 255; p[1] = (x >> 8) & 255; break; }
    case AUDIO_S16LSB: { int x = (int)(v * 32768.0); p[0] = x & 255; p[1] = (x >> 8) & 255; break; }
    case AUDIO_S32LSB: { Sint32 x = (Sint32)(v * 2147483648.0); for (int i = 0; i < 4; i++) p[i] = (Uint8)(x >> (8 * i)); break; }
    case AUDIO_F32LSB: { float x = (float)v; memcpy(p, &x, 4); break; }
    }
}

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, SDL_AudioFormat src_fmt, Uint8 src_canales, int src_frec,
                      SDL_AudioFormat dst_fmt, Uint8 dst_canales, int dst_frec)
{
    memset(cvt, 0, sizeof *cvt);
    if (src_frec <= 0 || dst_frec <= 0 || src_canales < 1 || dst_canales < 1 || src_canales > 2 || dst_canales > 2 ||
        !fmt_bytes(src_fmt) || !fmt_bytes(dst_fmt))
        return SDL_SetError("SDL_BuildAudioCVT: parámetros no admitidos (frecuencia > 0, 1 o 2 canales)");
    cvt->src_format = src_fmt; cvt->dst_format = dst_fmt;
    cvt->src_channels = src_canales; cvt->dst_channels = dst_canales;
    cvt->src_rate = src_frec; cvt->dst_rate = dst_frec;
    cvt->needed = !(src_fmt == dst_fmt && src_canales == dst_canales && src_frec == dst_frec);
    cvt->len_ratio = ((double)dst_frec / src_frec) * ((double)dst_canales / src_canales) *
                     ((double)fmt_bytes(dst_fmt) / fmt_bytes(src_fmt));
    cvt->len_mult = (int)(cvt->len_ratio + 0.999999);
    if (cvt->len_mult < 1) cvt->len_mult = 1;
    cvt->rate_incr = (double)dst_frec / src_frec;
    return cvt->needed;
}

/* El búfer cvt->buf tiene len * len_mult bytes; se convierte en el sitio (por un búfer temporal). */
int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    if (!cvt->needed) { cvt->len_cvt = cvt->len; return 0; }
    int sb = fmt_bytes(cvt->src_format), db = fmt_bytes(cvt->dst_format);
    int sc = cvt->src_channels, dc = cvt->dst_channels;
    int n_src = cvt->len / (sb * sc);
    long n_dst = (long)((double)n_src * cvt->dst_rate / cvt->src_rate);
    if (n_dst * db * dc > (long)cvt->len * cvt->len_mult) n_dst = (long)cvt->len * cvt->len_mult / (db * dc);
    Uint8 *tmp = malloc((size_t)(n_dst * db * dc) + 8);
    if (!tmp) return -1;
    for (long i = 0; i < n_dst; i++) {
        /* interpolación lineal entre las muestras vecinas de la entrada */
        double pos = (double)i * cvt->src_rate / cvt->dst_rate;
        long i0 = (long)pos;
        double fr = pos - i0;
        long i1 = i0 + 1 < n_src ? i0 + 1 : n_src - 1;
        if (i0 >= n_src) i0 = n_src - 1;
        for (int c = 0; c < dc; c++) {
            int cs = sc == dc ? c : (sc == 1 ? 0 : (c < sc ? c : sc - 1));
            double a = leer_muestra(cvt->buf + (i0 * sc + cs) * sb, cvt->src_format);
            double b = leer_muestra(cvt->buf + (i1 * sc + cs) * sb, cvt->src_format);
            double v = a + (b - a) * fr;
            if (sc > dc && dc == 1) {   /* de estéreo a mono: media */
                double a2 = leer_muestra(cvt->buf + (i0 * sc + 1) * sb, cvt->src_format);
                double b2 = leer_muestra(cvt->buf + (i1 * sc + 1) * sb, cvt->src_format);
                v = (v + a2 + (b2 - a2) * fr) * 0.5;
            }
            escribir_muestra(tmp + (i * dc + c) * db, cvt->dst_format, v);
        }
    }
    cvt->len_cvt = (int)(n_dst * db * dc);
    memcpy(cvt->buf, tmp, (size_t)cvt->len_cvt);
    free(tmp);
    return 0;
}

int SDL_LowerBlit(SDL_Surface *o, SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd) { return SDL_UpperBlit(o, ro, d, rd); }
int SDL_GetWindowDisplayIndex(SDL_Window *w) { (void)w; return 0; }
int SDL_GameControllerTypeForIndex(int indice) { (void)indice; return SDL_CONTROLLER_TYPE_UNKNOWN; }
