/* sdl2_image.c - SDL2_image mínima del SaguiOS (D374).
 *
 * IMG_Load y IMG_Load_Mem: PNG (decodificador propio con canal alfa, todos los tipos de color y
 * profundidades de 1 a 16 bits, tRNS; sin entrelazado Adam7) y BMP, JPEG y GIF con los de la
 * libsagui (imagem_decodificar, sin transparencia). Siempre da una superficie ARGB8888 de 32 bits.
 * El inflado es el de la libsagui (inflar). Pensada para los ports que usan SDL2_image
 * (Freeciv primero): API IMG_Init, IMG_Quit, IMG_Load. */
#include "SDL_image.h"
#include "imagem.h"
#include "compressao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int IMG_Init(int flags) { return flags & (IMG_INIT_JPG | IMG_INIT_PNG); }
void IMG_Quit(void) { }

static unsigned be32(const unsigned char *p) { return (unsigned)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static int abs_i(int v) { return v < 0 ? -v : v; }

static unsigned char paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs_i(p - a), pb = abs_i(p - b), pc = abs_i(p - c);
    return (unsigned char)(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}

static SDL_Surface *superficie(int w, int h)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) SDL_SetError("IMG: sin memoria para %dx%d", w, h);
    return s;
}

static SDL_Surface *cargar_png(const unsigned char *d, int n)
{
    static const unsigned char firma[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (n < 33 || memcmp(d, firma, 8)) return NULL;
    int w = 0, h = 0, prof = 0, tipo = 0, entre = 0;
    unsigned char pal[256][4];
    int npal = 0;
    for (int i = 0; i < 256; i++) { pal[i][0] = pal[i][1] = pal[i][2] = 0; pal[i][3] = 255; }
    int gris_trns = -1, rgb_trns[3] = { -1, -1, -1 };
    unsigned char *idat = NULL;
    size_t nidat = 0, capidat = 0;
    int pos = 8, fin = 0;
    while (pos + 12 <= n && !fin) {
        unsigned len = be32(d + pos);
        const unsigned char *t = d + pos + 4, *c = d + pos + 8;
        if (len > (unsigned)(n - pos - 12)) break;
        if (!memcmp(t, "IHDR", 4) && len >= 13) {
            w = (int)be32(c); h = (int)be32(c + 4); prof = c[8]; tipo = c[9]; entre = c[12];
        } else if (!memcmp(t, "PLTE", 4)) {
            npal = (int)len / 3; if (npal > 256) npal = 256;
            for (int i = 0; i < npal; i++) { pal[i][0] = c[i * 3]; pal[i][1] = c[i * 3 + 1]; pal[i][2] = c[i * 3 + 2]; }
        } else if (!memcmp(t, "tRNS", 4)) {
            if (tipo == 3) { for (unsigned i = 0; i < len && i < 256; i++) pal[i][3] = c[i]; }
            else if (tipo == 0 && len >= 2) gris_trns = c[0] << 8 | c[1];
            else if (tipo == 2 && len >= 6) { rgb_trns[0] = c[0] << 8 | c[1]; rgb_trns[1] = c[2] << 8 | c[3]; rgb_trns[2] = c[4] << 8 | c[5]; }
        } else if (!memcmp(t, "IDAT", 4)) {
            if (nidat + len > capidat) {
                capidat = (nidat + len) * 2;
                unsigned char *nn = realloc(idat, capidat);
                if (!nn) { free(idat); return NULL; }
                idat = nn;
            }
            memcpy(idat + nidat, c, len);
            nidat += len;
        } else if (!memcmp(t, "IEND", 4)) fin = 1;
        pos += 12 + (int)len;
    }
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || entre != 0 || nidat < 6 ||
        !(prof == 1 || prof == 2 || prof == 4 || prof == 8 || prof == 16)) { free(idat); return NULL; }
    int canales = tipo == 0 ? 1 : tipo == 2 ? 3 : tipo == 3 ? 1 : tipo == 4 ? 2 : tipo == 6 ? 4 : 0;
    if (!canales) { free(idat); return NULL; }
    int bpp_bits = canales * prof;
    int stride = (w * bpp_bits + 7) / 8;
    int bpp = (bpp_bits + 7) / 8;
    size_t total = (size_t)(stride + 1) * (size_t)h;
    unsigned char *raw = malloc(total);
    if (!raw) { free(idat); return NULL; }
    /* flujo zlib: 2 bytes de cabecera, deflate, 4 de Adler-32 */
    int r = inflar(idat + 2, (int)nidat - 2, raw, (int)total);
    free(idat);
    if (r < (int)total) { free(raw); return NULL; }
    SDL_Surface *s = superficie(w, h);
    if (!s) { free(raw); return NULL; }
    unsigned char *prev = calloc(1, (size_t)stride + 8);
    if (!prev) { free(raw); SDL_FreeSurface(s); return NULL; }
    for (int y = 0; y < h; y++) {
        unsigned char *f = raw + (size_t)y * (stride + 1);
        unsigned char *cur = f + 1;
        int ft = f[0];
        for (int i = 0; i < stride; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
            switch (ft) {
            case 1: cur[i] = (unsigned char)(cur[i] + a); break;
            case 2: cur[i] = (unsigned char)(cur[i] + b); break;
            case 3: cur[i] = (unsigned char)(cur[i] + ((a + b) >> 1)); break;
            case 4: cur[i] = (unsigned char)(cur[i] + paeth(a, b, c)); break;
            default: break;
            }
        }
        memcpy(prev, cur, (size_t)stride);
        Uint32 *dst = (Uint32 *)((unsigned char *)s->pixels + (size_t)y * s->pitch);
        for (int x = 0; x < w; x++) {
            unsigned R, G, B, A = 255;
            if (prof == 8) {
                const unsigned char *p = cur + x * canales;
                if (tipo == 6) { R = p[0]; G = p[1]; B = p[2]; A = p[3]; }
                else if (tipo == 2) { R = p[0]; G = p[1]; B = p[2];
                    if (rgb_trns[0] >= 0 && (int)R == rgb_trns[0] && (int)G == rgb_trns[1] && (int)B == rgb_trns[2]) A = 0; }
                else if (tipo == 4) { R = G = B = p[0]; A = p[1]; }
                else if (tipo == 3) { R = pal[p[0]][0]; G = pal[p[0]][1]; B = pal[p[0]][2]; A = pal[p[0]][3]; }
                else { R = G = B = p[0]; if (gris_trns >= 0 && (int)R == gris_trns) A = 0; }
            } else if (prof == 16) {
                const unsigned char *p = cur + x * canales * 2;
                if (tipo == 6) { R = p[0]; G = p[2]; B = p[4]; A = p[6]; }
                else if (tipo == 2) { R = p[0]; G = p[2]; B = p[4]; }
                else if (tipo == 4) { R = G = B = p[0]; A = p[2]; }
                else { R = G = B = p[0]; }
            } else {   /* 1, 2 y 4 bits: gris o paleta */
                int bit = x * prof;
                unsigned v = (cur[bit >> 3] >> (8 - prof - (bit & 7))) & ((1u << prof) - 1);
                if (tipo == 3) { R = pal[v][0]; G = pal[v][1]; B = pal[v][2]; A = pal[v][3]; }
                else { R = G = B = v * 255 / ((1u << prof) - 1); if (gris_trns >= 0 && (int)v == gris_trns) A = 0; }
            }
            dst[x] = A << 24 | R << 16 | G << 8 | B;
        }
    }
    free(prev);
    free(raw);
    return s;
}

