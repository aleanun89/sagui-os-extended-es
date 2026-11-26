/* SDL.h - la SDL 2 del SaguiOS para los programas portados (D230).
 *
 * Esta cabecera es nuestra: el SaguiOS no tiene SDL. Los nombres, los tipos y
 * los valores de las constantes son los de SDL 2.0 para que el código de los
 * programas compile sin cambios; lo que hay detrás está en sdl2_sagui.c,
 * sobre las ventanas, el teclado, el ratón y el sonido del sistema. Es la
 * hermana de la SDL 1.2 común (userland/libsdl12, D228) para lo que se
 * escribió para SDL 2 (Vanilla Conquer, el primero). Un subconjunto:
 *
 *   - una ventana (SDL_CreateWindow) con su «renderer» de software, texturas
 *     de 32 bits (ARGB8888/RGB888) y la superficie de la ventana
 *     (SDL_GetWindowSurface), escalada a la ventana del sistema y, a pantalla
 *     completa, con bandas para guardar la proporción (4:3 para 320x200 y
 *     640x400, como la VGA, D204);
 *   - superficies de 8 bits con paleta y de 32 bits, blits entre ellas (de 8
 *     a 32 por la paleta), color clave y recorte;
 *   - eventos de teclado (con códigos de rastreo de SDL 2 y texto), ratón
 *     (en coordenadas de la ventana del programa), rueda, ventana y salida;
 *   - audio por llamada de vuelta o por cola (SDL_QueueAudio): SDL_PauseAudio(0)
 *     arranca un hilo de audio propio que llama a la llamada de vuelta con el
 *     cerrojo del dispositivo tomado (SDL_LockAudio es de verdad; D393).
 *     SDL_SAGUI_AUDIO_HILO=0 vuelve a moverlo desde el hilo principal
 *     (SDL_PollEvent, SDL_Delay y SDL_RenderPresent);
 *   - hilos de verdad (SDL_CreateThread sobre los hilos del sistema), cerrojos
 *     recursivos, condiciones, semáforos y atómicas (D393). Un hilo de trabajo
 *     no debe llamar a vídeo, eventos ni audio;
 *   - OpenGL ES 1.1 (SDL_GL_*, sobre EGL y SaguiGL: sdl2_gl.c, D425) y renderizador
 *     opcional por GPU (D288), con presentación a pantalla completa sin pasar por
 *     la RAM (D432);
 *   - reloj, pistas (hints), rutas (SDL_GetBasePath, SDL_GetPrefPath).
 *
 * No hay: temporizadores con llamada de vuelta (SDL_AddTimer devuelve 0), mandos
 * (ninguno conectado), ratón relativo (SDL_SetRelativeMouseMode devuelve -1 al
 * pedirlo), cursores propios (los de los programas se aceptan y no se usan;
 * SDL_ShowCursor(SDL_DISABLE) sí oculta el del sistema sobre la ventana, D230).
 * De la SDL original solo se conserva la interfaz.
 */
#ifndef SAGUI_SDL2_H
#define SAGUI_SDL2_H

#include <stddef.h>
#include <stdint.h>
/* La SDL de verdad (SDL_stdinc.h) trae estas cabeceras y los programas lo
 * dan por hecho. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- tipos ---- */
typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;
typedef uint64_t Uint64;
typedef int64_t  Sint64;
typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#define SDL_arraysize(a) (sizeof(a) / sizeof((a)[0]))
#define SDL_min(x, y) (((x) < (y)) ? (x) : (y))
#define SDL_max(x, y) (((x) > (y)) ? (x) : (y))
#define SDL_clamp(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))
#define SDL_zero(x) memset(&(x), 0, sizeof((x)))
#define SDL_memset memset
#define SDL_memcpy memcpy
#define SDL_memmove memmove
#define SDL_memcmp memcmp
#define SDL_strlen strlen
#define SDL_strcmp strcmp
#define SDL_strncmp strncmp
#define SDL_strlcpy sagui_sdl_strlcpy
#define SDL_snprintf snprintf
#define SDL_vsnprintf vsnprintf
#define SDL_atoi atoi
#define SDL_strtol strtol
#define SDL_abs abs
#define SDL_toupper toupper
#define SDL_tolower tolower
#define SDL_qsort qsort
#define SDL_calloc calloc
#define SDL_realloc realloc
size_t sagui_sdl_strlcpy(char *dst, const char *src, size_t n);
void *SDL_malloc(size_t n);
void  SDL_free(void *p);
char *SDL_getenv(const char *nome);
char *SDL_strdup(const char *s);

/* ---- orden de los bytes (el procesador es pequeño) ---- */
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN
#define SDL_Swap16(X) ((Uint16)((((Uint16)(X)) << 8) | (((Uint16)(X)) >> 8)))
#define SDL_Swap32(X) ((Uint32)((((Uint32)(X)) << 24) | ((((Uint32)(X)) & 0xFF00u) << 8) | \
                                ((((Uint32)(X)) >> 8) & 0xFF00u) | (((Uint32)(X)) >> 24)))
#define SDL_SwapLE16(X) (X)
#define SDL_SwapLE32(X) (X)
#define SDL_SwapBE16(X) SDL_Swap16(X)
#define SDL_SwapBE32(X) SDL_Swap32(X)

/* ---- versión ---- */
typedef struct SDL_version { Uint8 major, minor, patch; } SDL_version;
#define SDL_MAJOR_VERSION 2
#define SDL_MINOR_VERSION 0
#define SDL_PATCHLEVEL    22
#define SDL_VERSION(x) do { (x)->major = SDL_MAJOR_VERSION; (x)->minor = SDL_MINOR_VERSION; \
                            (x)->patch = SDL_PATCHLEVEL; } while (0)
#define SDL_VERSIONNUM(X, Y, Z) ((X) * 1000 + (Y) * 100 + (Z))
#define SDL_COMPILEDVERSION SDL_VERSIONNUM(SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL)
#define SDL_VERSION_ATLEAST(X, Y, Z) (SDL_COMPILEDVERSION >= SDL_VERSIONNUM(X, Y, Z))
void SDL_GetVersion(SDL_version *v);
const char *SDL_GetPlatform(void);

/* ---- inicio ---- */
#define SDL_INIT_TIMER          0x00000001u
#define SDL_INIT_AUDIO          0x00000010u
#define SDL_INIT_VIDEO          0x00000020u
#define SDL_INIT_JOYSTICK       0x00000200u
#define SDL_INIT_HAPTIC         0x00001000u
#define SDL_INIT_GAMECONTROLLER 0x00002000u
#define SDL_INIT_EVENTS         0x00004000u
#define SDL_INIT_SENSOR         0x00008000u
#define SDL_INIT_NOPARACHUTE    0x00100000u
#define SDL_INIT_EVERYTHING     0x0000F231u
int    SDL_Init(Uint32 flags);
int    SDL_InitSubSystem(Uint32 flags);
void   SDL_QuitSubSystem(Uint32 flags);
Uint32 SDL_WasInit(Uint32 flags);
void   SDL_Quit(void);
void   SDL_SetMainReady(void);

/* ---- errores y registro ---- */
const char *SDL_GetError(void);
int  SDL_SetError(const char *fmt, ...);
void SDL_ClearError(void);
void SDL_Log(const char *fmt, ...);
#define SDL_LogInfo(cat, ...)  SDL_Log(__VA_ARGS__)
#define SDL_LogWarn(cat, ...)  SDL_Log(__VA_ARGS__)
#define SDL_LogError(cat, ...) SDL_Log(__VA_ARGS__)
#define SDL_LogDebug(cat, ...) SDL_Log(__VA_ARGS__)
#define SDL_LOG_CATEGORY_APPLICATION 0

/* ---- pistas ---- */
typedef enum { SDL_HINT_DEFAULT, SDL_HINT_NORMAL, SDL_HINT_OVERRIDE } SDL_HintPriority;
#define SDL_HINT_AUDIO_RESAMPLING_MODE "SDL_AUDIO_RESAMPLING_MODE"
#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"
#define SDL_HINT_RENDER_DRIVER        "SDL_RENDER_DRIVER"
#define SDL_HINT_RENDER_VSYNC         "SDL_RENDER_VSYNC"
#define SDL_HINT_OPENGL_ES_DRIVER     "SDL_OPENGL_ES_DRIVER"   /* Brumacanto (D444): se acepta y se ignora (siempre EGL/SaguiGL) */
#define SDL_HINT_SAGUI_GL_ESCALA      "SDL_SAGUI_GL_ESCALA"    /* D451 (SaguiOS): escala de dibujo de OpenGL ES, 25..100 */
#define SDL_HINT_SAGUI_FPS_MAX        "SDL_SAGUI_FPS_MAX"      /* D476 (SaguiOS): tope de cuadros por segundo (0 = sin tope; 30, 60...) */
void sagui_sdl_fps_limitar(void);
#define SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS "SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS"
#define SDL_HINT_MOUSE_RELATIVE_MODE_WARP "SDL_MOUSE_RELATIVE_MODE_WARP"
SDL_bool SDL_SetHint(const char *nome, const char *valor);
SDL_bool SDL_SetHintWithPriority(const char *nome, const char *valor, SDL_HintPriority p);
const char *SDL_GetHint(const char *nome);

