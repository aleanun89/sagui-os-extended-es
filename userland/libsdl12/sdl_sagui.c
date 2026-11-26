/* sdl_sagui.c - la SDL 1.2 del SaguiOS para los juegos portados (D228).
 *
 * Generaliza la capa del OpenTyrian (D156, D204) para cualquier port:
 *
 *   - vídeo:    la superficie de la pantalla es memoria propia, de 8 bits con
 *               paleta (o de 32); SDL_Flip la convierte (por la paleta
 *               «física») y la copia escalada a la ventana. En modo ventana
 *               la ventana es la superficie por sagui_sdl_escala (2 por
 *               defecto, para que 320x200 no salga diminuto); a pantalla
 *               completa (SDL_FULLSCREEN, F11, o el propio juego) se escala
 *               a 4:3 con bandas negras, como la VGA (D204).
 *   - blits:    entre superficies de 8 bits (copia de índices, con color
 *               clave y recorte) y de 32 bits; de 8 a 32 por la paleta.
 *               Las paletas «lógicas» de las superficies no se aplican a la
 *               copia (los juegos de 8 bits usan la misma en todas).
 *   - teclado:  EV_TECLA -> SDL_KEYDOWN; el kernel no avisa de cuándo se
 *               suelta una tecla, así que se mira teclas_seguradas() para
 *               fabricar los SDL_KEYUP. Las teclas modificadoras (Alt, Ctrl,
 *               Mayús) no dan EV_TECLA: también se sacan de ese mapa, con su
 *               KEYDOWN y su KEYUP (los juegos disparan con el Alt).
 *   - ratón:    EV_MOVER, EV_CLIQUE y EV_SOLTOU, en coordenadas de la superficie.
 *   - reloj:    SDL_GetTicks y SDL_Delay sobre tempo_ms() y dormir().
 *   - audio:    el juego mezcla en un callback; aquí se le llama cuando cabe
 *               otro bloque en el flujo de sonido del sistema (en cada
 *               SDL_PollEvent, SDL_Delay y SDL_Flip). Sin placa, mudo.
 *   - joystick (D307): los mandos del sistema por sondeo, con sus eventos (libsagui mandos.h).
 *
 * Lo que es solo del SaguiOS (sagui_sdl_nome, sagui_sdl_titulo, sagui_sdl_escala,
 * sagui_cheia_inicial, sagui_sdl_ao_mudar_cheia) lo ajusta el programa antes
 * de abrir el vídeo. Es código nuestro: de la SDL solo está la interfaz.
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

#define TAM_FILA   128   /* eventos en cola */
#define TAM_TECLAS 64    /* teclas vigiladas para saber cuándo se sueltan */

const char *sagui_sdl_nome = "SDL";
const char *sagui_sdl_titulo = NULL;
int  sagui_sdl_escala = 2;
int  sagui_cheia_inicial = 0;
void (*sagui_sdl_ao_mudar_cheia)(int cheia) = NULL;
int  sagui_sdl_foco_ao_soltar = 0;

/* ---- la ventana ---- */
/* D230: SDL_ShowCursor(SDL_DISABLE) oculta el puntero del sistema sobre la ventana
 * (JAN_SEM_CURSOR); va en cada estilo que se pide, para no perderlo. */
static int cursor_oculto = 0;
#define ESTILO_BASE (JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK | JAN_ESC_LIVRE | (cursor_oculto ? JAN_SEM_CURSOR : 0))
static int janela_id = -1;
static uint32_t *janela_px = NULL;
static int janela_l = 0, janela_a = 0;
static int em_cheia = 0;

/* ---- la pantalla ---- */
static SDL_Surface *tela = NULL;          /* la superficie que ve el juego */
static SDL_Color paleta_fisica[256];      /* la que se ve (SDL_PHYSPAL) */
static uint32_t paleta32[256];            /* la misma, en 0xRRGGBB */
static int dest_x, dest_y, dest_l, dest_a;   /* dónde va la superficie en la ventana */
static int *tab_col = NULL, *tab_fila = NULL;
static int tab_col_n = 0, tab_fila_n = 0;
static int primeiro_quadro = 0;

/* ---- eventos ---- */
static SDL_Event fila[TAM_FILA];
static int fila_ini = 0, fila_n = 0;
static SDLMod mods_atual = KMOD_NONE;
static Uint32 sub_sistemas = 0;
static int rato_x = 0, rato_y = 0;
static Uint8 rato_botoes = 0;
static Uint8 estado_teclas[SDLK_LAST];

typedef struct {
    SDLKey sim;
    unsigned char sc;     /* código de rastreo (conjunto 1); 128+ = tecla extendida */
    unsigned char down;
    unsigned desde;       /* tempo_ms() del último KEYDOWN */
} tecla_vigiada_t;

/* Una pulsación dura como poco esto antes de su KEYUP. La tecla que manda el
 * QEMU (o una pulsación muy rápida) se suelta antes de que el juego lea los
 * eventos: si el KEYDOWN y el KEYUP llegan en la misma vuelta de su bucle, los
 * juegos que miran el estado de las teclas una vez por cuadro (OpenJazz) no
 * la ven nunca. Con una SDL de verdad el KEYUP llega cuando se suelta. */
#define PULSACION_MIN_MS 80
static tecla_vigiada_t teclas[TAM_TECLAS];
static int n_teclas = 0;

/* Las modificadoras: no llegan como EV_TECLA, solo en el mapa. */
static const struct { SDLKey sim; unsigned char sc; SDLMod mod; } modificadoras[] = {
    { SDLK_LALT,   0x38,        KMOD_LALT },
    { SDLK_RALT,   0x80 | 0x38, KMOD_RALT },
    { SDLK_LCTRL,  0x1D,        KMOD_LCTRL },
    { SDLK_RCTRL,  0x80 | 0x1D, KMOD_RCTRL },
    { SDLK_LSHIFT, 0x2A,        KMOD_LSHIFT },
    { SDLK_RSHIFT, 0x36,        KMOD_RSHIFT },
};
#define N_MODIF ((int)(sizeof modificadoras / sizeof modificadoras[0]))
static unsigned char modif_down[N_MODIF];

/* ---- audio ---- */
static SDL_AudioSpec audio_spec;
static int audio_fd = -1;
static int audio_pausado = 1, audio_aberto = 0;
static unsigned char audio_buf[8192];
static unsigned audio_bloco = sizeof audio_buf;

static void bombear_audio(void);
static void eventos_do_sistema(void);

/* ------------------------------------------------------------------ general */

static char erro[128] = "error de la SDL del SaguiOS";

const char *SDL_GetError(void) { return erro; }
void SDL_ClearError(void) { erro[0] = 0; }
void SDL_SetError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(erro, sizeof erro, fmt, ap);
    va_end(ap);
}

char *SDL_getenv(const char *nome) { return getenv(nome); }
void *SDL_malloc(size_t n) { return malloc(n); }
void  SDL_free(void *p) { free(p); }

const SDL_version *SDL_Linked_Version(void)
{
    static SDL_version v = { SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL };
    return &v;
}

int SDL_Init(Uint32 flags) { sub_sistemas |= flags; return 0; }
int SDL_InitSubSystem(Uint32 flags) { sub_sistemas |= flags; return 0; }
void SDL_QuitSubSystem(Uint32 flags) { sub_sistemas &= ~flags; }
Uint32 SDL_WasInit(Uint32 flags) { return flags ? (sub_sistemas & flags) : sub_sistemas; }

void SDL_Quit(void)
{
    SDL_CloseAudio();
    if (janela_id >= 0) { fechar_janela(janela_id); janela_id = -1; janela_px = NULL; }
    sub_sistemas = 0;
}

/* ------------------------------------------------------------------ superficies */

static void formato_preparar(SDL_PixelFormat *f, int bits)
{
    memset(f, 0, sizeof *f);
    f->BitsPerPixel = (Uint8)bits;
    f->BytesPerPixel = (Uint8)((bits + 7) / 8);
    if (bits == 32) {
        f->Rshift = 16; f->Gshift = 8; f->Bshift = 0;
        f->Rmask = 0xFF0000u; f->Gmask = 0x00FF00u; f->Bmask = 0x0000FFu;
    }
    f->alpha = 255;
}