/* BMP, JPEG, GIF y PNM con la libsagui: píxeles 0x00RRGGBB, opacos. */
static SDL_Surface *cargar_sagui(const unsigned char *d, int n)
{
    imagem_t img;
    memset(&img, 0, sizeof img);
    if (imagem_decodificar(d, n, &img) != 0) return NULL;
    SDL_Surface *s = superficie(img.larg, img.alt);
    if (s) {
        for (int y = 0; y < img.alt; y++) {
            Uint32 *dst = (Uint32 *)((unsigned char *)s->pixels + (size_t)y * s->pitch);
            for (int x = 0; x < img.larg; x++) dst[x] = 0xFF000000u | img.px[(size_t)y * img.larg + x];
        }
    }
    imagem_liberar(&img);
    return s;
}

SDL_Surface *IMG_Load_Mem(const void *datos, int n)
{
    SDL_Surface *s = cargar_png(datos, n);
    if (!s) s = cargar_sagui(datos, n);
    if (!s) SDL_SetError("IMG: formato de imagen no reconocido");
    return s;
}

SDL_Surface *IMG_Load(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) { SDL_SetError("IMG: no se puede abrir %s", ruta); return NULL; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = n > 0 ? malloc((size_t)n) : NULL;
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); SDL_SetError("IMG: no se puede leer %s", ruta); return NULL; }
    fclose(f);
    SDL_Surface *s = IMG_Load_Mem(b, (int)n);
    free(b);
    return s;
}
