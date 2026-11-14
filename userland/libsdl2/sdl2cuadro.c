/* sdl2cuadro.c - programa sintético de la SDL 2 para medir y comprobar la cadena de dibujo de
 * Command & Conquer (D247): superficie de 8 bits con paleta -> superficie de 32 bits ->
 * textura -> SDL_RenderCopy -> SDL_RenderPresent, como video_sdl2.cpp.
 *
 *   sdl2cuadro [cuadros [ancho alto [completa|escala2 [medir]]]]
 *
 * Anima un fondo fijo, un sprite que se mueve y un marcador que cambia (la mayor parte de la
 * pantalla queda quieta, como en un juego de estrategia). Escribe por el registro:
 *   [SDL2CUADRO] N cuadros: T ms, F.F fps
 *   [SDL2CUADRO] hash ventana XXXXXXXX lienzo YYYYYYYY
 * Los dos hash (FNV-1a) salen de la ventana del sistema y del lienzo lógico tras el último
 * cuadro: son los de la prueba h246, que comprueba que las optimizaciones no cambian ni un píxel.
 * La ventana lógica es de 640x400 (320x200 con «escala2», ampliada x2 por el sistema).
 */
#include "sagui.h"
#include "SDL.h"

unsigned sagui_sdl2_hash_janela(void);
unsigned sagui_sdl2_hash_lienzo(void);

static int num(const char *s, int def)
{
    int v = 0;
    if (!s || !*s) return def;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return v ? v : def;
}

/* La pantalla del cuadro f: determinista. */
static void dibujar(unsigned char *p, int pitch, int w, int h, int f)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            p[y * pitch + x] = (unsigned char)(((x ^ y) & 0x3F) + ((x >> 4) & 7) * 8 + 16);
    int sx = (f * 3) % (w > 60 ? w - 52 : 1), sy = 30 + (f * 2) % (h > 100 ? h - 90 : 1);
    for (int y = 0; y < 48 && sy + y < h; y++)
        for (int x = 0; x < 48 && sx + x < w; x++)
            p[(sy + y) * pitch + sx + x] = (unsigned char)(200 + ((x + y + f) & 31));
    for (int y = 0; y < 24 && y < h; y++)
        for (int x = 0; x < 80 && x < w; x++)
            p[y * pitch + x] = (unsigned char)((f * 7 + x) & 255);
}

int principal(int argc, char **argv)
{
    int n = argc > 1 ? num(argv[1], 120) : 120;
    int w = argc > 3 ? num(argv[2], 320) : 320, h = argc > 3 ? num(argv[3], 200) : 200;
    int completa = argc > 4 && argv[4][0] == 'c';
    if (argc > 5) sagui_sdl2_medir = 1;   /* con un 5.º argumento, las líneas «cuadro:» */
    int lw = 640, lh = 400;
    sagui_sdl2_nome = "SDL2CUADRO";
    sagui_sdl2_escala = (argc > 4 && argv[4][0] == 'e') ? 2 : 1;
    if (sagui_sdl2_escala == 2) { lw = 320; lh = 200; }   /* ventana lógica de 320x200 en una de 640x400 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    SDL_Window *ven = SDL_CreateWindow("sdl2cuadro", 0, 0, lw, lh, 0);
    if (!ven) return 2;
    SDL_Renderer *ren = SDL_CreateRenderer(ven, -1, SDL_RENDERER_TARGETTEXTURE);
    SDL_Surface *s8 = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
    SDL_Surface *s32 = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!ren || !s8 || !s32 || !tex) return 3;
    SDL_Color cores[256];
    for (int i = 0; i < 256; i++) {
        cores[i].r = (Uint8)(i * 5); cores[i].g = (Uint8)(255 - i); cores[i].b = (Uint8)(i * 3 + 40); cores[i].a = 255;
    }
    SDL_SetPaletteColors(s8->format->palette, cores, 0, 256);
    if (completa) {
        SDL_SetWindowFullscreen(ven, SDL_WINDOW_FULLSCREEN_DESKTOP);
        for (int i = 0; i < 20; i++) { SDL_PumpEvents(); SDL_Delay(20); }
    }
    SDL_Rect dst = { 0, 0, lw, lh };
    unsigned t0 = SDL_GetTicks();
    for (int f = 0; f < n; f++) {
        dibujar((unsigned char *)s8->pixels, s8->pitch, w, h, f);
        SDL_BlitSurface(s8, NULL, s32, NULL);
        SDL_UpdateTexture(tex, NULL, s32->pixels, s32->pitch);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, &dst);
        SDL_RenderPresent(ren);
    }
    unsigned ms = SDL_GetTicks() - t0;
    if (!ms) ms = 1;
    registrar("[SDL2CUADRO] %d cuadros: %u ms, %u.%u fps", n, ms, n * 1000u / ms, (n * 10000u / ms) % 10);
    registrar("[SDL2CUADRO] hash ventana %08x lienzo %08x", sagui_sdl2_hash_janela(), sagui_sdl2_hash_lienzo());
    registrar("[SDL2CUADRO] fin");
    SDL_Quit();
    return 0;
}