static SDL_Surface *superficie_nova(int largura, int altura, int bits, void *pixels, int pitch)
{
    if (largura <= 0 || altura <= 0 || (bits != 8 && bits != 16 && bits != 32)) {
        SDL_SetError("superficie %dx%d de %d bits no admitida", largura, altura, bits);
        return NULL;
    }
    SDL_Surface *s = calloc(1, sizeof *s);
    SDL_PixelFormat *f = calloc(1, sizeof *f);
    if (!s || !f) { free(s); free(f); SDL_SetError("sin memoria"); return NULL; }
    formato_preparar(f, bits);
    if (bits == 8) {
        SDL_Palette *p = calloc(1, sizeof *p);
        SDL_Color *c = calloc(256, sizeof *c);
        if (!p || !c) { free(p); free(c); free(f); free(s); SDL_SetError("sin memoria"); return NULL; }
        for (int i = 0; i < 256; i++) c[i].r = c[i].g = c[i].b = (Uint8)i;
        p->ncolors = 256;
        p->colors = c;
        f->palette = p;
    }
    s->format = f;
    s->w = largura;
    s->h = altura;
    s->refcount = 1;
    if (pixels) {
        s->pixels = pixels;
        s->pitch = (Uint16)pitch;
        s->flags = SDL_PREALLOC;
    } else {
        int p = (largura * f->BytesPerPixel + 3) & ~3;   /* la SDL alinea la fila a 4 */
        s->pitch = (Uint16)p;
        s->pixels = calloc((size_t)p, (size_t)altura);
        if (!s->pixels) { SDL_FreeSurface(s); SDL_SetError("sin memoria"); return NULL; }
    }
    s->clip_rect.x = 0;
    s->clip_rect.y = 0;
    s->clip_rect.w = largura;
    s->clip_rect.h = altura;
    return s;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int largura, int altura, int bits,
                                  Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask)
{
    (void)flags; (void)rmask; (void)gmask; (void)bmask; (void)amask;
    return superficie_nova(largura, altura, bits, NULL, 0);
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int largura, int altura, int bits, int pitch,
                                      Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask)
{
    (void)rmask; (void)gmask; (void)bmask; (void)amask;
    return superficie_nova(largura, altura, bits, pixels, pitch);
}

void SDL_FreeSurface(SDL_Surface *s)
{
    if (!s || s == tela) return;
    if (--s->refcount > 0) return;
    if (!(s->flags & SDL_PREALLOC)) free(s->pixels);
    if (s->format) {
        if (s->format->palette) { free(s->format->palette->colors); free(s->format->palette); }
        free(s->format);
    }
    free(s);
}

int  SDL_LockSurface(SDL_Surface *s) { if (s) s->locked++; return 0; }
void SDL_UnlockSurface(SDL_Surface *s) { if (s && s->locked) s->locked--; }

/* La intersección de a y b en a; 0 si queda vacía. */
static int intersectar(SDL_Rect *a, const SDL_Rect *b)
{
    int x0 = a->x > b->x ? a->x : b->x;
    int y0 = a->y > b->y ? a->y : b->y;
    int x1 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
    int y1 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;
    a->x = x0;
    a->y = y0;
    a->w = x1 > x0 ? x1 - x0 : 0;
    a->h = y1 > y0 ? y1 - y0 : 0;
    return a->w > 0 && a->h > 0;
}

SDL_bool SDL_SetClipRect(SDL_Surface *s, const SDL_Rect *rect)
{
    if (!s) return SDL_FALSE;
    SDL_Rect todo = { 0, 0, s->w, s->h };
    if (!rect) { s->clip_rect = todo; return SDL_TRUE; }
    SDL_Rect r = *rect;
    if (!intersectar(&r, &todo)) { s->clip_rect = r; return SDL_FALSE; }
    s->clip_rect = r;
    return SDL_TRUE;
}

void SDL_GetClipRect(SDL_Surface *s, SDL_Rect *rect)
{
    if (s && rect) *rect = s->clip_rect;
}

int SDL_SetColorKey(SDL_Surface *s, Uint32 flag, Uint32 chave)
{
    if (!s) return -1;
    if (flag & SDL_SRCCOLORKEY) { s->flags |= SDL_SRCCOLORKEY; s->format->colorkey = chave; }
    else s->flags &= ~SDL_SRCCOLORKEY;
    return 0;
}

int SDL_SetAlpha(SDL_Surface *s, Uint32 flag, Uint8 alfa)
{
    if (!s) return -1;
    s->format->alpha = alfa;
    if (flag & SDL_SRCALPHA) s->flags |= SDL_SRCALPHA; else s->flags &= ~SDL_SRCALPHA;
    return 0;
}

int SDL_FillRect(SDL_Surface *d, SDL_Rect *rect, Uint32 cor)
{
    if (!d || !d->pixels) return -1;
    SDL_Rect r = rect ? *rect : (SDL_Rect){ 0, 0, d->w, d->h };
    if (!intersectar(&r, &d->clip_rect)) return 0;
    int bpp = d->format->BytesPerPixel;
    for (int j = 0; j < r.h; j++) {
        unsigned char *linha = (unsigned char *)d->pixels + (size_t)(r.y + j) * d->pitch + (size_t)r.x * bpp;
        if (bpp == 1) memset(linha, (int)(cor & 0xFF), (size_t)r.w);
        else if (bpp == 2) for (int i = 0; i < r.w; i++) ((Uint16 *)linha)[i] = (Uint16)cor;
        else for (int i = 0; i < r.w; i++) ((Uint32 *)linha)[i] = cor;
    }
    return 0;
}

static uint32_t cor32(const SDL_Color *c) { return ((uint32_t)c->r << 16) | ((uint32_t)c->g << 8) | c->b; }

int SDL_UpperBlit(SDL_Surface *o, SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd)
{
    if (!o || !d || !o->pixels || !d->pixels) { SDL_SetError("blit sin superficie"); return -1; }
    SDL_Rect src = ro ? *ro : (SDL_Rect){ 0, 0, o->w, o->h };
    int dx = rd ? rd->x : 0, dy = rd ? rd->y : 0;
    /* Recorte en el origen (la superficie entera). */
    SDL_Rect todo = { 0, 0, o->w, o->h };
    int ox = src.x, oy = src.y;
    if (!intersectar(&src, &todo)) { if (rd) { rd->w = rd->h = 0; } return 0; }
    dx += src.x - ox;
    dy += src.y - oy;
    /* Recorte en el destino (su clip_rect). */
    SDL_Rect dst = { dx, dy, src.w, src.h };
    if (!intersectar(&dst, &d->clip_rect)) { if (rd) { rd->x = dst.x; rd->y = dst.y; rd->w = rd->h = 0; } return 0; }
    src.x += dst.x - dx;
    src.y += dst.y - dy;
    if (rd) *rd = dst;

    int bo = o->format->BytesPerPixel, bd = d->format->BytesPerPixel;
    int chave = (o->flags & SDL_SRCCOLORKEY) != 0;
    Uint32 k = o->format->colorkey;
    for (int j = 0; j < dst.h; j++) {
        const unsigned char *lo = (const unsigned char *)o->pixels + (size_t)(src.y + j) * o->pitch + (size_t)src.x * bo;
        unsigned char *ld = (unsigned char *)d->pixels + (size_t)(dst.y + j) * d->pitch + (size_t)dst.x * bd;
        if (bo == 1 && bd == 1) {
            if (!chave) memcpy(ld, lo, (size_t)dst.w);
            else for (int i = 0; i < dst.w; i++) { if (lo[i] != (Uint8)k) ld[i] = lo[i]; }
        } else if (bo == 1 && bd == 4) {
            const SDL_Color *pal = o->format->palette ? o->format->palette->colors : paleta_fisica;
            for (int i = 0; i < dst.w; i++) {
                if (chave && lo[i] == (Uint8)k) continue;
                ((Uint32 *)ld)[i] = cor32(&pal[lo[i]]);
            }
        } else if (bo == bd) {
            if (!chave) memcpy(ld, lo, (size_t)dst.w * bo);
            else if (bo == 4) {
                for (int i = 0; i < dst.w; i++) if (((const Uint32 *)lo)[i] != k) ((Uint32 *)ld)[i] = ((const Uint32 *)lo)[i];
            } else {
                for (int i = 0; i < dst.w; i++) if (((const Uint16 *)lo)[i] != (Uint16)k) ((Uint16 *)ld)[i] = ((const Uint16 *)lo)[i];
            }
        } else {
            SDL_SetError("blit de %d a %d bits no admitido", bo * 8, bd * 8);
            return -1;
        }
    }
    return 0;
}

