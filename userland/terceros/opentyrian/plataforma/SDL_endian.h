/* SDL_endian.h - o parte do endian da SDL 1.2 que o jogo usa (D156).
 *
 * O processador e pequeno: so as trocas LE nao fazem nada. */
#ifndef SAGUI_SDL_ENDIAN_H
#define SAGUI_SDL_ENDIAN_H

#include "SDL_types.h"

/* A SDL de verdade (SDL_stdinc.h) traz estas cabeceras, e o motor
 * depende delas: usa memcpy, strcmp e strlen sem as incluir. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN

/* D187: en la SDL de verdad, SDL_Swap* intercambia SIEMPRE los bytes y solo
 * SDL_SwapLE* (en un procesador pequeno) no hace nada. Antes todas eran la
 * identidad, y el motor, que lee de los datos del Tyrian (guardados en orden
 * grande) la tabla de formas del fondo con SDL_Swap16, la dejaba mal: los
 * mosaicos del terreno no se encontraban y el fondo salia negro. */
#define SDL_Swap16(X) ((Uint16)((((Uint16)(X)) << 8) | (((Uint16)(X)) >> 8)))
#define SDL_Swap32(X) ((Uint32)((((Uint32)(X)) << 24) | ((((Uint32)(X)) & 0xFF00u) << 8) | \
                                ((((Uint32)(X)) >> 8) & 0xFF00u) | (((Uint32)(X)) >> 24)))
#define SDL_Swap64(X) ((Uint64)(((Uint64)SDL_Swap32((Uint32)(X)) << 32) | \
                                (Uint64)SDL_Swap32((Uint32)(((Uint64)(X)) >> 32))))
#define SDL_SwapLE16(X) (X)
#define SDL_SwapLE32(X) (X)
#define SDL_SwapLE64(X) (X)
#define SDL_SwapBE16(X) SDL_Swap16(X)
#define SDL_SwapBE32(X) SDL_Swap32(X)
#define SDL_SwapBE64(X) SDL_Swap64(X)

#endif
