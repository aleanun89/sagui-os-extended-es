/* sys/types.h mínimo para el MicroPython de SaguiOS (lo pide ulab, D479). */
#ifndef SAGUI_MP_SYS_TYPES_H
#define SAGUI_MP_SYS_TYPES_H
#include <stddef.h>
#include <stdint.h>
#ifndef _SSIZE_T_DEFINED_MP
#define _SSIZE_T_DEFINED_MP
typedef int ssize_t;
#endif
#endif
