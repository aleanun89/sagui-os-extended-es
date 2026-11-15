/* SDL.h - la SDL 1.2 del SaguiOS para los juegos portados (D228).
 *
 * Esta cabecera es nuestra: el SaguiOS no tiene SDL. Los nombres, los tipos y
 * los valores de las constantes son los de SDL 1.2 para que el código de los
 * juegos compile sin cambios; lo que hay detrás está en sdl_sagui.c, sobre
 * las ventanas, el teclado y el sonido del sistema. Nació como la capa del
 * OpenTyrian (D156, userland/terceros/opentyrian/plataforma/) y aquí está
 * generalizada para cualquier port: superficies de 8 bits con paleta (la
 * pantalla incluida), blits con color clave y recorte, la mezcla de sonido y
 * eventos de C y de C++. De la SDL original solo se conserva la interfaz.
 */
#ifndef SAGUI_SDL12_H
#define SAGUI_SDL12_H

#include <stddef.h>
#include <stdint.h>
/* La SDL de verdad (SDL_stdinc.h) trae estas cabeceras y los juegos lo dan
 * por hecho. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
#define SDL_MAJOR_VERSION 1
#define SDL_MINOR_VERSION 2
#define SDL_PATCHLEVEL    15
#define SDLCALL
#define SDL_VERSION_ATLEAST(X, Y, Z) \
    ((SDL_MAJOR_VERSION >= (X)) && (SDL_MAJOR_VERSION > (X) || SDL_MINOR_VERSION >= (Y)) && \
     (SDL_MAJOR_VERSION > (X) || SDL_MINOR_VERSION > (Y) || SDL_PATCHLEVEL >= (Z)))
#define SDL_VERSION(x) do { (x)->major = SDL_MAJOR_VERSION; (x)->minor = SDL_MINOR_VERSION; \
                            (x)->patch = SDL_PATCHLEVEL; } while (0)
const SDL_version *SDL_Linked_Version(void);

/* ---- subsistemas ---- */
#define SDL_INIT_TIMER          0x00000001
#define SDL_INIT_AUDIO          0x00000010
#define SDL_INIT_VIDEO          0x00000020
#define SDL_INIT_CDROM          0x00000100
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
#define SDL_SRCCOLORKEY 0x00001000
#define SDL_RLEACCEL    0x00004000
#define SDL_SRCALPHA    0x00010000
#define SDL_PREALLOC    0x01000000
#define SDL_LOGPAL      0x01
#define SDL_PHYSPAL     0x02
/* Las superficies del SaguiOS están siempre en memoria: no hay que bloquearlas. */
#define SDL_MUSTLOCK(s) 0

/* ---- eventos ---- */
#define SDL_NOEVENT          0
#define SDL_ACTIVEEVENT      1
#define SDL_KEYDOWN          2
#define SDL_KEYUP            3
#define SDL_MOUSEMOTION      4
#define SDL_MOUSEBUTTONDOWN  5
#define SDL_MOUSEBUTTONUP    6
#define SDL_JOYAXISMOTION    7
#define SDL_JOYBALLMOTION    8
#define SDL_JOYHATMOTION     9
#define SDL_JOYBUTTONDOWN    10
#define SDL_JOYBUTTONUP      11
#define SDL_QUIT             12
#define SDL_SYSWMEVENT       13
#define SDL_VIDEORESIZE      16
#define SDL_VIDEOEXPOSE      17
#define SDL_USEREVENT        24

#define SDL_APPMOUSEFOCUS    0x01
#define SDL_APPINPUTFOCUS    0x02
#define SDL_APPACTIVE        0x04

/* ---- ratón y joystick ---- */
#define SDL_BUTTON(X)        (1 << ((X) - 1))
#define SDL_BUTTON_LEFT      1
#define SDL_BUTTON_MIDDLE    2
#define SDL_BUTTON_RIGHT     3
#define SDL_BUTTON_WHEELUP   4
#define SDL_BUTTON_WHEELDOWN 5
#define SDL_HAT_CENTERED  0x00
#define SDL_HAT_UP        0x01
#define SDL_HAT_RIGHT     0x02
#define SDL_HAT_DOWN      0x04
#define SDL_HAT_LEFT      0x08
#define SDL_HAT_RIGHTUP   (SDL_HAT_RIGHT | SDL_HAT_UP)
#define SDL_HAT_RIGHTDOWN (SDL_HAT_RIGHT | SDL_HAT_DOWN)
#define SDL_HAT_LEFTUP    (SDL_HAT_LEFT | SDL_HAT_UP)
#define SDL_HAT_LEFTDOWN  (SDL_HAT_LEFT | SDL_HAT_DOWN)

