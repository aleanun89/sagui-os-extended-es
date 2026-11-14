/* SDL.h - la parte de la SDL 1.2 que usa el motor de OpenTyrian.
 *
 * Esta cabecera es nuestra (D156): el SaguiOS no tiene SDL. Los nombres, los
 * tipos y los valores de las constantes son los de SDL 1.2 para que el codigo
 * del juego (que es de GPL-2.0 y no se toca) compile sin cambios. Lo que
 * falta de la SDL de verdad se implementa en sdl_sagui.c.
 *
 * De la SDL original solo se conserva la interfaz; no hay ni una linea de su
 * codigo. Ver ORIGEM.md.
 */
#ifndef SAGUI_SDL_H
#define SAGUI_SDL_H

#include "SDL_types.h"
#include "SDL_endian.h"

/* A SDL de verdade (SDL_stdinc.h) traz estas cabeceras, e o motor depende
 * delas: usa memcpy, strcmp e strlen sem as incluir. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* ---- subsistemas ---- */
#define SDL_INIT_TIMER          0x00000001
#define SDL_INIT_AUDIO          0x00000010
#define SDL_INIT_VIDEO          0x00000020
#define SDL_INIT_JOYSTICK       0x00000200
#define SDL_INIT_NOPARACHUTE    0x00100000
#define SDL_INIT_EVENTTHREAD    0x01000000
#define SDL_INIT_EVERYTHING     0x0000FFFF

/* ---- superficies ---- */
#define SDL_SWSURFACE   0x00000000
#define SDL_HWSURFACE   0x00000001
#define SDL_ASYNCBLIT   0x00000004
#define SDL_ANYFORMAT   0x10000000
#define SDL_HWPALETTE   0x20000000
#define SDL_DOUBLEBUF   0x40000000
#define SDL_FULLSCREEN  0x80000000
#define SDL_OPENGL      0x00000002
#define SDL_RESIZABLE   0x00000010
#define SDL_NOFRAME     0x00000020

/* ---- eventos ---- */
#define SDL_NOEVENT          0
#define SDL_ACTIVEEVENT      1
#define SDL_KEYDOWN          2
#define SDL_KEYUP            3
#define SDL_MOUSEMOTION      4
#define SDL_MOUSEBUTTONDOWN  5
#define SDL_MOUSEBUTTONUP    6
#define SDL_QUIT             12
#define SDL_VIDEORESIZE      16
#define SDL_VIDEOEXPOSE      17
#define SDL_USEREVENT        24

#define SDL_APPINPUTFOCUS    0x0001
#define SDL_APPINPUTFOCUS_LOST 0x0002
#define SDL_APPMOUSEFOCUS    0x0004
#define SDL_APPWINDOWFOCUS   0x0008
#define SDL_APPMOUSECAPTURE  0x0010
#define SDL_APPACTIVE        0x0020

/* ---- botones y gorros ---- */
#define SDL_BUTTON_LEFT      1
#define SDL_BUTTON_MIDDLE    2
#define SDL_BUTTON_RIGHT     3
#define SDL_BUTTON_WHEELUP   4
#define SDL_BUTTON_WHEELDOWN 5

#define SDL_HAT_CENTERED 0x00
#define SDL_HAT_UP       0x01
#define SDL_HAT_RIGHT    0x02
#define SDL_HAT_DOWN     0x04
#define SDL_HAT_LEFT     0x08
#define SDL_HAT_RIGHTUP  (SDL_HAT_RIGHT | SDL_HAT_UP)
#define SDL_HAT_RIGHTDOWN (SDL_HAT_RIGHT | SDL_HAT_DOWN)
#define SDL_HAT_LEFTUP   (SDL_HAT_LEFT | SDL_HAT_UP)
#define SDL_HAT_LEFTDOWN (SDL_HAT_LEFT | SDL_HAT_DOWN)

/* ---- raton ---- */
#define SDL_ENABLE  1
#define SDL_DISABLE 0
#define SDL_IGNORE  0

