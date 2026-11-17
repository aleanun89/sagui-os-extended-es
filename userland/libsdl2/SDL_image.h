/* SDL_image.h - SDL2_image mínima del SaguiOS (D374): PNG con transparencia, JPEG, BMP y GIF
 * (los tres últimos con los decodificadores de la libsagui). IMG_Load devuelve una superficie
 * ARGB8888. Implementación: sdl2_image.c. Sin SDL_RWops (IMG_Load_RW no existe aún). */
#ifndef SDL_IMAGE_H_SAGUI
#define SDL_IMAGE_H_SAGUI
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { IMG_INIT_JPG = 1, IMG_INIT_PNG = 2, IMG_INIT_TIF = 4, IMG_INIT_WEBP = 8 } IMG_InitFlags;
int  IMG_Init(int flags);                 /* devuelve las que se pidieron y tenemos (JPG y PNG) */
void IMG_Quit(void);
SDL_Surface *IMG_Load(const char *ruta);
SDL_Surface *IMG_Load_Mem(const void *datos, int n);   /* extensión nuestra: desde memoria */
#define IMG_GetError SDL_GetError
#define IMG_SetError SDL_SetError
#ifdef __cplusplus
}
#endif
#endif