#define SDL_QUERY   -1
#define SDL_IGNORE   0
#define SDL_DISABLE  0
#define SDL_ENABLE   1
#define SDL_PRESSED  1
#define SDL_RELEASED 0
#define SDL_GRAB_QUERY -1
#define SDL_GRAB_OFF   0
#define SDL_GRAB_ON    1

/* ---- audio ---- */
#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_U16LSB 0x0010
#define AUDIO_S16LSB 0x8010
#define AUDIO_U16MSB 0x1010
#define AUDIO_S16MSB 0x9010
#define AUDIO_U16    AUDIO_U16LSB
#define AUDIO_S16    AUDIO_S16LSB
#define AUDIO_U16SYS AUDIO_U16LSB
#define AUDIO_S16SYS AUDIO_S16LSB
#define SDL_MIX_MAXVOLUME 128

/* ---- teclas ---- */
typedef enum {
    SDLK_UNKNOWN = 0, SDLK_FIRST = 0,
    SDLK_BACKSPACE = 8, SDLK_TAB = 9, SDLK_CLEAR = 12, SDLK_RETURN = 13, SDLK_PAUSE = 19,
    SDLK_ESCAPE = 27, SDLK_SPACE = 32, SDLK_EXCLAIM = 33, SDLK_QUOTEDBL = 34, SDLK_HASH = 35,
    SDLK_DOLLAR = 36, SDLK_AMPERSAND = 38, SDLK_QUOTE = 39, SDLK_LEFTPAREN = 40,
    SDLK_RIGHTPAREN = 41, SDLK_ASTERISK = 42, SDLK_PLUS = 43, SDLK_COMMA = 44, SDLK_MINUS = 45,
    SDLK_PERIOD = 46, SDLK_SLASH = 47,
    SDLK_0 = 48, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
    SDLK_COLON = 58, SDLK_SEMICOLON = 59, SDLK_LESS = 60, SDLK_EQUALS = 61, SDLK_GREATER = 62,
    SDLK_QUESTION = 63, SDLK_AT = 64,
    SDLK_LEFTBRACKET = 91, SDLK_BACKSLASH = 92, SDLK_RIGHTBRACKET = 93, SDLK_CARET = 94,
    SDLK_UNDERSCORE = 95, SDLK_BACKQUOTE = 96,
    SDLK_a = 97, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
    SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
    SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
    SDLK_DELETE = 127,
    /* Las letras de Latin-1 que no son ASCII (º, ñ, ç...), con su código (D229). */
    SDLK_WORLD_0 = 160, SDLK_WORLD_95 = 255,
    SDLK_KP0 = 256, SDLK_KP1, SDLK_KP2, SDLK_KP3, SDLK_KP4, SDLK_KP5, SDLK_KP6, SDLK_KP7,
    SDLK_KP8, SDLK_KP9,
    SDLK_KP_PERIOD = 266, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY, SDLK_KP_MINUS, SDLK_KP_PLUS,
    SDLK_KP_ENTER, SDLK_KP_EQUALS,
    SDLK_UP = 273, SDLK_DOWN, SDLK_RIGHT, SDLK_LEFT, SDLK_INSERT, SDLK_HOME, SDLK_END,
    SDLK_PAGEUP, SDLK_PAGEDOWN,
    SDLK_F1 = 282, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9,
    SDLK_F10, SDLK_F11, SDLK_F12, SDLK_F13, SDLK_F14, SDLK_F15,
    SDLK_NUMLOCK = 300, SDLK_CAPSLOCK, SDLK_SCROLLOCK, SDLK_RSHIFT, SDLK_LSHIFT, SDLK_RCTRL,
    SDLK_LCTRL, SDLK_RALT, SDLK_LALT, SDLK_RMETA, SDLK_LMETA, SDLK_LSUPER, SDLK_RSUPER,
    SDLK_MODE, SDLK_COMPOSE,
    SDLK_HELP = 315, SDLK_PRINT, SDLK_SYSREQ, SDLK_BREAK, SDLK_MENU, SDLK_POWER, SDLK_EURO,
    SDLK_UNDO,
    SDLK_LAST
} SDLKey;