#define SDL_PRESSED  1
#define SDL_RELEASED 0

#define SDL_GRAB_ON   1
#define SDL_GRAB_OFF  0

/* ---- audio ---- */
#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_S16LSB 0x8010
#define AUDIO_S16MSB 0x9010
#define AUDIO_S16SYS AUDIO_S16LSB

/* ---- endian ---- */
#define SDL_BYTEORDER  SDL_LIL_ENDIAN

/* ---- utiles ---- */
#define SDL_TRUE  1
#define SDL_FALSE 0

/* ---- teclas ---- */
typedef enum {
    SDLK_UNKNOWN        = 0,
    SDLK_BACKSPACE      = 8,
    SDLK_TAB            = 9,
    SDLK_CLEAR          = 12,
    SDLK_RETURN         = 13,
    SDLK_PAUSE          = 19,
    SDLK_ESCAPE         = 27,
    SDLK_SPACE          = 32,
    SDLK_EXCLAIM        = 33,
    SDLK_QUOTEDBL       = 34,
    SDLK_HASH           = 35,
    SDLK_DOLLAR         = 36,
    SDLK_AMPERSAND      = 38,
    SDLK_QUOTE          = 39,
    SDLK_LEFTPAREN      = 40,
    SDLK_RIGHTPAREN     = 41,
    SDLK_ASTERISK       = 42,
    SDLK_PLUS           = 43,
    SDLK_COMMA          = 44,
    SDLK_MINUS          = 45,
    SDLK_PERIOD         = 46,
    SDLK_SLASH          = 47,
    SDLK_0              = 48,
    SDLK_1              = 49,
    SDLK_2              = 50,
    SDLK_3              = 51,
    SDLK_4              = 52,
    SDLK_5              = 53,
    SDLK_6              = 54,
    SDLK_7              = 55,
    SDLK_8              = 56,
    SDLK_9              = 57,
    SDLK_COLON          = 58,
    SDLK_SEMICOLON      = 59,
    SDLK_LESS           = 60,
    SDLK_EQUALS         = 61,
    SDLK_GREATER        = 62,
    SDLK_QUESTION       = 63,
    SDLK_AT             = 64,
    SDLK_LEFTBRACKET    = 91,
    SDLK_BACKSLASH      = 92,
    SDLK_RIGHTBRACKET   = 93,
    SDLK_CARET          = 94,
    SDLK_UNDERSCORE     = 95,
    SDLK_BACKQUOTE      = 96,
    SDLK_a              = 97,
    SDLK_b              = 98,
    SDLK_c              = 99,
    SDLK_d              = 100,
    SDLK_e              = 101,
    SDLK_f              = 102,
    SDLK_g              = 103,
    SDLK_h              = 104,
    SDLK_i              = 105,
    SDLK_j              = 106,
    SDLK_k              = 107,
    SDLK_l              = 108,
    SDLK_m              = 109,
    SDLK_n              = 110,
    SDLK_o              = 111,
    SDLK_p              = 112,
    SDLK_q              = 113,
    SDLK_r              = 114,
    SDLK_s              = 115,
    SDLK_t              = 116,
    SDLK_u              = 117,
    SDLK_v              = 118,
    SDLK_w              = 119,
    SDLK_x              = 120,
    SDLK_y              = 121,
    SDLK_z              = 122,
    SDLK_DELETE         = 127,

    SDLK_KP0            = 256,
    SDLK_KP1            = 257,
    SDLK_KP2            = 258,
    SDLK_KP3            = 259,
    SDLK_KP4            = 260,
    SDLK_KP5            = 261,
    SDLK_KP6            = 262,
    SDLK_KP7            = 263,
    SDLK_KP8            = 264,
    SDLK_KP9            = 265,
    SDLK_KP_PERIOD      = 266,
    SDLK_KP_DIVIDE      = 267,
    SDLK_KP_MULTIPLY    = 268,
    SDLK_KP_MINUS       = 269,
    SDLK_KP_PLUS        = 270,
    SDLK_KP_ENTER       = 271,
    SDLK_KP_EQUALS      = 272,
    SDLK_UP             = 273,
    SDLK_DOWN           = 274,
    SDLK_RIGHT          = 275,
    SDLK_LEFT           = 276,
    SDLK_INSERT         = 277,
    SDLK_HOME           = 278,
    SDLK_END            = 279,
    SDLK_PAGEUP         = 280,
    SDLK_PAGEDOWN       = 281,
    SDLK_F1             = 282,
    SDLK_F2             = 283,
    SDLK_F3             = 284,
    SDLK_F4             = 285,
    SDLK_F5             = 286,
    SDLK_F6             = 287,
    SDLK_F7             = 288,
    SDLK_F8             = 289,
    SDLK_F9             = 290,
    SDLK_F10            = 291,
    SDLK_F11            = 292,
    SDLK_F12            = 293,
    SDLK_NUMLOCK        = 300,
    SDLK_CAPSLOCK       = 301,
    SDLK_SCROLLOCK      = 302,
    SDLK_RSHIFT         = 303,
    SDLK_LSHIFT         = 304,
    SDLK_RCTRL          = 305,
    SDLK_LCTRL          = 306,
    SDLK_RALT           = 307,
    SDLK_LALT           = 308,
    SDLK_MODE           = 309,
    SDLK_HELP           = 315,
    SDLK_PRINT          = 316,
    SDLK_SYSREQ         = 317,
    SDLK_BREAK          = 318,
    SDLK_MENU           = 319,
    SDLK_POWER          = 320,
    SDLK_EURO           = 321,
    SDLK_UNDO           = 322,
    SDLK_LAST           = 323
} SDLKey;