/* Escalado por vecino más próximo, entre superficies del mismo formato. */
int SDL_SoftStretch(SDL_Surface *o, SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd)
{
    if (!o || !d || o->format->BytesPerPixel != d->format->BytesPerPixel) return -1;
    SDL_Rect s = ro ? *ro : (SDL_Rect){ 0, 0, o->w, o->h };
    SDL_Rect t = rd ? *rd : (SDL_Rect){ 0, 0, d->w, d->h };
    if (s.w <= 0 || s.h <= 0 || t.w <= 0 || t.h <= 0) return 0;
    int b = o->format->BytesPerPixel;
    for (int j = 0; j < t.h; j++) {
        int sy = s.y + j * s.h / t.h;
        const unsigned char *lo = (const unsigned char *)o->pixels + (size_t)sy * o->pitch;
        unsigned char *ld = (unsigned char *)d->pixels + (size_t)(t.y + j) * d->pitch + (size_t)t.x * b;
        for (int i = 0; i < t.w; i++) {
            int sx = s.x + i * s.w / t.w;
            memcpy(ld + (size_t)i * b, lo + (size_t)sx * b, (size_t)b);
        }
    }
    return 0;
}

SDL_Surface *SDL_DisplayFormat(SDL_Surface *s)
{
    if (!s) return NULL;
    SDL_Surface *n = superficie_nova(s->w, s->h, s->format->BitsPerPixel, NULL, 0);
    if (!n) return NULL;
    for (int j = 0; j < s->h; j++)
        memcpy((char *)n->pixels + (size_t)j * n->pitch, (char *)s->pixels + (size_t)j * s->pitch,
               (size_t)s->w * s->format->BytesPerPixel);
    if (s->format->palette && n->format->palette)
        memcpy(n->format->palette->colors, s->format->palette->colors, 256 * sizeof(SDL_Color));
    return n;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b)
{
    if (f && f->BitsPerPixel == 8 && f->palette) {
        /* El índice de la paleta más parecido (la SDL hace lo mismo). */
        int melhor = 0, dmin = 0x7FFFFFFF;
        for (int i = 0; i < f->palette->ncolors; i++) {
            const SDL_Color *c = &f->palette->colors[i];
            int dr = c->r - r, dg = c->g - g, db = c->b - b;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < dmin) { dmin = dist; melhor = i; if (!dist) break; }
        }
        return (Uint32)melhor;
    }
    return ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    (void)a;
    return SDL_MapRGB(f, r, g, b);
}

void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *f, Uint8 *r, Uint8 *g, Uint8 *b)
{
    if (f && f->BitsPerPixel == 8 && f->palette) {
        const SDL_Color *c = &f->palette->colors[pixel & 0xFF];
        *r = c->r; *g = c->g; *b = c->b;
        return;
    }
    *r = (Uint8)(pixel >> 16); *g = (Uint8)(pixel >> 8); *b = (Uint8)pixel;
}

static void fisica_mudar(const SDL_Color *cores, int primeiro, int n)
{
    for (int i = 0; i < n; i++) {
        int k = primeiro + i;
        if (k < 0 || k > 255) continue;
        paleta_fisica[k] = cores[i];
        paleta32[k] = cor32(&cores[i]);
    }
}

int SDL_SetPalette(SDL_Surface *s, int flags, SDL_Color *cores, int primeiro, int n)
{
    if (!s || !cores) return 0;
    if ((flags & SDL_LOGPAL) && s->format->palette) {
        SDL_Palette *p = s->format->palette;
        for (int i = 0; i < n; i++) {
            int k = primeiro + i;
            if (k >= 0 && k < p->ncolors) p->colors[k] = cores[i];
        }
    }
    if ((flags & SDL_PHYSPAL) && s == tela) fisica_mudar(cores, primeiro, n);
    return 1;
}

int SDL_SetColors(SDL_Surface *s, SDL_Color *cores, int primeiro, int n)
{
    return SDL_SetPalette(s, SDL_LOGPAL | SDL_PHYSPAL, cores, primeiro, n);
}

int SDL_SaveBMP(SDL_Surface *s, const char *ficheiro)
{
    (void)s; (void)ficheiro;
    SDL_SetError("SDL_SaveBMP no está en el SaguiOS");
    return -1;
}

/* ------------------------------------------------------------------ vídeo */

SDL_Rect **SDL_ListModes(SDL_PixelFormat *formato, Uint32 flags)
{
    (void)formato; (void)flags;
    return (SDL_Rect **)-1;   /* cualquier tamaño */
}

int SDL_VideoModeOK(int largura, int altura, int bits, Uint32 flags)
{
    (void)flags;
    if (largura <= 0 || altura <= 0) return 0;
    return (bits == 8 || bits == 32) ? bits : 32;
}

const SDL_VideoInfo *SDL_GetVideoInfo(void)
{
    static SDL_VideoInfo info;
    static SDL_PixelFormat f;
    formato_preparar(&f, 32);
    info.wm_available = 1;
    info.blit_sw = 1;
    info.blit_sw_CC = 1;
    info.blit_fill = 1;
    info.vfmt = &f;
    info.current_w = janela_l;
    info.current_h = janela_a;
    return &info;
}

/* Calcula dónde va la superficie en la ventana y las tablas de escalado:
 * a pantalla completa (o en una ventana de otro tamaño), 4:3 centrado; si
 * la ventana es la superficie por un factor entero, ese factor. */
static void preparar_escala(void)
{
    if (!tela) return;
    int sl = tela->w, sa = tela->h;
    if (janela_l == sl * sagui_sdl_escala && janela_a == sa * sagui_sdl_escala && !em_cheia) {
        dest_l = janela_l; dest_a = janela_a;
    } else {
        /* 320x200 es 16:10 con píxeles cuadrados, pero la VGA lo mostraba en
         * una pantalla 4:3 (píxeles altos): se rellena al alto. */
        dest_a = janela_a;
        dest_l = (janela_a * 4) / 3;
        if (sl * 3 != sa * 4 && sl * 10 != sa * 16) dest_l = janela_a * sl / sa;   /* otras proporciones: las suyas */
        if (dest_l > janela_l) { dest_l = janela_l; dest_a = janela_l * 3 / 4; }
    }
    if (dest_l < 1) dest_l = 1;
    if (dest_a < 1) dest_a = 1;
    dest_x = (janela_l - dest_l) / 2;
    dest_y = (janela_a - dest_a) / 2;
    if (tab_col_n < dest_l) { free(tab_col); tab_col = malloc((size_t)dest_l * sizeof(int)); tab_col_n = tab_col ? dest_l : 0; }
    if (tab_fila_n < dest_a) { free(tab_fila); tab_fila = malloc((size_t)dest_a * sizeof(int)); tab_fila_n = tab_fila ? dest_a : 0; }
    if (!tab_col || !tab_fila) { dest_l = dest_a = 0; return; }
    for (int x = 0; x < dest_l; x++) tab_col[x] = x * sl / dest_l;
    for (int y = 0; y < dest_a; y++) tab_fila[y] = y * sa / dest_a;
    if (janela_px) preencher32(janela_px, 0, (size_t)janela_l * (size_t)janela_a);   /* las bandas, negras */
    registrar("[%s] escala: superficie %dx%d en ventana %dx%d -> %dx%d en (%d,%d)",
              sagui_sdl_nome, sl, sa, janela_l, janela_a, dest_l, dest_a, dest_x, dest_y);
}

