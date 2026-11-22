// SPDX-License-Identifier: GPL-2.0-or-later
/* sdl2presentarprova.c - /apps/bin/sdl2presentar: prueba y medida de «presentar sin volver a la
 * RAM» (D432, docs/gpu3d.md): un programa de la SDL 2 a pantalla completa con el renderizador
 * acelerado, como un juego.
 *
 *   sdl2presentar [-n cuadros] [-m textura|dibujo|superficie] [-l ANCHOxALTO] [-v] [-e segundos] [-a]
 *     -n   cuadros de la medida (por defecto 120), sin pausas: al final escribe los fps
 *     -m   «textura» (por defecto): una textura que se actualiza cada cuadro y un SDL_RenderCopy que
 *          cubre el lienzo (DOOM, AoEN, emuladores); «dibujo»: borrado, rectángulos y copias sueltas;
 *          «superficie»: sin renderizador, pintando en la superficie de la ventana (Tzar, OpenTTD), que
 *          solo va por la GPU con SDL_SAGUI_GPU=1
 *     -l   lienzo lógico (por defecto 320x240)
 *     -v   en ventana (no a pantalla completa): no hay presentación directa
 *     -e   después de la medida, mantiene un patrón fijo esos segundos (rojo, verde, azul y blanco en
 *          los cuatro cuartos del lienzo) presentándolo a 50 cuadros por segundo; Esc lo corta
 *     -a   a mitad de esa espera pide un aviso en pantalla (el compositor recupera la pantalla)
 *
 * Con SDL_SAGUI_GPU=0 mide el camino por software; con SDL_SAGUI_GPU_PRESENTAR=0, la GPU trayendo el
 * cuadro a la RAM (lo de antes de la D432). Escribe por el registro:
 *   [SDL2PRES] renderizador NOMBRE gpu=G lienzo LxA ventana LxA completa=C modo=M
 *   [SDL2PRES] medida: N cuadros en T ms: F.F fps; directos D, por la RAM R
 *   [SDL2PRES] patron fijo / [SDL2PRES] aviso pedido / [SDL2PRES] fin: directos D, por la RAM R */
#include "sagui.h"
#include "SDL.h"

static int L = 320, A = 240;

static int num(const char **s)
{
    int v = 0;
    while (**s >= '0' && **s <= '9') v = v * 10 + (*(*s)++ - '0');
    return v;
}

/* El patrón fijo: cuatro cuartos de color liso. */
static Uint32 patron(int x, int y)
{
    int dir = x >= L / 2, abajo = y >= A / 2;
    return !dir && !abajo ? 0xFFFF0000u : dir && !abajo ? 0xFF00FF00u : !dir ? 0xFF0000FFu : 0xFFFFFFFFu;
}

/* Un cuadro de la medida: fondo que cambia y una barra que se mueve (determinista). */
static void cuadro_px(Uint32 *px, int f)
{
    int bx = (f * 5) % (L - 16);
    for (int y = 0; y < A; y++) {
        Uint32 *p = px + (size_t)y * L;
        Uint32 fundo = 0xFF000000u | ((Uint32)((y + f) & 255) << 16) | ((Uint32)((y * 2) & 255) << 8) | (Uint32)(f & 255);
        for (int x = 0; x < L; x++) p[x] = fundo ^ (Uint32)((x & 32) ? 0x00202020u : 0);
        for (int x = bx; x < bx + 16; x++) p[x] = 0xFFFFFF00u;
    }
}

static void cuadro_dibujo(SDL_Renderer *r, SDL_Texture *sello, int f, int fijo)
{
    if (fijo) {
        SDL_Rect q[4] = { { 0, 0, L / 2, A / 2 }, { L / 2, 0, L - L / 2, A / 2 }, { 0, A / 2, L / 2, A - A / 2 }, { L / 2, A / 2, L - L / 2, A - A / 2 } };
        static const Uint8 c[4][3] = { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 255, 255, 255 } };
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);
        for (int i = 0; i < 4; i++) { SDL_SetRenderDrawColor(r, c[i][0], c[i][1], c[i][2], 255); SDL_RenderFillRect(r, &q[i]); }
        return;
    }
    SDL_SetRenderDrawColor(r, (Uint8)(f * 3), 40, (Uint8)(255 - f * 2), 255);
    SDL_RenderClear(r);
    for (int i = 0; i < 12; i++) {
        SDL_Rect d = { (f * (i + 2) + i * 37) % (L - 40), (f * (i + 1) + i * 23) % (A - 40), 40, 40 };
        if (i & 1) SDL_RenderCopy(r, sello, NULL, &d);
        else { SDL_SetRenderDrawColor(r, (Uint8)(i * 20), (Uint8)(255 - i * 20), 128, 255); SDL_RenderFillRect(r, &d); }
    }
}