typedef enum {
    KMOD_NONE     = 0x0000,
    KMOD_LSHIFT   = 0x0001,
    KMOD_RSHIFT   = 0x0002,
    KMOD_LCTRL    = 0x0040,
    KMOD_RCTRL    = 0x0080,
    KMOD_LALT     = 0x0100,
    KMOD_RALT     = 0x0200,
    KMOD_NUM      = 0x1000,
    KMOD_CAPS     = 0x2000,
    KMOD_MODE     = 0x4000,
    KMOD_RESERVED = 0x8000
} SDLMod;

#define KMOD_CTRL  (KMOD_LCTRL | KMOD_RCTRL)
#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
#define KMOD_ALT   (KMOD_LALT | KMOD_RALT)

typedef struct {
    Uint16  scancode;
    SDLKey  sym;
    SDLMod  mod;
    Uint16  unicode;
} SDL_keysym;

/* ---- superficies ---- */
typedef struct {
    int x, y;
    int w, h;
} SDL_Rect;

typedef struct {
    Uint8 r, g, b, unused;
} SDL_Color;

typedef struct {
    int        ncolors;
    SDL_Color *colors;
} SDL_Palette;

typedef struct {
    SDL_Palette *palette;
    Uint8  BitsPerPixel;
    Uint8  BytesPerPixel;
    Uint8  Rloss, Gloss, Bloss, Aloss;
    Uint8  Rshift, Gshift, Bshift, Ashift;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 colormap;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32          flags;
    SDL_PixelFormat *format;
    int             w, h;
    Uint16          pitch;
    void           *pixels;
    int             offset;
    struct SDL_Surface *hwdata;
} SDL_Surface;

/* ---- eventos ---- */
typedef struct {
    Uint8 type;
    Uint8 state;
    Uint8 gain;
} SDL_ActiveEvent;

typedef struct {
    Uint8      type;
    Uint8      which;
    Uint8      state;
    SDL_keysym keysym;
} SDL_KeyboardEvent;

typedef struct {
    Uint8  type;
    Uint8  which;
    Uint8  state;
    Uint16 x, y;
    Sint16 xrel, yrel;
} SDL_MouseMotionEvent;