/* Convierte y copia la superficie a la ventana. */
static void copiar_para_janela(void)
{
    if (!tela || !janela_px || !dest_l || !dest_a) return;
    int prev = -1;
    uint32_t *d = janela_px + (size_t)dest_y * janela_l + dest_x;
    for (int y = 0; y < dest_a; y++, d += janela_l) {
        int sy = tab_fila[y];
        if (sy == prev) { memcpy(d, d - janela_l, (size_t)dest_l * 4); continue; }
        prev = sy;
        const unsigned char *o = (const unsigned char *)tela->pixels + (size_t)sy * tela->pitch;
        if (tela->format->BytesPerPixel == 1)
            for (int x = 0; x < dest_l; x++) d[x] = paleta32[o[tab_col[x]]];
        else
            for (int x = 0; x < dest_l; x++) d[x] = ((const uint32_t *)o)[tab_col[x]] & 0xFFFFFF;
    }
}

static void ao_redimensionar(int l, int a, int estado)
{
    int cheia = (estado == JAN_ESTADO_TELA_CHEIA);
    if (l != janela_l || a != janela_a) {
        uint32_t *px = NULL;
        if (redimensionar_janela(janela_id, l, a, &px) < 0 || !px) return;
        janela_px = px; janela_l = l; janela_a = a;
    }
    if (cheia != em_cheia) {
        em_cheia = cheia;
        if (sagui_sdl_ao_mudar_cheia) sagui_sdl_ao_mudar_cheia(em_cheia);
    }
    preparar_escala();
    copiar_para_janela();
    atualizar_janela(janela_id, NULL);
    registrar("[%s] ventana %dx%d (%s)", sagui_sdl_nome, l, a, em_cheia ? "pantalla completa" : "ventana");
}

static void tratar_evento(evento_t *e);

/* Espera (poco) a que el kernel confirme la pantalla completa o la salida. */
static void esperar_estado(int alvo)
{
    unsigned limite = tempo_ms() + 700;
    while (em_cheia != alvo && (int)(limite - tempo_ms()) > 0) {
        evento_t e;
        if (esperar_evento(janela_id, &e, 30) == 1) tratar_evento(&e);
    }
}

SDL_Surface *SDL_SetVideoMode(int largura, int altura, int bits, Uint32 flags)
{
    if (largura <= 0 || altura <= 0) { SDL_SetError("modo %dx%d no válido", largura, altura); return NULL; }
    if (bits != 8) bits = 32;
    int quer_cheia = (flags & SDL_FULLSCREEN) != 0;
    if (sagui_cheia_inicial) { quer_cheia = 1; sagui_cheia_inicial = 0; }
    if (sagui_sdl_escala < 1) sagui_sdl_escala = 1;

    /* La superficie de la pantalla: nueva si cambia de tamaño o de bits. */
    if (!tela || tela->w != largura || tela->h != altura || tela->format->BitsPerPixel != bits) {
        if (tela) { SDL_Surface *v = tela; tela = NULL; SDL_FreeSurface(v); }
        tela = superficie_nova(largura, altura, bits, NULL, 0);
        if (!tela) return NULL;
        tela->flags |= SDL_HWPALETTE;
        if (bits == 8) {
            /* La paleta física empieza como la lógica (gris), como en la SDL. */
            fisica_mudar(tela->format->palette->colors, 0, 256);
        }
    }
    tela->flags = (tela->flags & ~(SDL_FULLSCREEN | SDL_RESIZABLE)) | (flags & (SDL_FULLSCREEN | SDL_RESIZABLE));

    int jl = largura * sagui_sdl_escala, ja = altura * sagui_sdl_escala;
    if (janela_id < 0) {
        janela_id = criar_janela(sagui_sdl_titulo ? sagui_sdl_titulo : sagui_sdl_nome, jl, ja, &janela_px);
        if (janela_id < 0 || !janela_px) { janela_id = -1; janela_px = NULL; SDL_SetError("no se pudo abrir la ventana"); return NULL; }
        janela_l = jl;
        janela_a = ja;
        em_cheia = 0;
        estilo_janela(janela_id, ESTILO_BASE, 0, 0);   /* F11 del kernel, y el Esc llega al juego */
        registrar("[%s] ventana %dx%d creada (superficie %dx%d, %d bits)", sagui_sdl_nome, jl, ja, largura, altura, bits);
    } else if (!em_cheia && (janela_l != jl || janela_a != ja)) {
        uint32_t *px = NULL;
        if (redimensionar_janela(janela_id, jl, ja, &px) >= 0 && px) { janela_px = px; janela_l = jl; janela_a = ja; }
    }
    preparar_escala();

    if (quer_cheia != em_cheia) {
        estilo_janela(janela_id, ESTILO_BASE | (quer_cheia ? JAN_TELA_CHEIA : 0), 0, 0);
        esperar_estado(quer_cheia);
        registrar("[%s] pantalla completa %s: ventana %dx%d", sagui_sdl_nome,
                  em_cheia ? "activada" : "desactivada", janela_l, janela_a);
        if (!em_cheia && (janela_l != jl || janela_a != ja)) ao_redimensionar(jl, ja, JAN_ESTADO_NORMAL);
    }
    return tela;
}

SDL_Surface *SDL_GetVideoSurface(void) { return janela_id >= 0 ? tela : NULL; }
int sagui_sdl_em_cheia(void) { return em_cheia; }

int SDL_WM_ToggleFullScreen(SDL_Surface *s)
{
    (void)s;
    if (janela_id < 0) return 0;
    estilo_janela(janela_id, ESTILO_BASE | (em_cheia ? 0 : JAN_TELA_CHEIA), 0, 0);
    esperar_estado(!em_cheia);
    return 1;
}

/* D476: limitador de FPS general (variable SDL_SAGUI_FPS_MAX; 0 o ausente = sin tope). Duerme lo que falte tras presentar
 * (SDL_Delay, que sigue bombeando el audio). Igual que el de la SDL 2: plazo acumulado y reinicio si se va 100 ms tarde. */
static void fps_limitar(void)
{
    static int leido, fps, fps_ant;
    static unsigned base, n;
    if (!leido) { const char *e = getenv("SDL_SAGUI_FPS_MAX"); fps = e && *e ? atoi(e) : 0; leido = 1; }
    if (fps <= 0) return;
    if (fps > 1000) fps = 1000;
    unsigned ahora = tempo_ms();
    if (fps != fps_ant || (int)(ahora - (base + n * 1000u / (unsigned)fps)) > 100) { fps_ant = fps; base = ahora; n = 0; return; }
    n++;
    int falta = (int)(base + n * 1000u / (unsigned)fps - ahora);
    if (falta > 0) SDL_Delay((Uint32)falta);
    if (n >= 100000u) { base = tempo_ms(); n = 0; }
}

int SDL_Flip(SDL_Surface *s)
{
    (void)s;
    if (janela_id < 0) return -1;
    bombear_audio();
    copiar_para_janela();
    if (!primeiro_quadro) {
        primeiro_quadro = 1;
        registrar("[%s] primer cuadro dibujado (%dx%d)", sagui_sdl_nome, janela_l, janela_a);
    }
    int rc = atualizar_janela(janela_id, NULL);
    fps_limitar();
    return rc;
}

void SDL_UpdateRect(SDL_Surface *s, Sint32 x, Sint32 y, Uint32 l, Uint32 a)
{
    (void)x; (void)y; (void)l; (void)a;
    SDL_Flip(s);
}

void SDL_UpdateRects(SDL_Surface *s, int n, SDL_Rect *rects)
{
    (void)n; (void)rects;
    SDL_Flip(s);
}

void SDL_WM_SetCaption(const char *titulo, const char *icone)
{
    (void)icone;
    if (janela_id >= 0 && titulo) titulo_janela(janela_id, titulo);
}

/* D242: captura real del ratón. El kernel congela y oculta el puntero y manda los
 * desplazamientos crudos (EV_MOVER_CRU) como SDL_MOUSEMOTION con xrel/yrel. La suelta solo
 * si pierde el foco, se abre un menú o se pulsa Ctrl+Alt (EV_CAPTURA_SOLTA). */
