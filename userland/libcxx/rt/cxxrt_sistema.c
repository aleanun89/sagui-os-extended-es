/* cxxrt_sistema.c - lo que el runtime de C++ pide al SaguiOS (D228).
 *
 * Solo el aviso de una línea que se da antes de parar un programa (una
 * excepción sin capturar, new sin memoria...): va al registro del sistema y a
 * la salida de errores. La prueba en el PC (tests/cxxrt/) trae el suyo.
 */
#include "sagui.h"

#include <stdio.h>

void cxxrt_aviso(const char *texto)
{
    registrar("[CXXRT] %s", texto);
    fprintf(stderr, "%s\n", texto);
    fflush(stderr);
}
