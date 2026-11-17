/* consola_latin1.h - se incluye (-include) solo al compilar jni_std.c (D376): la consola de Java es
 * UTF-8 y el terminal de SaguiOS es Latin-1, así que System.out convierte (como TinyGo, D364).
 * System.in no se convierte: Java decodifica la entrada estándar como ISO-8859-1 por defecto, justo lo que
 * manda el terminal. Implementado en consola_sagui.c. */
#ifndef SAGUI_CONSOLA_LATIN1_H
#define SAGUI_CONSOLA_LATIN1_H
#include <stdio.h>
void sagui_saida_utf8(unsigned char c);
#define fprintf(f, fmt, c) sagui_saida_utf8(c)
#endif