static int captura_ativa = 0;
int SDL_WM_GrabInput(int modo)
{
    if (modo == SDL_GRAB_QUERY) return captura_ativa ? SDL_GRAB_ON : SDL_GRAB_OFF;
    if (janela_id < 0) return SDL_GRAB_OFF;
    if (modo == SDL_GRAB_ON && !captura_ativa) {
        if (estilo_janela(janela_id, JAN_SO_CAPTURA | JAN_CAPTURA, 0, 0) == 0) {
            captura_ativa = 1;
            registrar("[%s] raton capturado", sagui_sdl_nome);
        }
    } else if (modo == SDL_GRAB_OFF && captura_ativa) {
        estilo_janela(janela_id, JAN_SO_CAPTURA, 0, 0);
        captura_ativa = 0;
        registrar("[%s] raton soltado", sagui_sdl_nome);
    }
    return captura_ativa ? SDL_GRAB_ON : SDL_GRAB_OFF;
}
/* Como en la SDL 1.2: devuelve si se veía antes (SDL_QUERY: si se ve). El
 * puntero del sistema se oculta solo sobre la ventana con el foco (D230). */
int SDL_ShowCursor(int estado)
{
    int antes = cursor_oculto ? SDL_DISABLE : SDL_ENABLE;
    if (estado == SDL_ENABLE || estado == SDL_DISABLE) {
        int oculto = estado == SDL_DISABLE;
        if (oculto != cursor_oculto) {
            cursor_oculto = oculto;
            if (janela_id >= 0) cursor_janela(janela_id, !oculto);
            registrar("[%s] puntero del sistema %s", sagui_sdl_nome, oculto ? "oculto" : "visible");
        }
    }
    return antes;
}

/* El SaguiOS no mueve el ratón del sistema: se encola el movimiento pedido,
 * que es lo que leen los juegos (D156). */
void SDL_WarpMouse(Uint16 x, Uint16 y)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_MOUSEMOTION;
    ev.motion.x = x;
    ev.motion.y = y;
    rato_x = x; rato_y = y;
    if (fila_n < TAM_FILA) { fila[(fila_ini + fila_n) % TAM_FILA] = ev; fila_n++; }
}

/* ------------------------------------------------------------------ teclado */

SDLMod SDL_GetModState(void) { return mods_atual; }
Uint8 *SDL_GetKeyState(int *n) { if (n) *n = SDLK_LAST; return estado_teclas; }
Uint8 SDL_GetMouseState(int *x, int *y) { if (x) *x = rato_x; if (y) *y = rato_y; return rato_botoes; }

const char *SDL_GetKeyName(SDLKey t)
{
    static char buf[8];
    if ((t >= SDLK_a && t <= SDLK_z) || (t >= SDLK_0 && t <= SDLK_9) || (t > 32 && t < 127)) {
        buf[0] = (char)t; buf[1] = 0;
        return buf;
    }
    if (t >= SDLK_F1 && t <= SDLK_F15) { snprintf(buf, sizeof buf, "f%d", (int)(t - SDLK_F1) + 1); return buf; }
    if (t >= SDLK_KP0 && t <= SDLK_KP9) { snprintf(buf, sizeof buf, "[%d]", (int)(t - SDLK_KP0)); return buf; }
    switch (t) {
    case SDLK_RETURN: return "return";
    case SDLK_ESCAPE: return "escape";
    case SDLK_SPACE: return "space";
    case SDLK_BACKSPACE: return "backspace";
    case SDLK_TAB: return "tab";
    case SDLK_DELETE: return "delete";
    case SDLK_UP: return "up";
    case SDLK_DOWN: return "down";
    case SDLK_LEFT: return "left";
    case SDLK_RIGHT: return "right";
    case SDLK_HOME: return "home";
    case SDLK_END: return "end";
    case SDLK_PAGEUP: return "page up";
    case SDLK_PAGEDOWN: return "page down";
    case SDLK_INSERT: return "insert";
    case SDLK_LSHIFT: return "left shift";
    case SDLK_RSHIFT: return "right shift";
    case SDLK_LCTRL: return "left ctrl";
    case SDLK_RCTRL: return "right ctrl";
    case SDLK_LALT: return "left alt";
    case SDLK_RALT: return "right alt";
    default: return "unknown key";
    }
}

/* Lo que da el kernel (un carácter Latin-1 o TECLA_*) en una tecla de SDL.
 * Los códigos 1..26 son a la vez Ctrl+letra y caracteres de control (8 =
 * retroceso, 9 = tabulador, 10 = Intro): los distingue el Ctrl (D156). */
static SDLKey tecla_sdl(unsigned t, unsigned mods)
{
    switch (t) {
    case TECLA_CIMA: return SDLK_UP;
    case TECLA_BAIXO: return SDLK_DOWN;
    case TECLA_ESQUERDA: return SDLK_LEFT;
    case TECLA_DIREITA: return SDLK_RIGHT;
    case TECLA_HOME: return SDLK_HOME;
    case TECLA_END: return SDLK_END;
    case TECLA_PGUP: return SDLK_PAGEUP;
    case TECLA_PGDN: return SDLK_PAGEDOWN;
    case TECLA_INSERT: return SDLK_INSERT;
    case TECLA_DELETE: return SDLK_DELETE;
    case TECLA_ESC: return SDLK_ESCAPE;
    default: break;
    }
    if (t >= TECLA_F1 && t <= TECLA_F12) return (SDLKey)(SDLK_F1 + (t - TECLA_F1));
    if (t >= 1 && t <= 26) {
        if (!(mods & KMOD_CTRL)) {
            if (t == 8) return SDLK_BACKSPACE;
            if (t == 9) return SDLK_TAB;
            if (t == 10 || t == 13) return SDLK_RETURN;
        }
        return (SDLKey)(SDLK_a + (t - 1));
    }
    if (t == 27) return SDLK_ESCAPE;
    if (t >= 'A' && t <= 'Z') return (SDLKey)(t - 'A' + 'a');
    if (t >= 32 && t < 127) return (SDLKey)t;
    /* D229: como la SDL 1.2, las de Latin-1 (160..255) son SDLK_WORLD_*: con el
     * teclado español, la tecla de la consola de Quake (la de la izquierda del 1) da «º». */
    if (t >= 160 && t <= 255) return (SDLKey)t;
    return SDLK_UNKNOWN;
}

/* El código de rastreo (conjunto 1) de una tecla, para saber cuándo se
 * suelta. 0 = no se sabe (esa tecla no tendrá KEYUP hasta la siguiente). */
static unsigned char rastreo(SDLKey s)
{
    static const char fila1[] = "qwertyuiop", fila2[] = "asdfghjkl", fila3[] = "zxcvbnm";
    if (s >= SDLK_a && s <= SDLK_z) {
        const char *p;
        if ((p = strchr(fila1, (int)s))) return (unsigned char)(0x10 + (p - fila1));
        if ((p = strchr(fila2, (int)s))) return (unsigned char)(0x1E + (p - fila2));
        if ((p = strchr(fila3, (int)s))) return (unsigned char)(0x2C + (p - fila3));
        return 0;
    }
    if (s >= SDLK_1 && s <= SDLK_9) return (unsigned char)(0x02 + (s - SDLK_1));
    if (s == SDLK_0) return 0x0B;
    if (s >= SDLK_F1 && s <= SDLK_F10) return (unsigned char)(0x3B + (s - SDLK_F1));
    switch (s) {
    case SDLK_F11: return 0x57;
    case SDLK_F12: return 0x58;
    case SDLK_RETURN: return 0x1C;
    case SDLK_ESCAPE: return 0x01;
    case SDLK_SPACE: return 0x39;
    case SDLK_BACKSPACE: return 0x0E;
    case SDLK_TAB: return 0x0F;
    case SDLK_MINUS: return 0x0C;
    case SDLK_EQUALS: return 0x0D;
    case SDLK_LEFTBRACKET: return 0x1A;
    case SDLK_RIGHTBRACKET: return 0x1B;
    case SDLK_BACKSLASH: return 0x2B;
    case SDLK_SEMICOLON: return 0x27;
    case SDLK_QUOTE: return 0x28;
    case SDLK_BACKQUOTE: return 0x29;
    case SDLK_COMMA: return 0x33;
    case SDLK_PERIOD: return 0x34;
    case SDLK_SLASH: return 0x35;
    case SDLK_UP: return 0x80 | 0x48;
    case SDLK_DOWN: return 0x80 | 0x50;
    case SDLK_LEFT: return 0x80 | 0x4B;
    case SDLK_RIGHT: return 0x80 | 0x4D;
    case SDLK_HOME: return 0x80 | 0x47;
    case SDLK_END: return 0x80 | 0x4F;
    case SDLK_PAGEUP: return 0x80 | 0x49;
    case SDLK_PAGEDOWN: return 0x80 | 0x51;
    case SDLK_INSERT: return 0x80 | 0x52;
    case SDLK_DELETE: return 0x80 | 0x53;
    default: return 0;
    }
}

