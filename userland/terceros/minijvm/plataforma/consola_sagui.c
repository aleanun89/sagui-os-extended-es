// SPDX-License-Identifier: GPL-3.0-only
/* consola_sagui.c - consola de Java en SaguiOS (D376): UTF-8 de Java -> Latin-1 del terminal.
 * jni_std.c se compila con consola_latin1.h, que redirige System.out a esta función. */
#include <stdio.h>

void sagui_saida_utf8(unsigned char c)
{
    static unsigned pendiente;   /* 0 = nada; 1 = secuencia larga que se descarta; otro = byte inicial de 2 bytes */
    if (c < 0x80) { pendiente = 0; putchar(c); if (c == '\n') fflush(stdout); return; }   /* línea a línea: es interactivo */
    if ((c & 0xE0) == 0xC0) { pendiente = c; return; }
    if ((c & 0xC0) == 0x80) {
        if (pendiente > 1) {
            unsigned cp = ((pendiente & 0x1F) << 6) | (c & 0x3F);
            putchar(cp < 256 ? (int)cp : '?');
            pendiente = 0;
        }
        return;
    }
    putchar('?');   /* 3 o 4 bytes: fuera de Latin-1, un solo '?' */
    pendiente = 1;
}