/* ---- rectángulos ---- */
typedef struct SDL_Point { int x, y; } SDL_Point;
typedef struct SDL_FPoint { float x, y; } SDL_FPoint;
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef struct SDL_FRect { float x, y, w, h; } SDL_FRect;
SDL_bool SDL_HasIntersection(const SDL_Rect *a, const SDL_Rect *b);
SDL_bool SDL_IntersectRect(const SDL_Rect *a, const SDL_Rect *b, SDL_Rect *r);
static inline SDL_bool SDL_RectEmpty(const SDL_Rect *r) { return (!r || r->w <= 0 || r->h <= 0) ? SDL_TRUE : SDL_FALSE; }
static inline SDL_bool SDL_PointInRect(const SDL_Point *p, const SDL_Rect *r)
{
    return (p->x >= r->x && p->x < r->x + r->w && p->y >= r->y && p->y < r->y + r->h) ? SDL_TRUE : SDL_FALSE;
}

/* ---- formatos de píxel (los valores de SDL 2) ---- */
#define SDL_PIXELTYPE_UNKNOWN  0
#define SDL_PIXELTYPE_INDEX1   1
#define SDL_PIXELTYPE_INDEX4   2
#define SDL_PIXELTYPE_INDEX8   3
#define SDL_PIXELTYPE_PACKED8  4
#define SDL_PIXELTYPE_PACKED16 5
#define SDL_PIXELTYPE_PACKED32 6
#define SDL_PIXELTYPE_ARRAYU8  7
#define SDL_PACKEDORDER_NONE 0
#define SDL_PACKEDORDER_XRGB 1
#define SDL_PACKEDORDER_RGBX 2
#define SDL_PACKEDORDER_ARGB 3
#define SDL_PACKEDORDER_RGBA 4
#define SDL_PACKEDORDER_XBGR 5
#define SDL_PACKEDORDER_BGRX 6
#define SDL_PACKEDORDER_ABGR 7
#define SDL_PACKEDORDER_BGRA 8
#define SDL_ARRAYORDER_RGB 1
#define SDL_ARRAYORDER_BGR 4
#define SDL_PACKEDLAYOUT_1555 3
#define SDL_PACKEDLAYOUT_565  5
#define SDL_PACKEDLAYOUT_8888 6
#define SDL_DEFINE_PIXELFORMAT(type, order, layout, bits, bytes) \
    ((1u << 28) | ((Uint32)(type) << 24) | ((Uint32)(order) << 20) | ((Uint32)(layout) << 16) | \
     ((Uint32)(bits) << 8) | ((Uint32)(bytes) << 0))
#define SDL_PIXELFLAG(X)     (((X) >> 28) & 0x0F)
#define SDL_PIXELTYPE(X)     (((X) >> 24) & 0x0F)
#define SDL_PIXELORDER(X)    (((X) >> 20) & 0x0F)
#define SDL_PIXELLAYOUT(X)   (((X) >> 16) & 0x0F)
#define SDL_BITSPERPIXEL(X)  (((X) >> 8) & 0xFF)
#define SDL_BYTESPERPIXEL(X) ((X) & 0xFF)
#define SDL_ISPIXELFORMAT_INDEXED(f) (SDL_PIXELTYPE(f) >= SDL_PIXELTYPE_INDEX1 && SDL_PIXELTYPE(f) <= SDL_PIXELTYPE_INDEX8)
#define SDL_ISPIXELFORMAT_ALPHA(f) (SDL_PIXELTYPE(f) == SDL_PIXELTYPE_PACKED32 && \
    (SDL_PIXELORDER(f) == SDL_PACKEDORDER_ARGB || SDL_PIXELORDER(f) == SDL_PACKEDORDER_RGBA || \
     SDL_PIXELORDER(f) == SDL_PACKEDORDER_ABGR || SDL_PIXELORDER(f) == SDL_PACKEDORDER_BGRA))

#define SDL_PIXELFORMAT_UNKNOWN  0u
#define SDL_PIXELFORMAT_INDEX8   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX8, 0, 0, 8, 1)
#define SDL_PIXELFORMAT_RGB555   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_1555, 15, 2)
#define SDL_PIXELFORMAT_BGR555   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_1555, 15, 2)
#define SDL_PIXELFORMAT_RGB565   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_565, 16, 2)
#define SDL_PIXELFORMAT_BGR565   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED16, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_565, 16, 2)
#define SDL_PIXELFORMAT_RGB24    SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_RGB, 0, 24, 3)
#define SDL_PIXELFORMAT_BGR24    SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_BGR, 0, 24, 3)
#define SDL_PIXELFORMAT_RGB888   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XRGB, SDL_PACKEDLAYOUT_8888, 24, 4)
#define SDL_PIXELFORMAT_XRGB8888 SDL_PIXELFORMAT_RGB888
#define SDL_PIXELFORMAT_BGR888   SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_XBGR, SDL_PACKEDLAYOUT_8888, 24, 4)
#define SDL_PIXELFORMAT_XBGR8888 SDL_PIXELFORMAT_BGR888
#define SDL_PIXELFORMAT_ARGB8888 SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ARGB, SDL_PACKEDLAYOUT_8888, 32, 4)
#define SDL_PIXELFORMAT_RGBA8888 SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_RGBA, SDL_PACKEDLAYOUT_8888, 32, 4)
#define SDL_PIXELFORMAT_ABGR8888 SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_ABGR, SDL_PACKEDLAYOUT_8888, 32, 4)
#define SDL_PIXELFORMAT_BGRA8888 SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_PACKED32, SDL_PACKEDORDER_BGRA, SDL_PACKEDLAYOUT_8888, 32, 4)
/* Los formatos «por orden de bytes» (x86 pequeño: el byte 0 es el primero del nombre).
 * SDL_PIXELFORMAT_BGRA32 (bytes B, G, R, A) es lo mismo que ARGB8888 de 32 bits. */
#define SDL_PIXELFORMAT_ARGB32 SDL_PIXELFORMAT_BGRA8888
#define SDL_PIXELFORMAT_BGRA32 SDL_PIXELFORMAT_ARGB8888
#define SDL_PIXELFORMAT_ABGR32 SDL_PIXELFORMAT_RGBA8888
#define SDL_PIXELFORMAT_RGBA32 SDL_PIXELFORMAT_ABGR8888
const char *SDL_GetPixelFormatName(Uint32 formato);

typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;
#define SDL_Colour SDL_Color
typedef struct SDL_Palette {
    int ncolors;
    SDL_Color *colors;
    Uint32 version;
    int refcount;
} SDL_Palette;

typedef struct SDL_PixelFormat {
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel, BytesPerPixel;
    Uint8 padding[2];
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint8 Rloss, Gloss, Bloss, Aloss;
    Uint8 Rshift, Gshift, Bshift, Ashift;
    int refcount;
    struct SDL_PixelFormat *next;
} SDL_PixelFormat;

SDL_Palette *SDL_AllocPalette(int ncores);
void SDL_FreePalette(SDL_Palette *p);
int  SDL_SetPaletteColors(SDL_Palette *p, const SDL_Color *cores, int primeiro, int n);
Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *f, Uint8 *r, Uint8 *g, Uint8 *b);
void SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat *f, Uint8 *r, Uint8 *g, Uint8 *b, Uint8 *a);
SDL_PixelFormat *SDL_AllocFormat(Uint32 formato);
void SDL_FreeFormat(SDL_PixelFormat *f);

/* ---- superficies ---- */
#define SDL_SWSURFACE 0
#define SDL_PREALLOC  0x00000001
#define SDL_RLEACCEL  0x00000002
#define SDL_DONTFREE  0x00000004
typedef enum { SDL_BLENDMODE_NONE = 0, SDL_BLENDMODE_BLEND = 1, SDL_BLENDMODE_ADD = 2, SDL_BLENDMODE_MOD = 4,
               SDL_BLENDMODE_MUL = 8, SDL_BLENDMODE_INVALID = 0x7FFFFFFF } SDL_BlendMode;
