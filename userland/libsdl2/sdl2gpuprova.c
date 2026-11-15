/* sdl2gpuprova.c - prueba del renderizador de la SDL 2 por GPU (D288, prueba h288).
 *
 *   sdl2gpu [cuadros]
 *
 * Dibuja las mismas escenas con el renderizador por software (sobre una superficie) y con el de
 * la ventana pedido con SDL_RENDERER_ACCELERATED (por la GPU si la hay; si no, es el de
 * software), y compara pixel a pixel. Escribe por el registro:
 *   [SDL2GPU] renderizador <nombre> gpu=<0|1>
 *   [SDL2GPU] escena <nombre>: <n> pixeles distintos de <total> (max <d>)
 *   [SDL2GPU] cuadros: software X ms, acelerado Y ms
 *   [SDL2GPU] fin
 * «Distinto» es un canal que se aparta mas de 8 niveles: los bordes del escalado y del giro
 * no son bit a bit iguales entre la GPU y la CPU. */
#include "sagui.h"
#include "SDL.h"

#define L 320
#define A 240

static SDL_Texture *tex_a, *tex_b;   /* del renderizador que se esta usando */

static SDL_Texture *textura_gradiente(SDL_Renderer *r, int alfa)
{
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 64, 64);
    if (!t) return NULL;
    Uint32 px[64 * 64];
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            Uint32 al = alfa ? (Uint32)(x * 4 + 3) : 255;
            Uint32 c = (((x / 8 + y / 8) & 1) ? 0xE0u : 0x30u);
            px[y * 64 + x] = (al << 24) | ((Uint32)(x * 4) << 16) | ((Uint32)(y * 4) << 8) | c;
        }
    SDL_UpdateTexture(t, NULL, px, 64 * 4);
    if (alfa) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

static const char *nomes[] = { "limpiar", "rellenos", "copia 1:1", "escalada x2", "escalada 3/4", "espejos",
                               "mezcla alfa", "modulacion", "giro 30", "recortada", "lineas" };
#define N_ESCENAS 11