typedef struct {
    Uint8  type;
    Uint8  which;
    Uint8  button;
    Uint8  state;
    Uint16 x, y;
} SDL_MouseButtonEvent;

typedef struct {
    Uint8 type;
    int   w, h;
} SDL_ResizeEvent;

typedef struct {
    Uint8 type;
} SDL_QuitEvent;

typedef union {
    Uint8               type;
    SDL_ActiveEvent     active;
    SDL_KeyboardEvent   key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_ResizeEvent     resize;
    SDL_QuitEvent       quit;
} SDL_Event;

/* ---- audio ---- */
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);

typedef struct {
    int              freq;
    Uint16           format;
    Uint8            channels;
    Uint8            silence;
    Uint16           samples;
    Uint32           size;
    SDL_AudioCallback callback;
    void            *userdata;
} SDL_AudioSpec;

typedef struct {
    int   needed;
    Uint16 src_format;
    Uint16 dst_format;
    int   len_mult;
    int   len;
    int   len_cvt;
    void *buf;
} SDL_AudioCVT;

/* ---- joystick ---- */
typedef struct _SDL_Joystick SDL_Joystick;

/* ---- funciones ---- */
int  SDL_Init(Uint32 flags);
int  SDL_InitSubSystem(Uint32 flags);
void SDL_QuitSubSystem(Uint32 flags);
void SDL_Quit(void);
Uint32 SDL_WasInit(Uint32 flags);
const char *SDL_GetError(void);

int  SDL_VideoModeOK(int largura, int altura, int bits, Uint32 flags);
SDL_Surface *SDL_SetVideoMode(int largura, int altura, int bits, Uint32 flags);
SDL_Surface *SDL_GetVideoSurface(void);
int  SDL_Flip(SDL_Surface *superficie);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int largura, int altura, int bits,
                                  Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask);
void SDL_FreeSurface(SDL_Surface *superficie);
int  SDL_FillRect(SDL_Surface *destino, void *rect, Uint32 cor);
Uint32 SDL_MapRGB(const SDL_PixelFormat *formato, Uint8 r, Uint8 g, Uint8 b);
int  SDL_SetColors(SDL_Surface *superficie, SDL_Color *cores, int primeiro, int n);
void SDL_WM_SetCaption(const char *titulo, const char *icone);
int  SDL_WM_GrabInput(int modo);
void SDL_WarpMouse(int x, int y);

int  SDL_PollEvent(SDL_Event *evento);
int  SDL_PushEvent(SDL_Event *evento);
int  SDL_EnableKeyRepeat(int delay, int intervalo);
int  SDL_EnableUNICODE(int enable);
int  SDL_ShowCursor(int estado);
SDLMod SDL_GetModState(void);
const char *SDL_GetKeyName(SDLKey chave);

Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

int  SDL_OpenAudio(SDL_AudioSpec *pedido, SDL_AudioSpec *obtido);
void SDL_CloseAudio(void);
void SDL_PauseAudio(int pausa);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
int  SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 formato_origem, Uint8 canais_origem,
                       int freq_origem, Uint16 formato_destino, Uint8 canais_destino,
                       int freq_destino);
int  SDL_ConvertAudio(SDL_AudioCVT *cvt);

int  SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int indice);
void SDL_JoystickClose(SDL_Joystick *joy);
const char *SDL_JoystickName(SDL_Joystick *joy);
int  SDL_JoystickNumAxes(SDL_Joystick *joy);
int  SDL_JoystickNumButtons(SDL_Joystick *joy);
int  SDL_JoystickNumHats(SDL_Joystick *joy);
Sint16 SDL_JoystickGetAxis(SDL_Joystick *joy, int eixo);
Uint8 SDL_JoystickGetButton(SDL_Joystick *joy, int botao);
Uint8 SDL_JoystickGetHat(SDL_Joystick *joy, int chapeu);
void SDL_JoystickUpdate(void);
int  SDL_JoystickEventState(int estado);

void *SDL_malloc(size_t n);
void  SDL_free(void *p);

#endif