typedef enum {
    KMOD_NONE = 0x0000, KMOD_LSHIFT = 0x0001, KMOD_RSHIFT = 0x0002,
    KMOD_LCTRL = 0x0040, KMOD_RCTRL = 0x0080, KMOD_LALT = 0x0100, KMOD_RALT = 0x0200,
    KMOD_LMETA = 0x0400, KMOD_RMETA = 0x0800, KMOD_NUM = 0x1000, KMOD_CAPS = 0x2000,
    KMOD_MODE = 0x4000, KMOD_RESERVED = 0x8000
} SDLMod;
#define KMOD_CTRL  (KMOD_LCTRL | KMOD_RCTRL)
#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
#define KMOD_ALT   (KMOD_LALT | KMOD_RALT)
#define KMOD_META  (KMOD_LMETA | KMOD_RMETA)

typedef struct SDL_keysym {
    Uint8  scancode;
    SDLKey sym;
    SDLMod mod;
    Uint16 unicode;
} SDL_keysym;

/* ---- superficies ---- */
/* En la SDL 1.2 los campos son de 16 bits; aquí, int (como en la SDL 2): el
 * código C++ que hace «SDL_Rect r = {x, y, w, h}» con int no compilaría. */
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef struct SDL_Color { Uint8 r, g, b, unused; } SDL_Color;
#define SDL_Colour SDL_Color
typedef struct SDL_Palette { int ncolors; SDL_Color *colors; } SDL_Palette;

typedef struct SDL_PixelFormat {
    SDL_Palette *palette;
    Uint8  BitsPerPixel, BytesPerPixel;
    Uint8  Rloss, Gloss, Bloss, Aloss;
    Uint8  Rshift, Gshift, Bshift, Ashift;
    Uint32 Rmask, Gmask, Bmask, Amask;
    Uint32 colorkey;
    Uint8  alpha;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    Uint16 pitch;
    void *pixels;
    int offset;
    void *hwdata;
    SDL_Rect clip_rect;
    Uint32 unused1;
    Uint32 locked;
    void *map;
    unsigned int format_version;
    int refcount;
} SDL_Surface;

typedef struct SDL_VideoInfo {
    Uint32 hw_available : 1, wm_available : 1, UnusedBits1 : 6, UnusedBits2 : 1,
           blit_hw : 1, blit_hw_CC : 1, blit_hw_A : 1, blit_sw : 1, blit_sw_CC : 1,
           blit_sw_A : 1, blit_fill : 1, UnusedBits3 : 16;
    Uint32 video_mem;
    SDL_PixelFormat *vfmt;
    int current_w, current_h;
} SDL_VideoInfo;

/* ---- eventos ---- */
typedef struct { Uint8 type, gain, state; } SDL_ActiveEvent;
typedef struct { Uint8 type, which, state; SDL_keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint8 type, which, state; Uint16 x, y; Sint16 xrel, yrel; } SDL_MouseMotionEvent;
typedef struct { Uint8 type, which, button, state; Uint16 x, y; } SDL_MouseButtonEvent;
typedef struct { Uint8 type, which, axis; Sint16 value; } SDL_JoyAxisEvent;
typedef struct { Uint8 type, which, hat, value; } SDL_JoyHatEvent;
typedef struct { Uint8 type, which, button, state; } SDL_JoyButtonEvent;
typedef struct { Uint8 type; int w, h; } SDL_ResizeEvent;
typedef struct { Uint8 type; } SDL_ExposeEvent;
typedef struct { Uint8 type; } SDL_QuitEvent;
typedef struct { Uint8 type; int code; void *data1, *data2; } SDL_UserEvent;