static Uint16 unicode_de(SDLKey s, unsigned t)
{
    switch (s) {
    case SDLK_RETURN: return 13;
    case SDLK_BACKSPACE: return 8;
    case SDLK_TAB: return 9;
    case SDLK_ESCAPE: return 27;
    default: break;
    }
    if (t >= 0x20 && t < 0x100 && t != 0x7F) return (Uint16)t;
    return 0;
}

static void enfileirar(const SDL_Event *ev)
{
    if (fila_n >= TAM_FILA) return;
    fila[(fila_ini + fila_n) % TAM_FILA] = *ev;
    fila_n++;
}

static void evento_tecla(Uint8 tipo, SDLKey sim, Uint16 unicode)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = tipo;
    ev.key.type = tipo;
    ev.key.state = tipo == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
    ev.key.keysym.sym = sim;
    ev.key.keysym.mod = mods_atual;
    ev.key.keysym.unicode = unicode;
    ev.key.keysym.scancode = rastreo(sim);
    if (sim > 0 && sim < SDLK_LAST) estado_teclas[sim] = tipo == SDL_KEYDOWN;
    enfileirar(&ev);
}

static void marcar_tecla(SDLKey sim)
{
    unsigned char sc = rastreo(sim);
    if (!sc) return;
    for (int i = 0; i < n_teclas; i++)
        if (teclas[i].sim == sim) { teclas[i].down = 1; teclas[i].desde = tempo_ms(); return; }
    if (n_teclas >= TAM_TECLAS) return;
    teclas[n_teclas].sim = sim;
    teclas[n_teclas].sc = sc;
    teclas[n_teclas].down = 1;
    teclas[n_teclas].desde = tempo_ms();
    n_teclas++;
}

/* Compara con el mapa de teclas que el kernel dice que están pulsadas:
 * fabrica los KEYUP de las que se soltaron y los KEYDOWN/KEYUP de las
 * modificadoras. Sin foco, todo se da por suelto. */
static void teclas_do_mapa(void)
{
    unsigned char mapa[32];
    memset(mapa, 0, sizeof mapa);
    int foco = teclas_seguradas(mapa);
    SDLMod mods = KMOD_NONE;
    for (int i = 0; i < N_MODIF; i++) {
        int agora = foco && SEGURADA(mapa, modificadoras[i].sc);
        if (agora) mods |= modificadoras[i].mod;
    }
    mods_atual = (SDLMod)mods;
    for (int i = 0; i < N_MODIF; i++) {
        int agora = foco && SEGURADA(mapa, modificadoras[i].sc);
        if (agora != modif_down[i]) {
            modif_down[i] = (unsigned char)agora;
            evento_tecla(agora ? SDL_KEYDOWN : SDL_KEYUP, modificadoras[i].sim, 0);
        }
    }
    for (int i = 0; i < n_teclas; i++) {
        if (!teclas[i].down) continue;
        if (foco && tempo_ms() - teclas[i].desde < PULSACION_MIN_MS) continue;   /* todavía no */
        if (!(foco && SEGURADA(mapa, teclas[i].sc))) {
            teclas[i].down = 0;
            evento_tecla(SDL_KEYUP, teclas[i].sim, 0);
        }
    }
}

/* ------------------------------------------------------------------ eventos */

static void pos_superficie(int *x, int *y)
{
    if (!tela || !dest_l || !dest_a) return;
    int sx = ((*x - dest_x) * tela->w) / dest_l, sy = ((*y - dest_y) * tela->h) / dest_a;
    *x = sx < 0 ? 0 : sx >= tela->w ? tela->w - 1 : sx;
    *y = sy < 0 ? 0 : sy >= tela->h ? tela->h - 1 : sy;
}

static void tratar_evento(evento_t *pe)
{
    evento_t e = *pe;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    switch (e.tipo) {
    case EV_REDIMENSIONAR:
        ao_redimensionar(e.x, e.y, (int)e.tecla);
        break;
    case EV_TECLA: {
        SDLMod m = mods_atual;
        if (e.modificadores & 1) m |= KMOD_LSHIFT;
        if (e.modificadores & 2) m |= KMOD_LCTRL;
        if (e.modificadores & 4) m |= KMOD_LALT;
        mods_atual = m;
        SDLKey s = tecla_sdl(e.tecla, (unsigned)m);
        if (s == SDLK_UNKNOWN) break;
        evento_tecla(SDL_KEYDOWN, s, unicode_de(s, e.tecla));
        marcar_tecla(s);
        registrar("[%s] tecla %u (SDL %d)", sagui_sdl_nome, e.tecla, (int)s);
        break;
    }
    case EV_MOVER: {
        int x = e.x, y = e.y;
        pos_superficie(&x, &y);
        ev.type = SDL_MOUSEMOTION;
        ev.motion.state = rato_botoes;
        ev.motion.x = (Uint16)x;
        ev.motion.y = (Uint16)y;
        ev.motion.xrel = (Sint16)(x - rato_x);
        ev.motion.yrel = (Sint16)(y - rato_y);
        rato_x = x; rato_y = y;
        enfileirar(&ev);
        break;
    }
    case EV_MOVER_CRU: {   /* D242: desplazamiento crudo con el ratón capturado */
        if (tela && dest_l && dest_a) { e.x = e.x * tela->w / dest_l; e.y = e.y * tela->h / dest_a; }   /* ventana ampliada */
        int nx = rato_x + e.x, ny = rato_y + e.y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (tela && nx >= tela->w) nx = tela->w - 1;
        if (tela && ny >= tela->h) ny = tela->h - 1;
        ev.type = SDL_MOUSEMOTION;
        ev.motion.state = rato_botoes;
        ev.motion.x = (Uint16)nx;
        ev.motion.y = (Uint16)ny;
        ev.motion.xrel = (Sint16)e.x;
        ev.motion.yrel = (Sint16)e.y;
        rato_x = nx; rato_y = ny;
        enfileirar(&ev);
        break;
    }
    case EV_CAPTURA_SOLTA:
        captura_ativa = 0;
        registrar("[%s] el sistema solto el raton", sagui_sdl_nome);
        /* D294: DOSBox sigue creyendo que tiene el ratón (su «locked») hasta que ve que pierde el foco:
         * con este aviso lo suelta y el siguiente clic lo vuelve a capturar. */
        if (sagui_sdl_foco_ao_soltar) {
            SDL_Event af;
            memset(&af, 0, sizeof af);
            af.type = SDL_ACTIVEEVENT;
            af.active.gain = 0;
            af.active.state = SDL_APPINPUTFOCUS;
            enfileirar(&af);
        }
        break;
    case EV_CLIQUE:
    case EV_SOLTOU: {
        int x = e.x, y = e.y;
        pos_superficie(&x, &y);
        Uint8 b = (e.botoes & 1) ? SDL_BUTTON_LEFT : (e.botoes & 4) ? SDL_BUTTON_MIDDLE : SDL_BUTTON_RIGHT;
        ev.type = e.tipo == EV_CLIQUE ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        ev.button.button = b;
        ev.button.state = e.tipo == EV_CLIQUE ? SDL_PRESSED : SDL_RELEASED;
        ev.button.x = (Uint16)x;
        ev.button.y = (Uint16)y;
        if (e.tipo == EV_CLIQUE) rato_botoes |= (Uint8)SDL_BUTTON(b);
        else rato_botoes &= (Uint8)~SDL_BUTTON(b);
        rato_x = x; rato_y = y;
        enfileirar(&ev);
        break;
    }
    case EV_FECHAR:
        ev.type = SDL_QUIT;
        enfileirar(&ev);
        registrar("[%s] cerrar la ventana", sagui_sdl_nome);
        break;
    default:
        break;
    }
}

