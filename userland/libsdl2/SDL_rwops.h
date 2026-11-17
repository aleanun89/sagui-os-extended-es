/* SDL_rwops.h - SDL_RWops mínimo del SaguiOS (D374): leer y escribir ficheros y memoria con la API de la
 * SDL 2 (SDL_RWFromFile, SDL_RWFromMem, SDL_RWread...). Implementación: sdl2_rwops.c. Lo usan las capas
 * SDL2_image y SDL2_mixer (IMG_Load_RW, Mix_LoadWAV_RW...). */
#ifndef SDL_RWOPS_H_SAGUI
#define SDL_RWOPS_H_SAGUI
#include <stddef.h>
#include "SDL.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RW_SEEK_SET 0
#define RW_SEEK_CUR 1
#define RW_SEEK_END 2
typedef struct SDL_RWops {
    Sint64 (*size)(struct SDL_RWops *ctx);
    Sint64 (*seek)(struct SDL_RWops *ctx, Sint64 offset, int whence);
    size_t (*read)(struct SDL_RWops *ctx, void *ptr, size_t size, size_t maxnum);
    size_t (*write)(struct SDL_RWops *ctx, const void *ptr, size_t size, size_t num);
    int (*close)(struct SDL_RWops *ctx);
    Uint32 type;
    void *hidden1, *hidden2, *hidden3;
} SDL_RWops;
SDL_RWops *SDL_RWFromFile(const char *file, const char *mode);
SDL_RWops *SDL_RWFromMem(void *mem, int size);
SDL_RWops *SDL_RWFromConstMem(const void *mem, int size);
SDL_RWops *SDL_AllocRW(void);
void SDL_FreeRW(SDL_RWops *area);
Sint64 SDL_RWsize(SDL_RWops *ctx);
Sint64 SDL_RWseek(SDL_RWops *ctx, Sint64 offset, int whence);
Sint64 SDL_RWtell(SDL_RWops *ctx);
size_t SDL_RWread(SDL_RWops *ctx, void *ptr, size_t size, size_t maxnum);
size_t SDL_RWwrite(SDL_RWops *ctx, const void *ptr, size_t size, size_t num);
int SDL_RWclose(SDL_RWops *ctx);
/* Lee el resto del flujo a un búfer de malloc (extensión nuestra); *n = bytes. NULL si falla. */
void *SDL_LoadFile_RW(SDL_RWops *src, size_t *n, int cerrar);
#ifdef __cplusplus
}
#endif
#endif
