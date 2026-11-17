/* sdl2_ttf.c - SDL2_ttf mínima del SaguiOS sobre la libfontes (D374).
 *
 * TTF_OpenFont abre un .ttf/.otf con fonte_abrir_archivo; el tamaño en puntos vale por píxeles
 * (SDL_ttf usa 72 ppp). Los textos se miden y se dibujan con las métricas de la libfontes y la
 * cobertura de cada glifo (fonte_glifo) pasa al canal alfa de una superficie ARGB8888, de modo
 * que Solid, Shaded y Blended dan resultados mezclables con SDL_BlitSurface. La negrita es
 * sintética (el glifo se dibuja otra vez un píxel a la derecha), la cursiva usa fonte_obliqua,
 * el subrayado y el tachado son una línea del color del texto. */
#include "SDL_ttf.h"
#include "fontes.h"
#include <stdlib.h>
#include <string.h>

struct _TTF_Font {
    fonte_t *f;
    fonte_t *obliqua;   /* se crea al pedir cursiva */
    int px;
    int estilo;
};

static int iniciada;
int TTF_Init(void) { iniciada = 1; return 0; }
void TTF_Quit(void) { iniciada = 0; }
int TTF_WasInit(void) { return iniciada; }

TTF_Font *TTF_OpenFont(const char *ruta, int ptsize)
{
    fonte_t *f = fonte_abrir_archivo(ruta);
    if (!f) { SDL_SetError("TTF: no se puede abrir la fuente %s", ruta); return NULL; }
    TTF_Font *t = calloc(1, sizeof *t);
    if (!t) { fonte_fechar(f); return NULL; }
    t->f = f;
    t->px = ptsize < 4 ? 4 : ptsize;
    return t;
}

void TTF_CloseFont(TTF_Font *t)
{
    if (!t) return;
    if (t->obliqua) fonte_fechar(t->obliqua);
    fonte_fechar(t->f);
    free(t);
}

void TTF_SetFontStyle(TTF_Font *t, int estilo) { if (t) t->estilo = estilo; }
int TTF_GetFontStyle(const TTF_Font *t) { return t ? t->estilo : 0; }
int TTF_FontAscent(const TTF_Font *t) { return fonte_ascendente(t->f, t->px); }
int TTF_FontDescent(const TTF_Font *t) { return -fonte_descendente(t->f, t->px); }
int TTF_FontHeight(const TTF_Font *t) { return fonte_ascendente(t->f, t->px) + fonte_descendente(t->f, t->px); }
int TTF_FontLineSkip(const TTF_Font *t) { return fonte_altura_normal(t->f, t->px); }

static fonte_t *fonte_de(TTF_Font *t)
{
    if (!(t->estilo & TTF_STYLE_ITALIC)) return t->f;
    if (!t->obliqua) t->obliqua = fonte_obliqua(t->f);
    return t->obliqua ? t->obliqua : t->f;
}

static int leer_car(const char *p, unsigned *cp)
{
    unsigned c = (unsigned char)p[0];
    int n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    if (n == 1) { *cp = c; return 1; }
    unsigned v = n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
    for (int k = 1; k < n; k++) {
        if (((unsigned char)p[k] & 0xC0) != 0x80) { *cp = c; return 1; }
        v = v << 6 | ((unsigned char)p[k] & 0x3F);
    }
    *cp = v;
    return n;
}

static int ancho_texto(TTF_Font *t, const char *s)
{
    fonte_t *f = fonte_de(t);
    int x64 = 0;
    while (*s) {
        unsigned cp;
        s += leer_car(s, &cp);
        if (cp == '\r' || cp == '\n' || cp == 0xFEFF) continue;
        if (!fonte_tem(f, cp) && fonte_tem(f, '?')) cp = '?';
        x64 += fonte_avanco(f, cp, t->px);
    }
    int w = (x64 + 32) / 64;
    if (t->estilo & TTF_STYLE_BOLD) w += 1;
    return w;
}

int TTF_SizeUTF8(TTF_Font *t, const char *s, int *w, int *h)
{
    if (!t || !s) return -1;
    if (w) *w = ancho_texto(t, s);
    if (h) *h = TTF_FontHeight(t);
    return 0;
}
int TTF_SizeText(TTF_Font *t, const char *s, int *w, int *h) { return TTF_SizeUTF8(t, s, w, h); }