static void eventos_do_sistema(void)
{
    if (janela_id < 0) return;
    evento_t e;
    while (esperar_evento(janela_id, &e, 0) == 1) tratar_evento(&e);
    teclas_do_mapa();
}

static void joy_atualizar(int forcar);
static int joy_eventos;

void SDL_PumpEvents(void)
{
    bombear_audio();
    eventos_do_sistema();
    if (joy_eventos == SDL_ENABLE) joy_atualizar(0);   /* D307: los eventos de los joysticks abiertos */
}

int SDL_PollEvent(SDL_Event *evento)
{
    SDL_PumpEvents();
    if (fila_n <= 0) return 0;
    if (evento) {
        *evento = fila[fila_ini];
        fila_ini = (fila_ini + 1) % TAM_FILA;
        fila_n--;
    }
    return 1;
}

int SDL_WaitEvent(SDL_Event *evento)
{
    while (!SDL_PollEvent(evento)) dormir(5);
    return 1;
}

int SDL_PushEvent(SDL_Event *evento)
{
    if (!evento) return -1;
    enfileirar(evento);
    return 0;
}

Uint8 SDL_EventState(Uint8 tipo, int estado) { (void)tipo; (void)estado; return SDL_ENABLE; }
int SDL_EnableKeyRepeat(int atraso, int intervalo) { (void)atraso; (void)intervalo; return 0; }
int SDL_EnableUNICODE(int ativar) { (void)ativar; return 1; }

/* ------------------------------------------------------------------ reloj */

Uint32 SDL_GetTicks(void) { return tempo_ms(); }

void SDL_Delay(Uint32 ms)
{
    bombear_audio();
    dormir(ms);
}

/* ------------------------------------------------------------------ audio */

int SDL_OpenAudio(SDL_AudioSpec *pedido, SDL_AudioSpec *obtido)
{
    if (!pedido || !pedido->callback) { SDL_SetError("audio sin callback"); return -1; }
    audio_spec = *pedido;
    if (audio_spec.format != AUDIO_S8 && audio_spec.format != AUDIO_U8) audio_spec.format = AUDIO_S16SYS;
    if (!audio_spec.channels) audio_spec.channels = 1;
    if (audio_spec.channels > 2) audio_spec.channels = 2;
    if (!audio_spec.samples) audio_spec.samples = 1024;
    int bits = (audio_spec.format == AUDIO_S16SYS) ? 16 : 8;
    audio_spec.silence = (audio_spec.format == AUDIO_U8) ? 0x80 : 0;
    audio_spec.size = (Uint32)audio_spec.samples * audio_spec.channels * (bits / 8);
    /* Un bloque: lo que pide el juego, sin pasar del búfer. */
    audio_bloco = audio_spec.size;
    if (audio_bloco > sizeof audio_buf || audio_bloco == 0) audio_bloco = sizeof audio_buf;
    audio_fd = audio_abrir(audio_spec.freq, audio_spec.channels, bits);
    if (audio_fd < 0) audio_fd = -1;   /* sin placa de sonido: el juego queda mudo */
    audio_aberto = 1;
    audio_pausado = 1;
    registrar("[%s] audio %d Hz, %d canales, %d bits, bloque %u (%s)", sagui_sdl_nome, audio_spec.freq,
              audio_spec.channels, bits, audio_bloco, audio_fd >= 0 ? "con placa" : "mudo");
    if (obtido) *obtido = audio_spec;
    return 0;
}

void SDL_CloseAudio(void)
{
    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
    audio_aberto = 0;
}

void SDL_PauseAudio(int pausa) { audio_pausado = (pausa != 0); }
void SDL_LockAudio(void) {}
void SDL_UnlockAudio(void) {}