typedef union SDL_Event {
    Uint8 type;
    SDL_ActiveEvent active;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_ResizeEvent resize;
    SDL_ExposeEvent expose;
    SDL_QuitEvent quit;
    SDL_UserEvent user;
} SDL_Event;

/* ---- audio ---- */
typedef struct SDL_AudioSpec {
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint16 padding;
    Uint32 size;
    void (*callback)(void *userdata, Uint8 *stream, int len);
    void *userdata;
} SDL_AudioSpec;

typedef struct SDL_AudioCVT {
    int needed;
    Uint16 src_format, dst_format;
    double rate_incr;
    Uint8 *buf;
    int len, len_cvt, len_mult;
    double len_ratio;
} SDL_AudioCVT;

typedef struct _SDL_Joystick SDL_Joystick;

/* ---- funciones: general ---- */
int  SDL_Init(Uint32 flags);
int  SDL_InitSubSystem(Uint32 flags);
void SDL_QuitSubSystem(Uint32 flags);
void SDL_Quit(void);
Uint32 SDL_WasInit(Uint32 flags);
const char *SDL_GetError(void);
void SDL_SetError(const char *fmt, ...);
void SDL_ClearError(void);
char *SDL_getenv(const char *nome);
void *SDL_malloc(size_t n);
void  SDL_free(void *p);

/* ---- vídeo ---- */
SDL_Rect **SDL_ListModes(SDL_PixelFormat *formato, Uint32 flags);
int  SDL_VideoModeOK(int largura, int altura, int bits, Uint32 flags);
const SDL_VideoInfo *SDL_GetVideoInfo(void);
SDL_Surface *SDL_SetVideoMode(int largura, int altura, int bits, Uint32 flags);
SDL_Surface *SDL_GetVideoSurface(void);
int  SDL_Flip(SDL_Surface *superficie);
void SDL_UpdateRect(SDL_Surface *superficie, Sint32 x, Sint32 y, Uint32 l, Uint32 a);
void SDL_UpdateRects(SDL_Surface *superficie, int n, SDL_Rect *rects);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int largura, int altura, int bits,
                                  Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask);
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int largura, int altura, int bits, int pitch,
                                      Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask);
void SDL_FreeSurface(SDL_Surface *superficie);
int  SDL_LockSurface(SDL_Surface *superficie);
void SDL_UnlockSurface(SDL_Surface *superficie);
SDL_bool SDL_SetClipRect(SDL_Surface *superficie, const SDL_Rect *rect);
void SDL_GetClipRect(SDL_Surface *superficie, SDL_Rect *rect);
int  SDL_SetColorKey(SDL_Surface *superficie, Uint32 flag, Uint32 chave);
int  SDL_SetAlpha(SDL_Surface *superficie, Uint32 flag, Uint8 alfa);
int  SDL_FillRect(SDL_Surface *destino, SDL_Rect *rect, Uint32 cor);
int  SDL_UpperBlit(SDL_Surface *origem, SDL_Rect *rect_origem, SDL_Surface *destino, SDL_Rect *rect_destino);
#define SDL_BlitSurface SDL_UpperBlit
int  SDL_SoftStretch(SDL_Surface *origem, SDL_Rect *rect_origem, SDL_Surface *destino, SDL_Rect *rect_destino);
SDL_Surface *SDL_DisplayFormat(SDL_Surface *superficie);
Uint32 SDL_MapRGB(const SDL_PixelFormat *formato, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat *formato, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat *formato, Uint8 *r, Uint8 *g, Uint8 *b);
int  SDL_SetColors(SDL_Surface *superficie, SDL_Color *cores, int primeiro, int n);
int  SDL_SetPalette(SDL_Surface *superficie, int flags, SDL_Color *cores, int primeiro, int n);
int  SDL_SaveBMP(SDL_Surface *superficie, const char *ficheiro);
void SDL_WM_SetCaption(const char *titulo, const char *icone);
int  SDL_WM_ToggleFullScreen(SDL_Surface *superficie);
int  SDL_WM_GrabInput(int modo);
void SDL_WarpMouse(Uint16 x, Uint16 y);
int  SDL_ShowCursor(int estado);

