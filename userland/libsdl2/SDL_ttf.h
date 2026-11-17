/* SDL_ttf.h - SDL2_ttf mínima del SaguiOS (D374), sobre la libfontes (TrueType/OpenType, antialias).
 * TTF_OpenFont, medir y dibujar texto UTF-8 (Solid, Shaded, Blended; todos dan ARGB8888 con
 * cobertura como alfa), estilos negrita (sintética) y cursiva (oblicua). Implementación:
 * sdl2_ttf.c; los programas enlazan también FONTES_OBJ. */
#ifndef SDL_TTF_H_SAGUI
#define SDL_TTF_H_SAGUI
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TTF_STYLE_NORMAL        0
#define TTF_STYLE_BOLD          1
#define TTF_STYLE_ITALIC        2
#define TTF_STYLE_UNDERLINE     4
#define TTF_STYLE_STRIKETHROUGH 8
typedef struct _TTF_Font TTF_Font;
int  TTF_Init(void);
void TTF_Quit(void);
int  TTF_WasInit(void);
TTF_Font *TTF_OpenFont(const char *ruta, int ptsize);
void TTF_CloseFont(TTF_Font *f);
void TTF_SetFontStyle(TTF_Font *f, int estilo);
int  TTF_GetFontStyle(const TTF_Font *f);
int  TTF_FontHeight(const TTF_Font *f);
int  TTF_FontAscent(const TTF_Font *f);
int  TTF_FontDescent(const TTF_Font *f);
int  TTF_FontLineSkip(const TTF_Font *f);
int  TTF_SizeUTF8(TTF_Font *f, const char *texto, int *w, int *h);
int  TTF_SizeText(TTF_Font *f, const char *texto, int *w, int *h);
SDL_Surface *TTF_RenderUTF8_Solid(TTF_Font *f, const char *texto, SDL_Color fg);
SDL_Surface *TTF_RenderUTF8_Shaded(TTF_Font *f, const char *texto, SDL_Color fg, SDL_Color bg);
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *f, const char *texto, SDL_Color fg);
SDL_Surface *TTF_RenderText_Solid(TTF_Font *f, const char *texto, SDL_Color fg);
SDL_Surface *TTF_RenderText_Shaded(TTF_Font *f, const char *texto, SDL_Color fg, SDL_Color bg);
SDL_Surface *TTF_RenderText_Blended(TTF_Font *f, const char *texto, SDL_Color fg);
#define TTF_GetError SDL_GetError
#define TTF_SetError SDL_SetError
#ifdef __cplusplus
}
#endif
#endif