/* Mezcla con volumen (0..128) y saturación, en el formato abierto. */
void SDL_MixAudio(Uint8 *d, const Uint8 *o, Uint32 n, int volume)
{
    if (volume <= 0) return;
    if (volume > SDL_MIX_MAXVOLUME) volume = SDL_MIX_MAXVOLUME;
    if (audio_spec.format == AUDIO_S16SYS) {
        Sint16 *dd = (Sint16 *)d;
        const Sint16 *oo = (const Sint16 *)o;
        for (Uint32 i = 0; i < n / 2; i++) {
            int v = dd[i] + (oo[i] * volume) / SDL_MIX_MAXVOLUME;
            dd[i] = (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
    } else if (audio_spec.format == AUDIO_S8) {
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

/* Sin conversión: los juegos piden el formato que se abre. */
int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 formato_origem, Uint8 canais_origem, int freq_origem,
                      Uint16 formato_destino, Uint8 canais_destino, int freq_destino)
{
    if (!cvt) return -1;
    memset(cvt, 0, sizeof *cvt);
    cvt->src_format = formato_origem;
    cvt->dst_format = formato_destino;
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;
    cvt->rate_incr = 1.0;
    if (formato_origem != formato_destino || canais_origem != canais_destino || freq_origem != freq_destino) {
        SDL_SetError("conversión de audio no disponible");
        return -1;
    }
    return 0;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt) { if (cvt) cvt->len_cvt = cvt->len; return 0; }

/* Llama al callback del juego mientras quepa otro bloque en el flujo. */
static void bombear_audio(void)
{
    if (!audio_aberto || audio_pausado || audio_fd < 0) return;
    for (int vezes = 0; vezes < 4; vezes++) {
        int livre = audio_livre(audio_fd);
        if (livre < (int)audio_bloco) return;
        memset(audio_buf, audio_spec.silence, audio_bloco);
        audio_spec.callback(audio_spec.userdata, audio_buf, (int)audio_bloco);
        audio_escrever(audio_fd, audio_buf, audio_bloco);
    }
}

/* ------------------------------------------------------------------ joystick (D307) */

/* Los mandos del sistema (libsagui mandos.h): se leen por sondeo y, como en la SDL 1.2, con los
 * eventos activados (por defecto) SDL_PumpEvents genera SDL_JOYAXISMOTION, SDL_JOYHATMOTION y
 * SDL_JOYBUTTONDOWN/UP de los joysticks abiertos al cambiar su estado. La SDL 1.2 no conoce la
 * conexión en caliente: los índices son los de la lista del sistema al abrir. */
struct _SDL_Joystick {
    int indice;
    unsigned id;
    int abierto;
    int n_eixos, n_botoes, n_hats;
    estado_mando_t est;
};
static info_mando_t joy_lista[MANDOS_MAX];
static int joy_n;
static struct _SDL_Joystick joys[MANDOS_MAX];
static int joy_eventos = SDL_ENABLE;
static unsigned joy_ultimo;

static void joy_listar(void)
{
    int n = mandos_listar(joy_lista, MANDOS_MAX, NULL);
    joy_n = n < 0 ? 0 : n;
}

int SDL_NumJoysticks(void) { joy_listar(); return joy_n; }

const char *SDL_JoystickName(int indice)
{
    if (indice < 0 || indice >= joy_n) joy_listar();
    if (indice < 0 || indice >= joy_n) return NULL;
    return joy_lista[indice].nome;
}

SDL_Joystick *SDL_JoystickOpen(int indice)
{
    joy_listar();
    if (indice < 0 || indice >= joy_n) { SDL_SetError("no hay joystick %d", indice); return NULL; }
    struct _SDL_Joystick *j = &joys[indice];
    if (!j->abierto) {
        memset(j, 0, sizeof *j);
        j->indice = indice;
        j->id = joy_lista[indice].id;
        j->n_eixos = joy_lista[indice].n_eixos;
        j->n_botoes = joy_lista[indice].n_botoes;
        j->n_hats = joy_lista[indice].n_hats;
        mando_ler(indice, &j->est);   /* el estado de partida: sin eventos falsos al empezar */
        registrar("[%s] joystick abierto: %s (%d ejes, %d botones, %d hats)", sagui_sdl_nome, joy_lista[indice].nome,
                  j->n_eixos, j->n_botoes, j->n_hats);
    }
    j->abierto++;
    return j;
}

void SDL_JoystickClose(SDL_Joystick *joy) { if (joy && joy->abierto) joy->abierto--; }
int SDL_JoystickOpened(int indice) { return indice >= 0 && indice < MANDOS_MAX && joys[indice].abierto; }
int SDL_JoystickIndex(SDL_Joystick *joy) { return joy ? joy->indice : -1; }
int SDL_JoystickNumAxes(SDL_Joystick *joy) { return joy ? joy->n_eixos : 0; }
int SDL_JoystickNumButtons(SDL_Joystick *joy) { return joy ? joy->n_botoes : 0; }
int SDL_JoystickNumHats(SDL_Joystick *joy) { return joy ? joy->n_hats : 0; }
int SDL_JoystickNumBalls(SDL_Joystick *joy) { (void)joy; return 0; }
int SDL_JoystickGetBall(SDL_Joystick *joy, int b, int *dx, int *dy) { (void)joy; (void)b; if (dx) *dx = 0; if (dy) *dy = 0; return -1; }

Sint16 SDL_JoystickGetAxis(SDL_Joystick *joy, int eixo)
{
    if (!joy || eixo < 0 || eixo >= joy->n_eixos) return 0;
    return joy->est.eixos[eixo];
}

Uint8 SDL_JoystickGetButton(SDL_Joystick *joy, int botao)
{
    if (!joy || botao < 0 || botao >= joy->n_botoes) return 0;
    return (joy->est.botoes >> botao) & 1;
}

Uint8 SDL_JoystickGetHat(SDL_Joystick *joy, int chapeu)
{
    if (!joy || chapeu < 0 || chapeu >= joy->n_hats) return SDL_HAT_CENTERED;
    return joy->est.hats[chapeu];   /* MANDO_HAT_* son los mismos bits que SDL_HAT_* */
}

/* Lee los abiertos (como mucho cada 4 ms) y, con los eventos activados, avisa de los cambios. */
static void joy_atualizar(int forcar)
{
    int alguno = 0;
    for (int i = 0; i < MANDOS_MAX && !alguno; i++) alguno = joys[i].abierto;
    if (!alguno) return;   /* sin joysticks abiertos no se pregunta nada al sistema */
    unsigned agora = tempo_ms();
    if (!forcar && agora - joy_ultimo < 4) return;
    joy_ultimo = agora;
    for (int i = 0; i < MANDOS_MAX; i++) {
        struct _SDL_Joystick *j = &joys[i];
        if (!j->abierto) continue;
        estado_mando_t e;
        if (mando_ler(j->indice, &e) < 0 || e.id != j->id) memset(&e, 0, sizeof e);   /* desconectado: en reposo */
        if (joy_eventos == SDL_ENABLE) {
            SDL_Event ev;
            for (int k = 0; k < j->n_eixos && k < MANDO_EIXOS_MAX; k++)
                if (e.eixos[k] != j->est.eixos[k]) {
                    memset(&ev, 0, sizeof ev);
                    ev.jaxis.type = SDL_JOYAXISMOTION; ev.jaxis.which = (Uint8)i; ev.jaxis.axis = (Uint8)k; ev.jaxis.value = e.eixos[k];
                    enfileirar(&ev);
                }
            for (int k = 0; k < j->n_hats && k < MANDO_HATS_MAX; k++)
                if (e.hats[k] != j->est.hats[k]) {
                    memset(&ev, 0, sizeof ev);
                    ev.jhat.type = SDL_JOYHATMOTION; ev.jhat.which = (Uint8)i; ev.jhat.hat = (Uint8)k; ev.jhat.value = e.hats[k];
                    enfileirar(&ev);
                }
            uint32_t mudou = e.botoes ^ j->est.botoes;
            for (int k = 0; k < j->n_botoes && k < 32; k++)
                if (mudou & (1u << k)) {
                    memset(&ev, 0, sizeof ev);
                    bool p = (e.botoes >> k) & 1;
                    ev.jbutton.type = p ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP; ev.jbutton.which = (Uint8)i;
                    ev.jbutton.button = (Uint8)k; ev.jbutton.state = p ? SDL_PRESSED : SDL_RELEASED;
                    enfileirar(&ev);
                }
        }
        j->est = e;
    }
}

void SDL_JoystickUpdate(void) { joy_atualizar(1); }

int SDL_JoystickEventState(int estado)
{
    if (estado == SDL_ENABLE || estado == SDL_IGNORE) joy_eventos = estado;
    return joy_eventos;
}

/* ------------------------------------------------------------------ D294: lo que pide DOSBox */

/* Mira o saca de la cola los eventos cuyo tipo está en la máscara. */
int SDL_PeepEvents(SDL_Event *eventos, int n, SDL_eventaction accion, Uint32 mascara)
{
    if (accion == SDL_ADDEVENT) {
        for (int i = 0; i < n; i++) enfileirar(&eventos[i]);
        return n;
    }
    SDL_PumpEvents();
    int achados = 0;
    SDL_Event resto[TAM_FILA];
    int n_resto = 0;
    for (int i = 0; i < fila_n; i++) {
        SDL_Event *e = &fila[(fila_ini + i) % TAM_FILA];
        if (achados < n && (mascara & (1u << e->type))) {
            if (eventos) eventos[achados] = *e;
            achados++;
            if (accion == SDL_PEEKEVENT) resto[n_resto++] = *e;
        } else {
            resto[n_resto++] = *e;
        }
    }
    if (accion == SDL_GETEVENT) {
        for (int i = 0; i < n_resto; i++) fila[i] = resto[i];
        fila_ini = 0;
        fila_n = n_resto;
    }
    return achados;
}

void SDL_WM_SetIcon(SDL_Surface *icone, Uint8 *mascara) { (void)icone; (void)mascara; }

char *SDL_VideoDriverName(char *destino, int tamanho)
{
    if (!destino || tamanho <= 0) return NULL;
    strncpy(destino, "sagui", (size_t)tamanho - 1);
    destino[tamanho - 1] = 0;
    return destino;
}

/* Las variables de entorno de la SDL no hacen nada aquí: se guardan en el entorno de C. */
int SDL_putenv(const char *texto)
{
    char *copia = strdup(texto);
    if (!copia) return -1;
    char *igual = strchr(copia, '=');
    if (!igual) { free(copia); return -1; }
    *igual = 0;
    int r = setenv(copia, igual + 1, 1);
    free(copia);
    return r;
}

SDL_Overlay *SDL_CreateYUVOverlay(int l, int a, Uint32 formato, SDL_Surface *tela)
{
    (void)l; (void)a; (void)formato; (void)tela;
    SDL_SetError("no hay superposicion YUV");
    return NULL;
}
int SDL_LockYUVOverlay(SDL_Overlay *o) { (void)o; return -1; }
void SDL_UnlockYUVOverlay(SDL_Overlay *o) { (void)o; }
int SDL_DisplayYUVOverlay(SDL_Overlay *o, SDL_Rect *destino) { (void)o; (void)destino; return -1; }
void SDL_FreeYUVOverlay(SDL_Overlay *o) { (void)o; }

int SDL_CDNumDrives(void) { return 0; }
const char *SDL_CDName(int unidade) { (void)unidade; return NULL; }
SDL_CD *SDL_CDOpen(int unidade) { (void)unidade; return NULL; }
CDstatus SDL_CDStatus(SDL_CD *cd) { (void)cd; return CD_ERROR; }
int SDL_CDPlay(SDL_CD *cd, int inicio, int quadros) { (void)cd; (void)inicio; (void)quadros; return -1; }
int SDL_CDPause(SDL_CD *cd) { (void)cd; return -1; }
int SDL_CDResume(SDL_CD *cd) { (void)cd; return -1; }
int SDL_CDStop(SDL_CD *cd) { (void)cd; return -1; }
int SDL_CDEject(SDL_CD *cd) { (void)cd; return -1; }
void SDL_CDClose(SDL_CD *cd) { (void)cd; }

struct SDL_mutex { int ocupado; };
SDL_mutex *SDL_CreateMutex(void) { return calloc(1, sizeof(SDL_mutex)); }
void SDL_DestroyMutex(SDL_mutex *m) { free(m); }
int SDL_mutexP(SDL_mutex *m) { if (m) m->ocupado++; return 0; }
int SDL_mutexV(SDL_mutex *m) { if (m && m->ocupado > 0) m->ocupado--; return 0; }