/* Dibuja el texto en s (ARGB8888): el color fg con la cobertura como alfa. */
static void dibujar(TTF_Font *t, const char *texto, SDL_Surface *s, SDL_Color fg, int con_fondo)
{
    fonte_t *f = fonte_de(t);
    int asc = fonte_ascendente(f, t->px);
    int x64 = 0;
    Uint32 col = (Uint32)fg.r << 16 | (Uint32)fg.g << 8 | fg.b;
    int pasadas = (t->estilo & TTF_STYLE_BOLD) ? 2 : 1;
    while (*texto) {
        unsigned cp;
        texto += leer_car(texto, &cp);
        if (cp == '\r' || cp == '\n' || cp == 0xFEFF) continue;
        if (!fonte_tem(f, cp) && fonte_tem(f, '?')) cp = '?';
        int xe = (x64 + 32) >> 6;
        x64 += fonte_avanco(f, cp, t->px);
        if (cp == ' ' || cp == 0xA0 || cp < ' ') continue;
        int l, a, ox, oy;
        const unsigned char *g = fonte_glifo(f, cp, t->px, &l, &a, &ox, &oy);
        if (!g) continue;
        for (int p = 0; p < pasadas; p++)
            for (int yy = 0; yy < a; yy++) {
                int py = asc + oy + yy;
                if (py < 0 || py >= s->h) continue;
                Uint32 *fila = (Uint32 *)((unsigned char *)s->pixels + (size_t)py * s->pitch);
                for (int xx = 0; xx < l; xx++) {
                    int pxx = xe + ox + xx + p, c = g[yy * l + xx];
                    if (!c || pxx < 0 || pxx >= s->w) continue;
                    Uint32 d = fila[pxx];
                    unsigned da = d >> 24;
                    if (con_fondo) {   /* sobre el fondo opaco: mezcla de colores */
                        unsigned dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255;
                        dr += ((fg.r - (int)dr) * c + 127) / 255;
                        dg += ((fg.g - (int)dg) * c + 127) / 255;
                        db += ((fg.b - (int)db) * c + 127) / 255;
                        fila[pxx] = 0xFF000000u | dr << 16 | dg << 8 | db;
                    } else {           /* sobre transparente: el alfa suma cobertura */
                        unsigned na = da + (c * (255 - da) + 127) / 255;
                        fila[pxx] = na << 24 | col;
                    }
                }
            }
    }
    if (t->estilo & (TTF_STYLE_UNDERLINE | TTF_STYLE_STRIKETHROUGH)) {
        int grosor = t->px / 16 + 1;
        for (int k = 0; k < 2; k++) {
            if (!(t->estilo & (k ? TTF_STYLE_STRIKETHROUGH : TTF_STYLE_UNDERLINE))) continue;
            int y0 = k ? asc - t->px / 4 : asc + 1;
            for (int y = y0; y < y0 + grosor && y < s->h; y++) {
                Uint32 *fila = (Uint32 *)((unsigned char *)s->pixels + (size_t)y * s->pitch);
                for (int x = 0; x < s->w; x++) fila[x] = 0xFF000000u | col;
            }
        }
    }
}

static SDL_Surface *crear(TTF_Font *t, const char *texto, SDL_Color fg, SDL_Color bg, int con_fondo)
{
    if (!t || !texto) { SDL_SetError("TTF: fuente o texto nulos"); return NULL; }
    int w = ancho_texto(t, texto), h = TTF_FontHeight(t);
    if (w < 1) w = 1;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return NULL;
    Uint32 fondo = con_fondo ? (0xFF000000u | (Uint32)bg.r << 16 | (Uint32)bg.g << 8 | bg.b) : 0;
    for (int y = 0; y < h; y++) {
        Uint32 *fila = (Uint32 *)((unsigned char *)s->pixels + (size_t)y * s->pitch);
        for (int x = 0; x < w; x++) fila[x] = fondo;
    }
    dibujar(t, texto, s, fg, con_fondo);
    return s;
}

SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *t, const char *s, SDL_Color fg) { SDL_Color n = { 0, 0, 0, 0 }; return crear(t, s, fg, n, 0); }
SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *t, const char *s, SDL_Color fg) { return TTF_RenderUTF8_Blended(t, s, fg); }
SDL_Surface *TTF_RenderUTF8_Shaded(TTF_Font *t, const char *s, SDL_Color fg, SDL_Color bg) { return crear(t, s, fg, bg, 1); }
SDL_Surface *TTF_RenderText_Blended(TTF_Font *t, const char *s, SDL_Color fg) { return TTF_RenderUTF8_Blended(t, s, fg); }
SDL_Surface *TTF_RenderText_Solid(TTF_Font *t, const char *s, SDL_Color fg) { return TTF_RenderUTF8_Blended(t, s, fg); }
SDL_Surface *TTF_RenderText_Shaded(TTF_Font *t, const char *s, SDL_Color fg, SDL_Color bg) { return TTF_RenderUTF8_Shaded(t, s, fg, bg); }