/* D436: modos de mezcla compuestos (como en la SDL 2.0.6 y siguientes; el renderizador por GPU lleva los que caben
 * en gpu3d v1: factores ZERO, ONE, SRC_ALPHA y ONE_MINUS_SRC_ALPHA con la suma; el resto va por software) */
typedef enum {
    SDL_BLENDOPERATION_ADD = 1, SDL_BLENDOPERATION_SUBTRACT = 2, SDL_BLENDOPERATION_REV_SUBTRACT = 3,
    SDL_BLENDOPERATION_MINIMUM = 4, SDL_BLENDOPERATION_MAXIMUM = 5
} SDL_BlendOperation;
typedef enum {
    SDL_BLENDFACTOR_ZERO = 1, SDL_BLENDFACTOR_ONE = 2, SDL_BLENDFACTOR_SRC_COLOR = 3, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR = 4,
    SDL_BLENDFACTOR_SRC_ALPHA = 5, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA = 6, SDL_BLENDFACTOR_DST_COLOR = 7,
    SDL_BLENDFACTOR_ONE_MINUS_DST_COLOR = 8, SDL_BLENDFACTOR_DST_ALPHA = 9, SDL_BLENDFACTOR_ONE_MINUS_DST_ALPHA = 10
} SDL_BlendFactor;
SDL_BlendMode SDL_ComposeCustomBlendMode(SDL_BlendFactor orig_cor, SDL_BlendFactor dest_cor, SDL_BlendOperation op_cor,
                                         SDL_BlendFactor orig_alfa, SDL_BlendFactor dest_alfa, SDL_BlendOperation op_alfa);

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    int pitch;
    void *pixels;
    void *userdata;
    int locked;
    void *list_blitmap;
    SDL_Rect clip_rect;
    void *map;
    int refcount;
    /* Lo de la SDL del SaguiOS (no lo mira el programa). */
    Uint32 sagui_chave;      /* color clave */
    int sagui_tem_chave;
    Uint8 sagui_alfa_mod;    /* SDL_SetSurfaceAlphaMod */
    Uint8 sagui_alfa_set;    /* 1 = hay alfa global */
    Uint8 sagui_blend_set;   /* 1 = el programa fijó sagui_blend */
    int   sagui_blend;       /* SDL_BlendMode elegido */
} SDL_Surface;

#define SDL_MUSTLOCK(s) 0
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int largura, int altura, int bits,
                                  Uint32 r, Uint32 g, Uint32 b, Uint32 a);
SDL_Surface *SDL_CreateRGBSurfaceWithFormat(Uint32 flags, int largura, int altura, int bits, Uint32 formato);
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int largura, int altura, int bits, int pitch,
                                      Uint32 r, Uint32 g, Uint32 b, Uint32 a);
SDL_Surface *SDL_CreateRGBSurfaceWithFormatFrom(void *pixels, int largura, int altura, int bits, int pitch,
                                                Uint32 formato);
void SDL_FreeSurface(SDL_Surface *s);
int  SDL_SaveBMP(SDL_Surface *s, const char *ficheiro);   /* BMP de 24 bits (en la SDL de verdad es una macro de SDL_SaveBMP_RW) */
int  SDL_SetSurfacePalette(SDL_Surface *s, SDL_Palette *p);
int  SDL_LockSurface(SDL_Surface *s);
void SDL_UnlockSurface(SDL_Surface *s);
SDL_bool SDL_SetClipRect(SDL_Surface *s, const SDL_Rect *r);
void SDL_GetClipRect(SDL_Surface *s, SDL_Rect *r);
int  SDL_SetColorKey(SDL_Surface *s, int ativo, Uint32 chave);
int  SDL_GetColorKey(SDL_Surface *s, Uint32 *chave);
int  SDL_SetSurfaceBlendMode(SDL_Surface *s, SDL_BlendMode m);
int  SDL_SetSurfaceAlphaMod(SDL_Surface *s, Uint8 alfa);   /* D374: se guarda en la superficie */
int  SDL_GetSurfaceAlphaMod(SDL_Surface *s, Uint8 *alfa);
int  SDL_SetSurfaceRLE(SDL_Surface *s, int rle);
int  SDL_FillRect(SDL_Surface *d, const SDL_Rect *r, Uint32 cor);
int  SDL_FillRects(SDL_Surface *d, const SDL_Rect *rs, int n, Uint32 cor);
int  SDL_UpperBlit(SDL_Surface *o, const SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd);
#define SDL_BlitSurface SDL_UpperBlit
int  SDL_LowerBlit(SDL_Surface *o, SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd);
int  SDL_UpperBlitScaled(SDL_Surface *o, const SDL_Rect *ro, SDL_Surface *d, SDL_Rect *rd);
#define SDL_BlitScaled SDL_UpperBlitScaled
SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *s, Uint32 formato, Uint32 flags);
SDL_Surface *SDL_ConvertSurface(SDL_Surface *s, const SDL_PixelFormat *f, Uint32 flags);

/* ---- ventanas ---- */
typedef struct SDL_Window SDL_Window;
#define SDL_WINDOW_FULLSCREEN         0x00000001u
#define SDL_WINDOW_OPENGL             0x00000002u
#define SDL_WINDOW_SHOWN              0x00000004u
#define SDL_WINDOW_HIDDEN             0x00000008u
#define SDL_WINDOW_BORDERLESS         0x00000010u
#define SDL_WINDOW_RESIZABLE          0x00000020u
#define SDL_WINDOW_MINIMIZED          0x00000040u
#define SDL_WINDOW_MAXIMIZED          0x00000080u
#define SDL_WINDOW_INPUT_GRABBED      0x00000100u
#define SDL_WINDOW_INPUT_FOCUS        0x00000200u
#define SDL_WINDOW_MOUSE_FOCUS        0x00000400u
#define SDL_WINDOW_FULLSCREEN_DESKTOP (SDL_WINDOW_FULLSCREEN | 0x00001000u)
#define SDL_WINDOW_ALLOW_HIGHDPI      0x00002000u
#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000
#define SDL_WINDOWPOS_CENTERED  0x2FFF0000
SDL_Window *SDL_CreateWindow(const char *titulo, int x, int y, int largura, int altura, Uint32 flags);
void   SDL_DestroyWindow(SDL_Window *w);
void   SDL_SetWindowTitle(SDL_Window *w, const char *titulo);
const char *SDL_GetWindowTitle(SDL_Window *w);
void   SDL_GetWindowSize(SDL_Window *w, int *largura, int *altura);
void   SDL_SetWindowSize(SDL_Window *w, int largura, int altura);
void   SDL_SetWindowMinimumSize(SDL_Window *w, int largura, int altura);
void   SDL_GetWindowPosition(SDL_Window *w, int *x, int *y);
void   SDL_SetWindowPosition(SDL_Window *w, int x, int y);
int    SDL_SetWindowFullscreen(SDL_Window *w, Uint32 flags);
Uint32 SDL_GetWindowFlags(SDL_Window *w);
Uint32 SDL_GetWindowPixelFormat(SDL_Window *w);
Uint32 SDL_GetWindowID(SDL_Window *w);
void   SDL_SetWindowGrab(SDL_Window *w, SDL_bool agarrar);
SDL_bool SDL_GetWindowGrab(SDL_Window *w);
void   SDL_ShowWindow(SDL_Window *w);
void   SDL_HideWindow(SDL_Window *w);
void   SDL_RaiseWindow(SDL_Window *w);
void   SDL_SetWindowIcon(SDL_Window *w, SDL_Surface *icone);
void   SDL_SetWindowResizable(SDL_Window *w, SDL_bool sim);
SDL_Surface *SDL_GetWindowSurface(SDL_Window *w);
int    SDL_UpdateWindowSurface(SDL_Window *w);
int    SDL_UpdateWindowSurfaceRects(SDL_Window *w, const SDL_Rect *rs, int n);
SDL_Window *SDL_GetKeyboardFocus(void);
SDL_Window *SDL_GetMouseFocus(void);

typedef struct SDL_DisplayMode { Uint32 format; int w, h, refresh_rate; void *driverdata; } SDL_DisplayMode;
int SDL_GetNumVideoDisplays(void);
int SDL_GetDesktopDisplayMode(int tela, SDL_DisplayMode *m);
int SDL_GetWindowDisplayMode(SDL_Window *w, SDL_DisplayMode *m);
int SDL_GetCurrentDisplayMode(int tela, SDL_DisplayMode *m);
int SDL_GetNumDisplayModes(int tela);
int SDL_GetDisplayMode(int tela, int indice, SDL_DisplayMode *m);
const char *SDL_GetCurrentVideoDriver(void);
int SDL_GetDisplayBounds(int tela, SDL_Rect *r);
void SDL_DisableScreenSaver(void);
void SDL_EnableScreenSaver(void);