/* ---- eventos y teclado ---- */
int  SDL_PollEvent(SDL_Event *evento);
int  SDL_WaitEvent(SDL_Event *evento);
int  SDL_PushEvent(SDL_Event *evento);
void SDL_PumpEvents(void);
Uint8 SDL_EventState(Uint8 tipo, int estado);
int  SDL_EnableKeyRepeat(int atraso, int intervalo);
int  SDL_EnableUNICODE(int ativar);
SDLMod SDL_GetModState(void);
Uint8 *SDL_GetKeyState(int *n);
const char *SDL_GetKeyName(SDLKey tecla);
Uint8 SDL_GetMouseState(int *x, int *y);

/* ---- tiempo ---- */
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

/* ---- audio ---- */
int  SDL_OpenAudio(SDL_AudioSpec *pedido, SDL_AudioSpec *obtido);
void SDL_CloseAudio(void);
void SDL_PauseAudio(int pausa);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_MixAudio(Uint8 *destino, const Uint8 *origem, Uint32 n, int volume);
int  SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 formato_origem, Uint8 canais_origem, int freq_origem,
                       Uint16 formato_destino, Uint8 canais_destino, int freq_destino);
int  SDL_ConvertAudio(SDL_AudioCVT *cvt);

/* ---- joystick (D307): los mandos del sistema (USB HID, Xbox 360, puerto de juegos) ---- */
int  SDL_NumJoysticks(void);
SDL_Joystick *SDL_JoystickOpen(int indice);
void SDL_JoystickClose(SDL_Joystick *joy);
const char *SDL_JoystickName(int indice);
int  SDL_JoystickNumAxes(SDL_Joystick *joy);
int  SDL_JoystickNumButtons(SDL_Joystick *joy);
int  SDL_JoystickNumHats(SDL_Joystick *joy);
Sint16 SDL_JoystickGetAxis(SDL_Joystick *joy, int eixo);
Uint8 SDL_JoystickGetButton(SDL_Joystick *joy, int botao);
Uint8 SDL_JoystickGetHat(SDL_Joystick *joy, int chapeu);
void SDL_JoystickUpdate(void);
int  SDL_JoystickEventState(int estado);
int  SDL_JoystickOpened(int indice);
int  SDL_JoystickIndex(SDL_Joystick *joy);
int  SDL_JoystickNumBalls(SDL_Joystick *joy);
int  SDL_JoystickGetBall(SDL_Joystick *joy, int bola, int *dx, int *dy);

/* ---- D294 (DOSBox): lo que pide y el SaguiOS no tiene o no hace falta ---- */
typedef enum { SDL_ADDEVENT, SDL_PEEKEVENT, SDL_GETEVENT } SDL_eventaction;
#define SDL_EVENTMASK(X)        (1u << (X))
#define SDL_ACTIVEEVENTMASK     SDL_EVENTMASK(SDL_ACTIVEEVENT)
#define SDL_KEYDOWNMASK         SDL_EVENTMASK(SDL_KEYDOWN)
#define SDL_KEYUPMASK           SDL_EVENTMASK(SDL_KEYUP)
#define SDL_MOUSEMOTIONMASK     SDL_EVENTMASK(SDL_MOUSEMOTION)
#define SDL_QUITMASK            SDL_EVENTMASK(SDL_QUIT)
#define SDL_ALLEVENTS           0xFFFFFFFFu
int  SDL_PeepEvents(SDL_Event *eventos, int n, SDL_eventaction accion, Uint32 mascara);
void SDL_WM_SetIcon(SDL_Surface *icone, Uint8 *mascara);
char *SDL_VideoDriverName(char *destino, int tamanho);
int  SDL_putenv(const char *texto);