static void escena(SDL_Renderer *r, int e)
{
    SDL_SetRenderDrawColor(r, 20, 40, 60, 255);
    SDL_RenderClear(r);
    SDL_Rect d;
    switch (e) {
    case 0: break;
    case 1:
        SDL_SetRenderDrawColor(r, 200, 30, 30, 255);
        d = (SDL_Rect){ 10, 10, 100, 60 }; SDL_RenderFillRect(r, &d);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 30, 200, 30, 128);
        d = (SDL_Rect){ 60, 40, 120, 80 }; SDL_RenderFillRect(r, &d);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        break;
    case 2:
        d = (SDL_Rect){ 20, 30, 64, 64 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        d = (SDL_Rect){ 200, 100, 64, 64 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        break;
    case 3:
        d = (SDL_Rect){ 10, 10, 128, 128 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        break;
    case 4:
        d = (SDL_Rect){ 10, 10, 48, 48 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        d = (SDL_Rect){ 100, 50, 200, 150 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        break;
    case 5:
        d = (SDL_Rect){ 10, 10, 64, 64 }; SDL_RenderCopyEx(r, tex_a, NULL, &d, 0, NULL, SDL_FLIP_HORIZONTAL);
        d = (SDL_Rect){ 100, 10, 64, 64 }; SDL_RenderCopyEx(r, tex_a, NULL, &d, 0, NULL, SDL_FLIP_VERTICAL);
        d = (SDL_Rect){ 200, 10, 64, 64 }; SDL_RenderCopyEx(r, tex_a, NULL, &d, 0, NULL, SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);
        break;
    case 6:
        d = (SDL_Rect){ 20, 20, 160, 160 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        d = (SDL_Rect){ 60, 60, 128, 128 }; SDL_RenderCopy(r, tex_b, NULL, &d);
        break;
    case 7:
        SDL_SetTextureColorMod(tex_a, 255, 128, 64);
        d = (SDL_Rect){ 10, 10, 128, 128 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        SDL_SetTextureColorMod(tex_a, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex_b, 100);
        d = (SDL_Rect){ 100, 60, 128, 128 }; SDL_RenderCopy(r, tex_b, NULL, &d);
        SDL_SetTextureAlphaMod(tex_b, 255);
        break;
    case 8:
        d = (SDL_Rect){ 100, 70, 100, 100 }; SDL_RenderCopyEx(r, tex_a, NULL, &d, 30.0, NULL, SDL_FLIP_NONE);
        break;
    case 9:
        d = (SDL_Rect){ -30, -20, 128, 128 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        d = (SDL_Rect){ L - 50, A - 40, 128, 128 }; SDL_RenderCopy(r, tex_a, NULL, &d);
        break;
    case 10:
        SDL_SetRenderDrawColor(r, 255, 255, 0, 255);
        SDL_RenderDrawLine(r, 10, 10, 300, 10);
        SDL_RenderDrawLine(r, 10, 20, 10, 200);
        d = (SDL_Rect){ 40, 40, 100, 80 }; SDL_RenderDrawRect(r, &d);
        break;
    }
}

static int comparar(const Uint32 *a, const Uint32 *b, int *maximo)
{
    int n = 0, mx = 0;
    for (int i = 0; i < L * A; i++) {
        int dm = 0;
        for (int s = 0; s < 24; s += 8) {
            int d = (int)((a[i] >> s) & 255) - (int)((b[i] >> s) & 255);
            if (d < 0) d = -d;
            if (d > dm) dm = d;
        }
        if (dm > 8) n++;
        if (dm > mx) mx = dm;
    }
    *maximo = mx;
    return n;
}

int principal(int argc, char **argv)
{
    int cuadros = 60;
    if (argc > 1) { cuadros = 0; for (const char *s = argv[1]; *s >= '0' && *s <= '9'; s++) cuadros = cuadros * 10 + (*s - '0'); }
    sagui_sdl2_nome = "SDL2GPU";
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    SDL_Window *ven = SDL_CreateWindow("sdl2gpu", 0, 0, L, A, 0);
    if (!ven) return 2;
    SDL_Renderer *ren = SDL_CreateRenderer(ven, -1, SDL_RENDERER_ACCELERATED);
    SDL_Surface *sup = SDL_CreateRGBSurfaceWithFormat(0, L, A, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *sw = sup ? SDL_CreateSoftwareRenderer(sup) : NULL;
    if (!ren || !sw) return 3;
    SDL_RendererInfo info;
    SDL_GetRendererInfo(ren, &info);
    registrar("[SDL2GPU] renderizador %s gpu=%d", info.name, (info.flags & SDL_RENDERER_ACCELERATED) ? 1 : 0);

    static Uint32 pa[L * A], pb[L * A];
    SDL_Texture *ta_sw = textura_gradiente(sw, 0), *tb_sw = textura_gradiente(sw, 1);
    SDL_Texture *ta_ac = textura_gradiente(ren, 0), *tb_ac = textura_gradiente(ren, 1);
    if (!ta_sw || !tb_sw || !ta_ac || !tb_ac) return 4;
    int mal = 0;
    for (int e = 0; e < N_ESCENAS; e++) {
        tex_a = ta_sw; tex_b = tb_sw;
        escena(sw, e);
        SDL_RenderReadPixels(sw, NULL, SDL_PIXELFORMAT_ARGB8888, pa, L * 4);
        tex_a = ta_ac; tex_b = tb_ac;
        escena(ren, e);
        SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, pb, L * 4);
        SDL_RenderPresent(ren);
        int mx;
        int n = comparar(pa, pb, &mx);
        registrar("[SDL2GPU] escena %s: %d pixeles distintos de %d (max %d)", nomes[e], n, L * A, mx);
        mal += n;
    }
    /* Tiempo: la escena 6 (copias con mezcla) y la 4 (escalada), cuadros seguidos. */
    unsigned t0 = SDL_GetTicks();
    for (int f = 0; f < cuadros; f++) {
        tex_a = ta_sw; tex_b = tb_sw;
        escena(sw, (f & 1) ? 4 : 6);
    }
    unsigned ms_sw = SDL_GetTicks() - t0;
    t0 = SDL_GetTicks();
    for (int f = 0; f < cuadros; f++) {
        tex_a = ta_ac; tex_b = tb_ac;
        escena(ren, (f & 1) ? 4 : 6);
        SDL_RenderPresent(ren);
    }
    unsigned ms_ac = SDL_GetTicks() - t0;
    registrar("[SDL2GPU] cuadros: %d, software %u ms, acelerado %u ms", cuadros, ms_sw, ms_ac);
    registrar("[SDL2GPU] total de pixeles distintos: %d", mal);
    registrar("[SDL2GPU] fin");
    SDL_Quit();
    return 0;
}