/* Lo que es solo del SaguiOS: lo ajusta el programa antes de crear la
 * ventana. sagui_sdl2_nome va en las líneas del registro; sagui_sdl2_cheia_inicial
 * hace nacer la ventana a pantalla completa (como «--pantalla-completa» del
 * menú, D204); sagui_sdl2_escala amplía la ventana (2 = el doble). */
extern const char *sagui_sdl2_nome;
extern int sagui_sdl2_cheia_inicial;
extern int sagui_sdl2_escala;
extern int sagui_sdl2_medir;   /* D247: escribe el tiempo de cada fase por cuadro */
extern int sagui_sdl2_gpu;     /* D288: renderizador por GPU: 0 solo con SDL_RENDERER_ACCELERATED, 1 siempre, -1 nunca (o SAGUI_SDL2_GPU=0/1) */
/* D432: cuadros que este renderizador presentó directamente en la pantalla por la GPU y cuadros que, con la
 * GPU, tuvieron que volver por la RAM. Devuelve 1 si lo lleva la GPU, 0 si va por software. */
struct SDL_Renderer;
int sagui_sdl2_gpu_estado(struct SDL_Renderer *r, unsigned long *directos, unsigned long *por_ram);

/* ---- renderer de software ---- */
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
#define SDL_RENDERER_SOFTWARE      0x00000001u
#define SDL_RENDERER_ACCELERATED   0x00000002u
#define SDL_RENDERER_PRESENTVSYNC  0x00000004u
#define SDL_RENDERER_TARGETTEXTURE 0x00000008u
typedef struct SDL_RendererInfo {
    const char *name;
    Uint32 flags;
    Uint32 num_texture_formats;
    Uint32 texture_formats[16];
    int max_texture_width;
    int max_texture_height;
} SDL_RendererInfo;
typedef enum { SDL_TEXTUREACCESS_STATIC, SDL_TEXTUREACCESS_STREAMING, SDL_TEXTUREACCESS_TARGET } SDL_TextureAccess;
typedef enum { SDL_FLIP_NONE = 0, SDL_FLIP_HORIZONTAL = 1, SDL_FLIP_VERTICAL = 2 } SDL_RendererFlip;
int  SDL_GetNumRenderDrivers(void);
int  SDL_GetRenderDriverInfo(int indice, SDL_RendererInfo *info);
SDL_Renderer *SDL_CreateRenderer(SDL_Window *w, int indice, Uint32 flags);
SDL_Renderer *SDL_CreateSoftwareRenderer(SDL_Surface *s);
SDL_Renderer *SDL_GetRenderer(SDL_Window *w);
void SDL_DestroyRenderer(SDL_Renderer *r);
int  SDL_GetRendererInfo(SDL_Renderer *r, SDL_RendererInfo *info);
int  SDL_GetRendererOutputSize(SDL_Renderer *r, int *largura, int *altura);
int  SDL_RenderSetLogicalSize(SDL_Renderer *r, int largura, int altura);
void SDL_RenderGetLogicalSize(SDL_Renderer *r, int *largura, int *altura);
int  SDL_RenderSetIntegerScale(SDL_Renderer *r, SDL_bool sim);
int  SDL_RenderSetViewport(SDL_Renderer *r, const SDL_Rect *rect);
int  SDL_SetRenderDrawColor(SDL_Renderer *r, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 ca);
int  SDL_SetRenderDrawBlendMode(SDL_Renderer *r, SDL_BlendMode m);
int  SDL_RenderClear(SDL_Renderer *r);
int  SDL_RenderFillRect(SDL_Renderer *r, const SDL_Rect *rect);
int  SDL_RenderDrawRect(SDL_Renderer *r, const SDL_Rect *rect);
int  SDL_RenderDrawLine(SDL_Renderer *r, int x1, int y1, int x2, int y2);
int  SDL_RenderDrawPoint(SDL_Renderer *r, int x, int y);
int  SDL_RenderCopy(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *origem, const SDL_Rect *destino);
int  SDL_RenderCopyEx(SDL_Renderer *r, SDL_Texture *t, const SDL_Rect *origem, const SDL_Rect *destino,
                      double angulo, const SDL_Point *centro, SDL_RendererFlip espelho);
void SDL_RenderPresent(SDL_Renderer *r);
int  SDL_SetRenderTarget(SDL_Renderer *r, SDL_Texture *t);
int  SDL_RenderReadPixels(SDL_Renderer *r, const SDL_Rect *rect, Uint32 formato, void *pixels, int pitch);
SDL_Texture *SDL_CreateTexture(SDL_Renderer *r, Uint32 formato, int acesso, int largura, int altura);
SDL_Texture *SDL_CreateTextureFromSurface(SDL_Renderer *r, SDL_Surface *s);
void SDL_DestroyTexture(SDL_Texture *t);
int  SDL_QueryTexture(SDL_Texture *t, Uint32 *formato, int *acesso, int *largura, int *altura);
int  SDL_UpdateTexture(SDL_Texture *t, const SDL_Rect *rect, const void *pixels, int pitch);
int  SDL_LockTexture(SDL_Texture *t, const SDL_Rect *rect, void **pixels, int *pitch);
void SDL_UnlockTexture(SDL_Texture *t);
int  SDL_SetTextureBlendMode(SDL_Texture *t, SDL_BlendMode m);
int  SDL_SetTextureColorMod(SDL_Texture *t, Uint8 r, Uint8 g, Uint8 b);
int  SDL_SetTextureAlphaMod(SDL_Texture *t, Uint8 a);
int  SDL_GetTextureColorMod(SDL_Texture *t, Uint8 *r, Uint8 *g, Uint8 *b);
int  SDL_GetTextureAlphaMod(SDL_Texture *t, Uint8 *a);
int  SDL_GetTextureBlendMode(SDL_Texture *t, SDL_BlendMode *m);
/* D436: triángulos con color por vértice y textura (SDL 2.0.18); el recorte (SDL 2.0.4) */
typedef struct SDL_Vertex { SDL_FPoint position; SDL_Color color; SDL_FPoint tex_coord; } SDL_Vertex;
int  SDL_RenderGeometry(SDL_Renderer *r, SDL_Texture *t, const SDL_Vertex *vertices, int nv, const int *indices, int ni);
int  SDL_RenderSetClipRect(SDL_Renderer *r, const SDL_Rect *rect);
void SDL_RenderGetClipRect(SDL_Renderer *r, SDL_Rect *rect);
SDL_bool SDL_RenderIsClipEnabled(SDL_Renderer *r);

/* ---- ratón y cursor ---- */
typedef struct SDL_Cursor SDL_Cursor;
typedef enum { SDL_SYSTEM_CURSOR_ARROW, SDL_SYSTEM_CURSOR_IBEAM, SDL_SYSTEM_CURSOR_WAIT,
               SDL_SYSTEM_CURSOR_CROSSHAIR, SDL_SYSTEM_CURSOR_HAND = 11, SDL_NUM_SYSTEM_CURSORS = 12 } SDL_SystemCursor;
#define SDL_BUTTON(X)       (1u << ((X) - 1))
#define SDL_BUTTON_LEFT     1
#define SDL_BUTTON_MIDDLE   2
#define SDL_BUTTON_RIGHT    3
#define SDL_BUTTON_X1       4
#define SDL_BUTTON_X2       5
#define SDL_BUTTON_LMASK    SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK    SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK    SDL_BUTTON(SDL_BUTTON_RIGHT)
#define SDL_QUERY   -1
#define SDL_IGNORE   0
#define SDL_DISABLE  0
#define SDL_ENABLE   1
Uint32 SDL_GetMouseState(int *x, int *y);
Uint32 SDL_GetGlobalMouseState(int *x, int *y);
Uint32 SDL_GetRelativeMouseState(int *x, int *y);
void   SDL_WarpMouseInWindow(SDL_Window *w, int x, int y);
int    SDL_SetRelativeMouseMode(SDL_bool ativo);
SDL_bool SDL_GetRelativeMouseMode(void);
int    SDL_ShowCursor(int estado);
SDL_Cursor *SDL_CreateColorCursor(SDL_Surface *s, int hot_x, int hot_y);
SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id);
SDL_Cursor *SDL_CreateCursor(const Uint8 *dados, const Uint8 *mascara, int l, int a, int hot_x, int hot_y);
void   SDL_SetCursor(SDL_Cursor *c);
SDL_Cursor *SDL_GetCursor(void);
void   SDL_FreeCursor(SDL_Cursor *c);
int    SDL_CaptureMouse(SDL_bool sim);

