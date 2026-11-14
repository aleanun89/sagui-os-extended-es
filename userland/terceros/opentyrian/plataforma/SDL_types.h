/* SDL_types.h - los tipos enteros que el juego espera de la SDL.
 *
 * Esta es una implementacion propia (D156): la SDL de verdad no existe en el
 * SaguiOS. Los nombres y los anchos coinciden con los de SDL 1.2 para que el
 * motor de OpenTyrian compile sin cambios. Ver ORIGEM.md.
 */
#ifndef SAGUI_SDL_TYPES_H
#define SAGUI_SDL_TYPES_H

#include <stddef.h>   /* size_t, que o motor usa sem incluir a libc */

typedef unsigned char  Uint8;
typedef signed char    Sint8;
typedef unsigned short Uint16;
typedef signed short   Sint16;
typedef unsigned int   Uint32;
typedef signed int     Sint32;
typedef unsigned long long Uint64;
typedef signed long long   Sint64;

#endif