int principal(int argc, char **argv)
{
    int cuadros = 120, espera = 0, ventana = 0, dibujo = 0, aviso = 0, superficie = 0;
    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];
        if (s[0] != '-' || !s[1]) continue;
        const char *v = i + 1 < argc ? argv[i + 1] : "";
        switch (s[1]) {
        case 'n': cuadros = num(&v); i++; break;
        case 'e': espera = num(&v); i++; break;
        case 'm': dibujo = v[0] == 'd'; superficie = v[0] == 's'; i++; break;
        case 'l': L = num(&v); if (*v == 'x') { v++; A = num(&v); } i++; break;
        case 'v': ventana = 1; break;
        case 'a': aviso = 1; break;
        default:
            imprimir("uso: sdl2presentar [-n cuadros] [-m textura|dibujo|superficie] [-l ANCHOxALTO] [-v] [-e segundos] [-a]\n");
            return 2;
        }
    }
    if (L < 64 || A < 48 || L > 2048 || A > 2048) { imprimir("sdl2presentar: lienzo de 64x48 a 2048x2048\n"); return 2; }
    sagui_sdl2_nome = "SDL2PRES";
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    /* Un lienzo grande no cabe como ventana: se crea pequeña, pasa a pantalla completa y entonces se pide
     * el lienzo (como hacen Tzar y AoEN). */
    int cl = !ventana && L > 640 ? 640 : L, ca = !ventana && A > 480 ? 480 : A;
    SDL_Window *ven = SDL_CreateWindow("sdl2presentar", 0, 0, cl, ca, ventana ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!ven) return 3;
    if (cl != L || ca != A) SDL_SetWindowSize(ven, L, A);
    SDL_ShowCursor(SDL_DISABLE);
    int completa = (SDL_GetWindowFlags(ven) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL, *sello = NULL;
    SDL_Surface *sup = NULL;
    Uint32 *px = NULL;
    if (superficie) {
        /* Sin renderizador: el programa pinta en la superficie de la ventana (32 bits, el lienzo). */
        sup = SDL_GetWindowSurface(ven);
        if (!sup || sup->format->BytesPerPixel != 4 || sup->pitch != L * 4) return 4;
        px = sup->pixels;
        registrar("[SDL2PRES] renderizador ninguno gpu=%d lienzo %dx%d completa=%d modo=superficie",
                  getenv("SDL_SAGUI_GPU") && getenv("SDL_SAGUI_GPU")[0] == '1', L, A, completa);
    } else {
        ren = SDL_CreateRenderer(ven, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) return 4;
        SDL_RendererInfo info;
        SDL_GetRendererInfo(ren, &info);
        registrar("[SDL2PRES] renderizador %s gpu=%d lienzo %dx%d completa=%d modo=%s", info.name,
                  (info.flags & SDL_RENDERER_ACCELERATED) ? 1 : 0, L, A, completa, dibujo ? "dibujo" : "textura");
        px = malloc((size_t)L * A * 4);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, L, A);
        sello = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 32, 32);
        if (!px || !tex || !sello) return 5;
        static Uint32 sp[32 * 32];
        for (int i = 0; i < 32 * 32; i++) sp[i] = 0xFF000000u | (Uint32)((i % 32) * 8) << 16 | (Uint32)((i / 32) * 8) << 8 | 0x40u;
        SDL_UpdateTexture(sello, NULL, sp, 32 * 4);
    }

    unsigned long dir = 0, ram = 0;
    Uint32 t0 = SDL_GetTicks();
    for (int f = 0; f < cuadros; f++) {
        if (superficie) { cuadro_px(px, f); SDL_UpdateWindowSurface(ven); }
        else {
            if (dibujo) cuadro_dibujo(ren, sello, f, 0);
            else { cuadro_px(px, f); SDL_UpdateTexture(tex, NULL, px, L * 4); SDL_RenderCopy(ren, tex, NULL, NULL); }
            SDL_RenderPresent(ren);
        }
        if ((f & 15) == 15) { SDL_Event e; while (SDL_PollEvent(&e)) { } }
    }
    Uint32 ms = SDL_GetTicks() - t0;
    sagui_sdl2_gpu_estado(ren, &dir, &ram);
    if (cuadros > 0)
        registrar("[SDL2PRES] medida: %d cuadros en %u ms: %u.%u fps; directos %lu, por la RAM %lu", cuadros, ms,
                  ms ? (unsigned)cuadros * 1000u / ms : 0, ms ? (unsigned)cuadros * 10000u / ms % 10 : 0, dir, ram);
    if (espera > 0) {
        registrar("[SDL2PRES] patron fijo");
        for (int y = 0; y < A; y++) for (int x = 0; x < L; x++) px[(size_t)y * L + x] = patron(x, y);
        Uint32 fim = SDL_GetTicks() + (Uint32)espera * 1000u, meio = SDL_GetTicks() + (Uint32)espera * 500u;
        int sair = 0;
        while (!sair && (int)(fim - SDL_GetTicks()) > 0) {
            SDL_Event e;
            while (SDL_PollEvent(&e))
                if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) sair = 1;
            if (aviso && (int)(meio - SDL_GetTicks()) <= 0) {
                aviso = 0;
                aviso_tela("sdl2presentar: aviso de prueba (D432)", NULL);
                registrar("[SDL2PRES] aviso pedido");
            }
            if (superficie) SDL_UpdateWindowSurface(ven);
            else {
                if (dibujo) cuadro_dibujo(ren, sello, 0, 1);
                else { SDL_UpdateTexture(tex, NULL, px, L * 4); SDL_RenderCopy(ren, tex, NULL, NULL); }
                SDL_RenderPresent(ren);
            }
            SDL_Delay(20);
        }
    }
    sagui_sdl2_gpu_estado(ren, &dir, &ram);
    registrar("[SDL2PRES] fin: directos %lu, por la RAM %lu", dir, ram);
    SDL_Quit();
    return 0;
}