/* ---- teclado: códigos de rastreo (los de USB HID, como SDL 2) ---- */
typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_B = 5, SDL_SCANCODE_C = 6, SDL_SCANCODE_D = 7, SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9, SDL_SCANCODE_G = 10, SDL_SCANCODE_H = 11, SDL_SCANCODE_I = 12, SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15, SDL_SCANCODE_M = 16, SDL_SCANCODE_N = 17, SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19, SDL_SCANCODE_Q = 20, SDL_SCANCODE_R = 21, SDL_SCANCODE_S = 22, SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24, SDL_SCANCODE_V = 25, SDL_SCANCODE_W = 26, SDL_SCANCODE_X = 27, SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_2 = 31, SDL_SCANCODE_3 = 32, SDL_SCANCODE_4 = 33, SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35, SDL_SCANCODE_7 = 36, SDL_SCANCODE_8 = 37, SDL_SCANCODE_9 = 38, SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42, SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44, SDL_SCANCODE_MINUS = 45, SDL_SCANCODE_EQUALS = 46, SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48, SDL_SCANCODE_BACKSLASH = 49, SDL_SCANCODE_NONUSHASH = 50,
    SDL_SCANCODE_SEMICOLON = 51, SDL_SCANCODE_APOSTROPHE = 52, SDL_SCANCODE_GRAVE = 53, SDL_SCANCODE_COMMA = 54,
    SDL_SCANCODE_PERIOD = 55, SDL_SCANCODE_SLASH = 56, SDL_SCANCODE_CAPSLOCK = 57,
    SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2 = 59, SDL_SCANCODE_F3 = 60, SDL_SCANCODE_F4 = 61, SDL_SCANCODE_F5 = 62,
    SDL_SCANCODE_F6 = 63, SDL_SCANCODE_F7 = 64, SDL_SCANCODE_F8 = 65, SDL_SCANCODE_F9 = 66, SDL_SCANCODE_F10 = 67,
    SDL_SCANCODE_F11 = 68, SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_PRINTSCREEN = 70, SDL_SCANCODE_SCROLLLOCK = 71, SDL_SCANCODE_PAUSE = 72, SDL_SCANCODE_INSERT = 73,
    SDL_SCANCODE_HOME = 74, SDL_SCANCODE_PAGEUP = 75, SDL_SCANCODE_DELETE = 76, SDL_SCANCODE_END = 77,
    SDL_SCANCODE_PAGEDOWN = 78, SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82, SDL_SCANCODE_NUMLOCKCLEAR = 83, SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86, SDL_SCANCODE_KP_PLUS = 87, SDL_SCANCODE_KP_ENTER = 88, SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_2 = 90, SDL_SCANCODE_KP_3 = 91, SDL_SCANCODE_KP_4 = 92, SDL_SCANCODE_KP_5 = 93,
    SDL_SCANCODE_KP_6 = 94, SDL_SCANCODE_KP_7 = 95, SDL_SCANCODE_KP_8 = 96, SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98, SDL_SCANCODE_KP_PERIOD = 99, SDL_SCANCODE_NONUSBACKSLASH = 100,
    SDL_SCANCODE_APPLICATION = 101, SDL_SCANCODE_SELECT = 119, SDL_SCANCODE_SYSREQ = 154, SDL_SCANCODE_CLEAR = 156, SDL_SCANCODE_PRIOR = 157,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226, SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229, SDL_SCANCODE_RALT = 230, SDL_SCANCODE_RGUI = 231,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

/* Los códigos de tecla: el carácter, o el de rastreo con el bit 30. */
typedef Sint32 SDL_Keycode;
#define SDLK_SCANCODE_MASK (1 << 30)
#define SDL_SCANCODE_TO_KEYCODE(X) ((X) | SDLK_SCANCODE_MASK)
enum {
    SDLK_UNKNOWN = 0, SDLK_RETURN = '\r', SDLK_ESCAPE = 27, SDLK_BACKSPACE = '\b', SDLK_TAB = '\t',
    SDLK_SPACE = ' ', SDLK_EXCLAIM = '!', SDLK_QUOTEDBL = '"', SDLK_HASH = '#', SDLK_PERCENT = '%',
    SDLK_DOLLAR = '$', SDLK_AMPERSAND = '&', SDLK_QUOTE = '\'', SDLK_LEFTPAREN = '(', SDLK_RIGHTPAREN = ')',
    SDLK_ASTERISK = '*', SDLK_PLUS = '+', SDLK_COMMA = ',', SDLK_MINUS = '-', SDLK_PERIOD = '.', SDLK_SLASH = '/',
    SDLK_0 = '0', SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_COLON = ':', SDLK_SEMICOLON = ';', SDLK_LESS = '<', SDLK_EQUALS = '=', SDLK_GREATER = '>',
    SDLK_QUESTION = '?', SDLK_AT = '@', SDLK_LEFTBRACKET = '[', SDLK_BACKSLASH = '\\', SDLK_RIGHTBRACKET = ']',
    SDLK_CARET = '^', SDLK_UNDERSCORE = '_', SDLK_BACKQUOTE = '`',
    SDLK_a = 'a', SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j, SDLK_k, SDLK_l,
    SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
    SDLK_DELETE = 127,
    SDLK_CAPSLOCK = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_CAPSLOCK),
    SDLK_F1 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F1), SDLK_F2 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F2),
    SDLK_F3 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F3), SDLK_F4 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F4),
    SDLK_F5 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F5), SDLK_F6 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F6),
    SDLK_F7 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F7), SDLK_F8 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F8),
    SDLK_F9 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F9), SDLK_F10 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F10),
    SDLK_F11 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F11), SDLK_F12 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_F12),
    SDLK_PRINTSCREEN = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PRINTSCREEN),
    SDLK_SYSREQ = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SYSREQ), SDLK_PRIOR = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PRIOR),
    SDLK_SCROLLLOCK = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_SCROLLLOCK),
    SDLK_PAUSE = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAUSE), SDLK_INSERT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INSERT),
    SDLK_HOME = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_HOME), SDLK_PAGEUP = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEUP),
    SDLK_END = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_END), SDLK_PAGEDOWN = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_PAGEDOWN),
    SDLK_RIGHT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RIGHT), SDLK_LEFT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LEFT),
    SDLK_DOWN = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_DOWN), SDLK_UP = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_UP),
    SDLK_NUMLOCKCLEAR = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_NUMLOCKCLEAR),
    SDLK_KP_DIVIDE = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_DIVIDE),
    SDLK_KP_MULTIPLY = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MULTIPLY),
    SDLK_KP_MINUS = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_MINUS),
    SDLK_KP_PLUS = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_PLUS),
    SDLK_KP_ENTER = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_ENTER),
    SDLK_KP_0 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_0), SDLK_KP_1 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_1),
    SDLK_KP_2 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_2), SDLK_KP_3 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_3),
    SDLK_KP_4 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_4), SDLK_KP_5 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_5),
    SDLK_KP_6 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_6), SDLK_KP_7 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_7),
    SDLK_KP_8 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_8), SDLK_KP_9 = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_9),
    SDLK_KP_PERIOD = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_KP_PERIOD),
    SDLK_LCTRL = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LCTRL), SDLK_LSHIFT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LSHIFT),
    SDLK_LALT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LALT), SDLK_LGUI = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_LGUI),
    SDLK_RCTRL = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RCTRL), SDLK_RSHIFT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RSHIFT),
    SDLK_RALT = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RALT), SDLK_RGUI = SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_RGUI)
};

typedef enum {
    KMOD_NONE = 0x0000, KMOD_LSHIFT = 0x0001, KMOD_RSHIFT = 0x0002, KMOD_LCTRL = 0x0040, KMOD_RCTRL = 0x0080,
    KMOD_LALT = 0x0100, KMOD_RALT = 0x0200, KMOD_LGUI = 0x0400, KMOD_RGUI = 0x0800, KMOD_NUM = 0x1000,
    KMOD_CAPS = 0x2000, KMOD_MODE = 0x4000,
    KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL, KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT,
    KMOD_ALT = KMOD_LALT | KMOD_RALT, KMOD_GUI = KMOD_LGUI | KMOD_RGUI
} SDL_Keymod;

typedef struct SDL_Keysym {
    SDL_Scancode scancode;
    SDL_Keycode sym;
    Uint16 mod;
    Uint32 unused;
} SDL_Keysym;