/* Superposición YUV: no hay; SDL_CreateYUVOverlay da NULL y el programa usa otra salida. */
#define SDL_YV12_OVERLAY 0x32315659
#define SDL_YUY2_OVERLAY 0x32595559
#define SDL_UYVY_OVERLAY 0x59565955
#define SDL_YVYU_OVERLAY 0x55595659
typedef struct SDL_cond SDL_cond;
typedef struct SDL_Overlay {
    Uint32 format;
    int w, h, planes;
    Uint16 *pitches;
    Uint8 **pixels;
    Uint32 hw_overlay : 1;
} SDL_Overlay;
SDL_Overlay *SDL_CreateYUVOverlay(int l, int a, Uint32 formato, SDL_Surface *tela);
int  SDL_LockYUVOverlay(SDL_Overlay *o);
void SDL_UnlockYUVOverlay(SDL_Overlay *o);
int  SDL_DisplayYUVOverlay(SDL_Overlay *o, SDL_Rect *destino);
void SDL_FreeYUVOverlay(SDL_Overlay *o);

/* Los CD de audio de la SDL: no hay unidad que la SDL maneje (los CD se montan por imagen). */
typedef enum { CD_TRAYEMPTY, CD_STOPPED, CD_PLAYING, CD_PAUSED, CD_ERROR = -1 } CDstatus;
#define CD_INDRIVE(status) ((int)(status) > 0)
#define SDL_MAX_TRACKS 99
#define SDL_AUDIO_TRACK 0x00
#define SDL_DATA_TRACK  0x04
#define CD_FPS 75
#define FRAMES_TO_MSF(f, M, S, F) \
    { int value = (f); *(F) = value % CD_FPS; value /= CD_FPS; *(S) = value % 60; value /= 60; *(M) = value; }
#define MSF_TO_FRAMES(M, S, F) ((M) * 60 * CD_FPS + (S) * CD_FPS + (F))
typedef struct SDL_CDtrack { Uint8 id, type; Uint16 unused; Uint32 length, offset; } SDL_CDtrack;
typedef struct SDL_CD {
    int id;
    CDstatus status;
    int numtracks, cur_track, cur_frame;
    SDL_CDtrack track[SDL_MAX_TRACKS + 1];
} SDL_CD;
int  SDL_CDNumDrives(void);
const char *SDL_CDName(int unidade);
SDL_CD *SDL_CDOpen(int unidade);
CDstatus SDL_CDStatus(SDL_CD *cd);
int  SDL_CDPlay(SDL_CD *cd, int inicio, int quadros);
int  SDL_CDPause(SDL_CD *cd);
int  SDL_CDResume(SDL_CD *cd);
int  SDL_CDStop(SDL_CD *cd);
int  SDL_CDEject(SDL_CD *cd);
void SDL_CDClose(SDL_CD *cd);

/* Los cerrojos: el SaguiOS usa la SDL en un solo hilo (el sonido se mezcla en SDL_PollEvent,
 * SDL_Delay y SDL_Flip), así que no hay nada que cerrar; solo la interfaz (SDL_thread.h). */
typedef struct SDL_mutex SDL_mutex;
SDL_mutex *SDL_CreateMutex(void);
void SDL_DestroyMutex(SDL_mutex *m);
int  SDL_mutexP(SDL_mutex *m);
int  SDL_mutexV(SDL_mutex *m);
#define SDL_LockMutex(m)   SDL_mutexP(m)
#define SDL_UnlockMutex(m) SDL_mutexV(m)

/* ---- lo que es solo del SaguiOS ----
 * El programa los ajusta antes de abrir el vídeo (ver sdl_sagui.c). */
extern const char *sagui_sdl_nome;      /* prefijo del registro: "OPENJAZZ" */
extern const char *sagui_sdl_titulo;    /* título de la ventana al crearla (NULL = el nombre) */
extern int  sagui_sdl_escala;           /* la ventana es la superficie por este factor (1 = tal cual) */
extern int  sagui_cheia_inicial;        /* 1: la primera ventana nace a pantalla completa */
extern void (*sagui_sdl_ao_mudar_cheia)(int cheia);   /* aviso al programa (puede ser NULL) */
/* 1: cuando el sistema suelta el ratón (Ctrl+Alt) se avisa al programa con un SDL_ACTIVEEVENT de foco
 * perdido (D294: DOSBox lo necesita para soltar su captura). */
extern int  sagui_sdl_foco_ao_soltar;
int  sagui_sdl_em_cheia(void);          /* 1 si la ventana está a pantalla completa */

#ifdef __cplusplus
}
#endif

#endif
