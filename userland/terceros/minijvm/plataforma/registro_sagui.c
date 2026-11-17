// SPDX-License-Identifier: GPL-3.0-only
/* registro_sagui.c - los mensajes de la VM (D376): jvm_printf escribe en la salida estándar, pero
 * los avisos «[INFO]» (diseño de objetos, montón, recorte) no se muestran salvo con JAVA_INFO=1. */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int jvm_printf(const char *formato, ...)
{
    static int info = -1;
    if (info < 0) info = getenv("JAVA_INFO") != NULL;
    if (!info && !strncmp(formato, "[INFO]", 6)) return 0;
    va_list v;
    va_start(v, formato);
    int r = vprintf(formato, v);
    va_end(v);
    return r;
}