const Uint8 *SDL_GetKeyboardState(int *n);
SDL_Keymod SDL_GetModState(void);
void SDL_SetModState(SDL_Keymod m);
SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode s);
SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode k);
const char *SDL_GetKeyName(SDL_Keycode k);
const char *SDL_GetScancodeName(SDL_Scancode s);
void SDL_StartTextInput(void);
void SDL_StopTextInput(void);
SDL_bool SDL_IsTextInputActive(void);

/* ---- eventos ---- */
#define SDL_RELEASED 0
#define SDL_PRESSED  1
typedef enum {
    SDL_FIRSTEVENT = 0,
    SDL_QUIT = 0x100,
    SDL_WINDOWEVENT = 0x200, SDL_SYSWMEVENT,
    SDL_KEYDOWN = 0x300, SDL_KEYUP, SDL_TEXTEDITING, SDL_TEXTINPUT, SDL_KEYMAPCHANGED,
    SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL,
    SDL_JOYAXISMOTION = 0x600, SDL_JOYBALLMOTION, SDL_JOYHATMOTION, SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP,
    SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED,
    SDL_CONTROLLERAXISMOTION = 0x650, SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP,
    SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED, SDL_CONTROLLERDEVICEREMAPPED,
    SDL_FINGERDOWN = 0x700, SDL_FINGERUP, SDL_FINGERMOTION,
    SDL_CLIPBOARDUPDATE = 0x900,
    SDL_DROPFILE = 0x1000,
    SDL_AUDIODEVICEADDED = 0x1100, SDL_AUDIODEVICEREMOVED,
    SDL_RENDER_TARGETS_RESET = 0x2000, SDL_RENDER_DEVICE_RESET,
    SDL_USEREVENT = 0x8000,
    SDL_LASTEVENT = 0xFFFF
} SDL_EventType;

typedef enum {
    SDL_WINDOWEVENT_NONE, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_EXPOSED,
    SDL_WINDOWEVENT_MOVED, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED, SDL_WINDOWEVENT_MINIMIZED,
    SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED, SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE,
    SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_CLOSE
} SDL_WindowEventID;

typedef Sint32 SDL_JoystickID;
typedef struct SDL_CommonEvent { Uint32 type, timestamp; } SDL_CommonEvent;
typedef struct SDL_WindowEvent {
    Uint32 type, timestamp, windowID;
    Uint8 event, padding1, padding2, padding3;
    Sint32 data1, data2;
} SDL_WindowEvent;
typedef struct SDL_KeyboardEvent {
    Uint32 type, timestamp, windowID;
    Uint8 state, repeat, padding2, padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;
#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32
typedef struct SDL_TextInputEvent {
    Uint32 type, timestamp, windowID;
    char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
} SDL_TextInputEvent;
typedef struct SDL_MouseMotionEvent {
    Uint32 type, timestamp, windowID, which, state;
    Sint32 x, y, xrel, yrel;
} SDL_MouseMotionEvent;
typedef struct SDL_MouseButtonEvent {
    Uint32 type, timestamp, windowID, which;
    Uint8 button, state, clicks, padding1;
    Sint32 x, y;
} SDL_MouseButtonEvent;
#define SDL_MOUSEWHEEL_NORMAL  0
#define SDL_MOUSEWHEEL_FLIPPED 1
typedef struct SDL_MouseWheelEvent {
    Uint32 type, timestamp, windowID, which;
    Sint32 x, y;
    Uint32 direction;
    float preciseX, preciseY;
} SDL_MouseWheelEvent;
typedef struct SDL_JoyDeviceEvent { Uint32 type, timestamp; Sint32 which; } SDL_JoyDeviceEvent;
typedef struct SDL_JoyAxisEvent {
    Uint32 type, timestamp;
    SDL_JoystickID which;
    Uint8 axis, padding1, padding2, padding3;
    Sint16 value;
    Uint16 padding4;
} SDL_JoyAxisEvent;
typedef struct SDL_JoyHatEvent {
    Uint32 type, timestamp;
    SDL_JoystickID which;
    Uint8 hat, value, padding1, padding2;
} SDL_JoyHatEvent;
typedef struct SDL_JoyButtonEvent {
    Uint32 type, timestamp;
    SDL_JoystickID which;
    Uint8 button, state, padding1, padding2;
} SDL_JoyButtonEvent;
typedef struct SDL_ControllerDeviceEvent { Uint32 type, timestamp; Sint32 which; } SDL_ControllerDeviceEvent;
typedef struct SDL_ControllerAxisEvent {
    Uint32 type, timestamp;
    SDL_JoystickID which;
    Uint8 axis, padding1, padding2, padding3;
    Sint16 value;
    Uint16 padding4;
} SDL_ControllerAxisEvent;
typedef struct SDL_ControllerButtonEvent {
    Uint32 type, timestamp;
    SDL_JoystickID which;
    Uint8 button, state, padding1, padding2;
} SDL_ControllerButtonEvent;
typedef struct SDL_QuitEvent { Uint32 type, timestamp; } SDL_QuitEvent;
typedef struct SDL_UserEvent {
    Uint32 type, timestamp, windowID;
    Sint32 code;
    void *data1, *data2;
} SDL_UserEvent;
typedef struct SDL_DropEvent { Uint32 type, timestamp; char *file; Uint32 windowID; } SDL_DropEvent;

typedef long long SDL_TouchID;
typedef long long SDL_FingerID;
typedef struct SDL_TouchFingerEvent { Uint32 type, timestamp; SDL_TouchID touchId; SDL_FingerID fingerId;
    float x, y, dx, dy, pressure; } SDL_TouchFingerEvent;
typedef union SDL_Event {
    Uint32 type;
    SDL_CommonEvent common;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_JoyDeviceEvent jdevice;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_ControllerDeviceEvent cdevice;
    SDL_ControllerAxisEvent caxis;
    SDL_ControllerButtonEvent cbutton;
    SDL_QuitEvent quit;
    SDL_UserEvent user;
    SDL_DropEvent drop;
    SDL_TouchFingerEvent tfinger;
    Uint8 padding[56];
} SDL_Event;

typedef enum { SDL_ADDEVENT, SDL_PEEKEVENT, SDL_GETEVENT } SDL_eventaction;
void SDL_PumpEvents(void);
int  SDL_PollEvent(SDL_Event *e);
int  SDL_WaitEvent(SDL_Event *e);
int  SDL_WaitEventTimeout(SDL_Event *e, int ms);
int  SDL_PushEvent(SDL_Event *e);
int  SDL_PeepEvents(SDL_Event *es, int n, SDL_eventaction acao, Uint32 min, Uint32 max);
typedef int (*SDL_EventFilter)(void *userdata, SDL_Event *event);
void SDL_SetEventFilter(SDL_EventFilter filtro, void *userdata);   /* D374: se guarda pero no se aplica */
SDL_bool SDL_HasEvent(Uint32 tipo);
void SDL_FlushEvent(Uint32 tipo);
void SDL_FlushEvents(Uint32 min, Uint32 max);
Uint8 SDL_EventState(Uint32 tipo, int estado);
#define SDL_GetEventState(tipo) SDL_EventState(tipo, SDL_QUERY)
Uint32 SDL_RegisterEvents(int n);

/* ---- reloj ---- */
Uint32 SDL_GetTicks(void);
Uint64 SDL_GetTicks64(void);
void   SDL_Delay(Uint32 ms);
Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
#define SDL_TICKS_PASSED(A, B) ((Sint32)((B) - (A)) <= 0)
typedef int SDL_TimerID;
typedef Uint32 (*SDL_TimerCallback)(Uint32 intervalo, void *param);
SDL_TimerID SDL_AddTimer(Uint32 intervalo, SDL_TimerCallback f, void *param);
SDL_bool SDL_RemoveTimer(SDL_TimerID id);

/* ---- audio ---- */
typedef Uint16 SDL_AudioFormat;
#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_U16LSB 0x0010
#define AUDIO_S16LSB 0x8010
#define AUDIO_S32LSB 0x8020
#define AUDIO_F32LSB 0x8120
#define AUDIO_U16    AUDIO_U16LSB
#define AUDIO_S16    AUDIO_S16LSB
#define AUDIO_S16SYS AUDIO_S16LSB
#define AUDIO_S32SYS AUDIO_S32LSB
#define AUDIO_F32SYS AUDIO_F32LSB
#define SDL_AUDIO_BITSIZE(x) ((x) & 0xFF)
#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x00000001
#define SDL_AUDIO_ALLOW_FORMAT_CHANGE    0x00000002
#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE  0x00000004
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE   0x00000008
#define SDL_AUDIO_ALLOW_ANY_CHANGE       0x0000000F
#define SDL_MIX_MAXVOLUME 128
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);
typedef struct SDL_AudioSpec {
    int freq;
    SDL_AudioFormat format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    SDL_AudioCallback callback;
    void *userdata;
} SDL_AudioSpec;
typedef Uint32 SDL_AudioDeviceID;
typedef enum { SDL_AUDIO_STOPPED = 0, SDL_AUDIO_PLAYING, SDL_AUDIO_PAUSED } SDL_AudioStatus;
int  SDL_OpenAudio(SDL_AudioSpec *pedido, SDL_AudioSpec *obtido);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *nome, int captura, const SDL_AudioSpec *pedido,
                                      SDL_AudioSpec *obtido, int mudancas);
void SDL_PauseAudio(int pausa);
void SDL_PauseAudioDevice(SDL_AudioDeviceID d, int pausa);
SDL_AudioStatus SDL_GetAudioStatus(void);
SDL_AudioStatus SDL_GetAudioDeviceStatus(SDL_AudioDeviceID d);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_LockAudioDevice(SDL_AudioDeviceID d);
void SDL_UnlockAudioDevice(SDL_AudioDeviceID d);
void SDL_CloseAudio(void);
void SDL_CloseAudioDevice(SDL_AudioDeviceID d);
int  SDL_QueueAudio(SDL_AudioDeviceID d, const void *dados, Uint32 n);
Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID d);
void SDL_ClearQueuedAudio(SDL_AudioDeviceID d);
void SDL_MixAudio(Uint8 *d, const Uint8 *o, Uint32 n, int volume);
void SDL_MixAudioFormat(Uint8 *d, const Uint8 *o, SDL_AudioFormat formato, Uint32 n, int volume);
int  SDL_GetNumAudioDevices(int captura);
const char *SDL_GetAudioDeviceName(int indice, int captura);
const char *SDL_GetCurrentAudioDriver(void);

/* ---- mandos y joysticks (D307): los del sistema (USB HID, Xbox 360, puerto de juegos) ----
 * Todos se pueden abrir como SDL_GameController: el Xbox 360 y los Logitech con su mapeo
 * conocido; los demás, con uno genérico (botones 1-4 = A B X Y, 5-6 hombros, 7-8 gatillos,
 * 9 Back, 10 Start; ejes 1-2 stick izquierdo, 3-4 derecho; hat 1 = cruceta). */
typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_GameController SDL_GameController;
typedef enum {
    SDL_CONTROLLER_AXIS_INVALID = -1, SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_CONTROLLER_AXIS_MAX
} SDL_GameControllerAxis;
typedef enum {
    SDL_CONTROLLER_BUTTON_INVALID = -1, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_GUIDE, SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_MAX
} SDL_GameControllerButton;
#define SDL_HAT_CENTERED  0x00
#define SDL_HAT_UP        0x01
#define SDL_HAT_RIGHT     0x02
#define SDL_HAT_DOWN      0x04
#define SDL_HAT_LEFT      0x08
#define SDL_HAT_RIGHTUP   (SDL_HAT_RIGHT | SDL_HAT_UP)
#define SDL_HAT_RIGHTDOWN (SDL_HAT_RIGHT | SDL_HAT_DOWN)
#define SDL_HAT_LEFTUP    (SDL_HAT_LEFT | SDL_HAT_UP)
#define SDL_HAT_LEFTDOWN  (SDL_HAT_LEFT | SDL_HAT_DOWN)
const char *SDL_GameControllerGetStringForButton(SDL_GameControllerButton botao);
const char *SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis eixo);
const char *SDL_GameControllerNameForIndex(int indice);
int SDL_GameControllerRumble(SDL_GameController *c, Uint16 baixa, Uint16 alta, Uint32 ms);
int SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int indice);
void SDL_JoystickClose(SDL_Joystick *j);
SDL_bool SDL_IsGameController(int indice);
SDL_GameController *SDL_GameControllerOpen(int indice);
void SDL_GameControllerClose(SDL_GameController *c);
SDL_GameController *SDL_GameControllerFromInstanceID(SDL_JoystickID id);
SDL_bool SDL_GameControllerGetAttached(SDL_GameController *c);
Sint16 SDL_GameControllerGetAxis(SDL_GameController *c, SDL_GameControllerAxis eixo);
Uint8 SDL_GameControllerGetButton(SDL_GameController *c, SDL_GameControllerButton botao);
const char *SDL_GameControllerName(SDL_GameController *c);
int SDL_GameControllerEventState(int estado);
int SDL_JoystickEventState(int estado);
typedef struct { Uint8 data[16]; } SDL_JoystickGUID;
const char *SDL_JoystickName(SDL_Joystick *j);
const char *SDL_JoystickNameForIndex(int indice);
int SDL_JoystickNumAxes(SDL_Joystick *j);
int SDL_JoystickNumButtons(SDL_Joystick *j);
int SDL_JoystickNumHats(SDL_Joystick *j);
int SDL_JoystickNumBalls(SDL_Joystick *j);
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int eixo);
Uint8 SDL_JoystickGetButton(SDL_Joystick *j, int botao);
Uint8 SDL_JoystickGetHat(SDL_Joystick *j, int chapeu);
void SDL_JoystickUpdate(void);
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *j);
SDL_JoystickID SDL_JoystickGetDeviceInstanceID(int indice);
SDL_bool SDL_JoystickGetAttached(SDL_Joystick *j);
SDL_Joystick *SDL_JoystickFromInstanceID(SDL_JoystickID id);
Uint16 SDL_JoystickGetVendor(SDL_Joystick *j);
Uint16 SDL_JoystickGetProduct(SDL_Joystick *j);
SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *j);
SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int indice);
void SDL_JoystickGetGUIDString(SDL_JoystickGUID guid, char *texto, int max);
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *c);
void SDL_GameControllerUpdate(void);
char *SDL_GameControllerMapping(SDL_GameController *c);
int SDL_GameControllerAddMapping(const char *mapeo);
int SDL_GameControllerAddMappingsFromFile(const char *arquivo);

/* ---- caja de mensaje, portapapeles, rutas ---- */
#define SDL_MESSAGEBOX_ERROR       0x00000010
#define SDL_MESSAGEBOX_WARNING     0x00000020
#define SDL_MESSAGEBOX_INFORMATION 0x00000040
int SDL_ShowSimpleMessageBox(Uint32 flags, const char *titulo, const char *mensagem, SDL_Window *w);
#define SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT 0x00000001
#define SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT 0x00000002
typedef struct SDL_MessageBoxButtonData { Uint32 flags; int buttonid; const char *text; } SDL_MessageBoxButtonData;
typedef struct SDL_MessageBoxColorScheme { int colors[5][3]; } SDL_MessageBoxColorScheme;
typedef struct SDL_MessageBoxData {
    Uint32 flags;
    SDL_Window *window;
    const char *title, *message;
    int numbuttons;
    const SDL_MessageBoxButtonData *buttons;
    const SDL_MessageBoxColorScheme *colorScheme;
} SDL_MessageBoxData;
int SDL_ShowMessageBox(const SDL_MessageBoxData *dados, int *botao);   /* hasta 4 botones; si se cierra, el de ESCAPEKEY_DEFAULT (o -1) */
char *SDL_GetClipboardText(void);
int SDL_SetClipboardText(const char *t);
SDL_bool SDL_HasClipboardText(void);
char *SDL_GetBasePath(void);
char *SDL_GetPrefPath(const char *org, const char *app);
int SDL_GetCPUCount(void);
int SDL_GetSystemRAM(void);


/* ---- hilos, cerrojos, condiciones, semáforos y atómicas (D383, D393) ----
 * Hilos de verdad sobre los del SaguiOS (D167): con --smp corren a la vez en las otras CPU.
 * El audio tiene su HILO PROPIO desde SDL_PauseAudio(0) (D393, 2026-11-22): llama a la llamada de vuelta
 * del programa con SDL_LockAudio tomado, como la SDL de verdad, y mueve la cola de SDL_QueueAudio.
 * SDL_SAGUI_AUDIO_HILO=0 (variable de entorno o pista) vuelve al modo de antes, en el que el hilo principal
 * lo mueve (SDL_PollEvent, SDL_Delay, SDL_RenderPresent). Los demás hilos no deben llamar a vídeo ni
 * eventos. Los cerrojos son recursivos, como en la SDL de verdad: el hilo que ya lo tiene lo vuelve a
 * tomar (así siguen igual los programas que se llamaban a sí mismos con el cerrojo tomado).
 * SDL_CondWait en el hilo principal se despierta además cada pocos milisegundos (para mover la OpenAL y,
 * en el modo de antes, el audio; las esperas pueden despertarse sin aviso: quien espera vuelve a mirar
 * su condición, como manda la SDL).
 * SDL_GetCPUCount cuenta las CPU arrancadas (1 sin --smp). */
#define SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING "SDL_WINDOWS_DISABLE_THREAD_NAMING"
#define SDL_HINT_NO_SIGNAL_HANDLERS "SDL_NO_SIGNAL_HANDLERS"
#ifndef SDL_ALPHA_OPAQUE
#define SDL_ALPHA_OPAQUE 255
#define SDL_ALPHA_TRANSPARENT 0
#endif
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_cond SDL_cond;
typedef struct SDL_Thread SDL_Thread;
typedef int (*SDL_ThreadFunction)(void *datos);
SDL_mutex *SDL_CreateMutex(void);
int SDL_LockMutex(SDL_mutex *m);
int SDL_UnlockMutex(SDL_mutex *m);
void SDL_DestroyMutex(SDL_mutex *m);
SDL_cond *SDL_CreateCond(void);
int SDL_CondSignal(SDL_cond *c);
int SDL_CondWait(SDL_cond *c, SDL_mutex *m);
void SDL_DestroyCond(SDL_cond *c);
SDL_Thread *SDL_CreateThread(SDL_ThreadFunction fn, const char *nombre, void *datos);
void SDL_WaitThread(SDL_Thread *t, int *estado);
/* D393: lo que faltaba para usar hilos de verdad */
#define SDL_MUTEX_TIMEDOUT 1
#define SDL_MUTEX_MAXWAIT (~(Uint32)0)
typedef unsigned long SDL_threadID;
typedef enum { SDL_THREAD_PRIORITY_LOW, SDL_THREAD_PRIORITY_NORMAL, SDL_THREAD_PRIORITY_HIGH, SDL_THREAD_PRIORITY_TIME_CRITICAL } SDL_ThreadPriority;
int SDL_TryLockMutex(SDL_mutex *m);
int SDL_CondBroadcast(SDL_cond *c);
int SDL_CondWaitTimeout(SDL_cond *c, SDL_mutex *m, Uint32 ms);
SDL_Thread *SDL_CreateThreadWithStackSize(SDL_ThreadFunction fn, const char *nombre, size_t pila, void *datos);
void SDL_DetachThread(SDL_Thread *t);
SDL_threadID SDL_ThreadID(void);
SDL_threadID SDL_GetThreadID(SDL_Thread *t);
const char *SDL_GetThreadName(SDL_Thread *t);
int SDL_SetThreadPriority(SDL_ThreadPriority prioridad);
typedef struct SDL_semaphore SDL_sem;
SDL_sem *SDL_CreateSemaphore(Uint32 valor);
void SDL_DestroySemaphore(SDL_sem *s);
int SDL_SemWait(SDL_sem *s);
int SDL_SemTryWait(SDL_sem *s);
int SDL_SemWaitTimeout(SDL_sem *s, Uint32 ms);
int SDL_SemPost(SDL_sem *s);
Uint32 SDL_SemValue(SDL_sem *s);
typedef struct { int value; } SDL_atomic_t;
SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int viejo, int nuevo);
int SDL_AtomicSet(SDL_atomic_t *a, int v);
int SDL_AtomicGet(SDL_atomic_t *a);
int SDL_AtomicAdd(SDL_atomic_t *a, int v);
#define SDL_AtomicIncRef(a) SDL_AtomicAdd(a, 1)
#define SDL_AtomicDecRef(a) (SDL_AtomicAdd(a, -1) == 1)
void SDL_MemoryBarrierReleaseFunction(void);
void SDL_MemoryBarrierAcquireFunction(void);
#define SDL_MemoryBarrierRelease() SDL_MemoryBarrierReleaseFunction()
#define SDL_MemoryBarrierAcquire() SDL_MemoryBarrierAcquireFunction()
#define SDL_CompilerBarrier() __asm__ __volatile__("" ::: "memory")

/* ---- conversión de audio (D383): formato, canales y frecuencia (SDL_BuildAudioCVT / SDL_ConvertAudio) ---- */
#define SDL_AUDIOCVT_MAX_FILTERS 9
typedef struct SDL_AudioCVT {
    int needed;
    SDL_AudioFormat src_format, dst_format;
    double rate_incr;
    Uint8 *buf;
    int len, len_cvt, len_mult;
    double len_ratio;
    int src_channels, dst_channels, src_rate, dst_rate;   /* privados de nuestra implementación */
} SDL_AudioCVT;
int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, SDL_AudioFormat src_fmt, Uint8 src_canales, int src_frec,
                      SDL_AudioFormat dst_fmt, Uint8 dst_canales, int dst_frec);
int SDL_ConvertAudio(SDL_AudioCVT *cvt);
int SDL_GetWindowDisplayIndex(SDL_Window *w);
#define SDL_CONTROLLER_TYPE_UNKNOWN 0
int SDL_GameControllerTypeForIndex(int indice);
SDL_JoystickGUID SDL_JoystickGetGUIDFromString(const char *texto);

/* ---- el main ---- */
/* En el SaguiOS el programa empieza en principal(); el port pone un
 * principal que llama a su main. SDL_main no cambia el nombre. */
#define SDL_main main

#include "SDL_rwops.h"

/* ---- D425: contexto OpenGL ES 1 (SaguiGL por EGL, userland/libsdl2/sdl2_gl.c) ----
 * Solo OpenGL ES 1.x (SDL_GL_CONTEXT_PROFILE_ES con versión 1; sin perfil también es ES 1).
 * Hay que enlazar además la SaguiGL: SDL2GL_OBJ y SGL_OBJ en el Makefile, o con tcc
 * «-lSDL2 -lGLESv1_CM -lEGL». */
typedef void *SDL_GLContext;
typedef enum {
    SDL_GL_RED_SIZE, SDL_GL_GREEN_SIZE, SDL_GL_BLUE_SIZE, SDL_GL_ALPHA_SIZE, SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER, SDL_GL_DEPTH_SIZE, SDL_GL_STENCIL_SIZE, SDL_GL_ACCUM_RED_SIZE, SDL_GL_ACCUM_GREEN_SIZE,
    SDL_GL_ACCUM_BLUE_SIZE, SDL_GL_ACCUM_ALPHA_SIZE, SDL_GL_STEREO, SDL_GL_MULTISAMPLEBUFFERS, SDL_GL_MULTISAMPLESAMPLES,
    SDL_GL_ACCELERATED_VISUAL, SDL_GL_RETAINED_BACKING, SDL_GL_CONTEXT_MAJOR_VERSION, SDL_GL_CONTEXT_MINOR_VERSION,
    SDL_GL_CONTEXT_EGL, SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_SHARE_WITH_CURRENT_CONTEXT,
    SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, SDL_GL_CONTEXT_RELEASE_BEHAVIOR, SDL_GL_CONTEXT_RESET_NOTIFICATION,
    SDL_GL_CONTEXT_NO_ERROR
} SDL_GLattr;
#define SDL_GL_CONTEXT_PROFILE_CORE          0x0001
#define SDL_GL_CONTEXT_PROFILE_COMPATIBILITY 0x0002
#define SDL_GL_CONTEXT_PROFILE_ES            0x0004
int           SDL_GL_LoadLibrary(const char *ruta);
void          SDL_GL_UnloadLibrary(void);
void         *SDL_GL_GetProcAddress(const char *nome);
SDL_bool      SDL_GL_ExtensionSupported(const char *ext);
void          SDL_GL_ResetAttributes(void);
int           SDL_GL_SetAttribute(SDL_GLattr a, int v);
int           SDL_GL_GetAttribute(SDL_GLattr a, int *v);
SDL_GLContext SDL_GL_CreateContext(SDL_Window *w);
int           SDL_GL_MakeCurrent(SDL_Window *w, SDL_GLContext c);
SDL_Window   *SDL_GL_GetCurrentWindow(void);
SDL_GLContext SDL_GL_GetCurrentContext(void);
void          SDL_GL_GetDrawableSize(SDL_Window *w, int *l, int *a);
int           SDL_GL_SetSwapInterval(int i);
int           SDL_GL_GetSwapInterval(void);
void          SDL_GL_SwapWindow(SDL_Window *w);
void          SDL_GL_DeleteContext(SDL_GLContext c);
/* interna: la ventana del sistema detrás de una SDL_Window (para EGL) */
int sagui_sdl2_janela_nativa(SDL_Window *w, int *id, uint32_t **px, int *l, int *a);

#ifdef __cplusplus
}
#endif

#endif
